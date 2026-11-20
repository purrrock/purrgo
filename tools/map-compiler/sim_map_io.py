#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
sim_map_io.py - оценка нагрузки на SD-карту при отрисовке одного экрана карты.

Повторяет обход src/core/map/map.c + map_idx.c + map_mlp.c на готовых файлах
карты (roads/landuse/pois .idx/.mlp/.db) и считает:
  - сколько секторов (512 Б) прошивка читает с SD (кэш-промахи LRU на 32 сектора);
  - сколько из них подряд идущие, а сколько случайные;
  - сколько вершин геометрии проецируется (нагрузка на CPU);
  - сколько дорогих f_lseek "от начала файла" (FatFs без CLMT).
По этим числам по простой модели считается время.

Формат карты (V3 или V4) определяется автоматически по сигнатуре SQT в roads.idx.
V3: обход в глубину с прыжками v3_jump (как сейчас в прошивке).
V4: обход по блокам 512 Б по child_block (docs/map-format-v4.md, раздел 7.2),
    четыре LOD; LOD 3 выбирается по масштабу (см. target_lod).

Использование:
    python sim_map_io.py <папка_карты> <lat> <lon> [--scales 500,2000,...]
                         [--poi-mode no|icons] [--labels 0|1]
                         [--bus spi] [--spi-mhz MHZ] [--fastseek] [--cache N] [--line L]
                         [--min-px P] [--min-px-poly P] [--pan FRAC]
                         [--max-lod N] [--compare <папка_карты_2>]
--compare считает те же кадры на второй карте (обычно V4 против V3) и печатает
таблицу сравнения. Это ОЦЕНКА по модели, а не замер на железе. Параметры
модели - в блоке MODEL.
"""

import argparse
import math
import mmap
import os
import struct
import sys

# ---------------------------------------------------------------------------
# Константы прошивки
# ---------------------------------------------------------------------------
VP_W, VP_H = 176, 246                 # viewport карты (264 - 18)
SECTOR = 512
LRU_ENTRIES = 32                      # fs_hal_stm32.c
PER_METER_1E7 = 90                    # PURRGO_1E7_PER_METER (≈ 89.93)
MAX_PARTS, MAX_POINTS = 32, 2000
MAX_DEPTH = 32
# Feature codes, у которых в прошивке есть стиль линии/полигона (src/core/map/map_style.c).
# Диапазоны: 0..31 roads, 32..63 waterways, 64..95 water, 96..127 landuse, 128..191 POI.
STYLED_CODES = frozenset(
    list(range(1, 8)) +      # ROAD_*, RAILWAY
    [32, 33] +               # WATERWAY_MAJOR/MINOR
    [64, 65, 66] +           # WATER, WETLAND, GLACIER
    [96, 97, 98])            # LANDUSE_NATURAL/HUMAN, SAND
NAME_OFF = 32 + 129 + 17              # смещение имени в .db
REC = 117

V4_BLOCK = 512                        # блок индекса V4 = один сектор SD
V4_MAGIC = b"SQT\x02"

SCALES = [10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000,
          100000, 200000, 500000, 1000000]

# ---------------------------------------------------------------------------
# MODEL: параметры времени (оценки, не замеры). low / mid / high.
# ---------------------------------------------------------------------------
MODEL = {
    # задержка карты до первого байта (rand - "далёкий" сектор, seq - соседний)
    "lat_rand_ms":  (0.3,  0.6,  1.2),
    "lat_seq_ms":   (0.05, 0.12, 0.3),
    # передача одного сектора 512 Б по шине (без задержки карты)
    #   spi : ~20 МГц (160 МГц / 8), DMA (low) ... HAL polling (high)
    "xfer_spi_ms":  (0.21, 0.25, 0.45),
    # CPU на одну вершину (проекция: 2 деления int64 + отрезок) и на один purrgo_fs_read
    "us_per_vertex_84":  (6.0, 10.0, 16.0),   # ~84 МГц (калибровка по Cortex-M4; близко к текущим 80 МГц U585)
    "us_per_vertex_160": (3.0,  5.0,  8.0),   # Cortex-M33 @ 160 МГц (U585)
    "us_per_fsread_84":  (2.0, 3.0, 5.0),
    "us_per_fsread_160": (1.0, 1.5, 2.5),
    "cluster_kb":   (32, 32, 32),             # кластер FAT32
}
CPU = "84"        # переопределяется --cpu


class Cache:
    """Глобальный LRU из N секторов (в прошивке N = 32, fs_hal_stm32.c)."""

    def __init__(self, entries=LRU_ENTRIES):
        self.entries = entries
        self.d = {}
        self.t = 0

    def hit(self, key):
        if key in self.d:
            self.t += 1
            self.d[key] = self.t
            return True
        return False

    def put(self, key):
        if key in self.d:
            return
        if len(self.d) >= self.entries:
            victim = min(self.d, key=self.d.get)
            del self.d[victim]
        self.t += 1
        self.d[key] = self.t


class Stats:
    def __init__(self):
        self.x_seq = 0               # передач, начатых сразу за предыдущей
        self.x_rand = 0              # передач с "далёким" адресом
        self.sectors = 0             # секторов по шине всего
        self.hits = 0
        self.fsreads = 0
        self.vertices = 0
        self.features = 0
        self.skipped_small = 0
        self.labels = 0
        self.seek_back = 0
        self.seek_clusters = 0
        self.fat_cross = 0
        self.seeks = 0
        self.jumps = 0
        self.depth = {}
        self.pass_nav = 0            # навигационных узлов, вошедших в камеру
        self.by_file = {}            # слой:тип -> передач (roads:idx ...)
        self.by_kind = {}            # тип (idx/mlp/db) -> передач

    def copy_counts(self):
        return dict(self.__dict__)


class SimFile:
    def __init__(self, path, world):
        self.path = path
        self.name = os.path.basename(path)
        # mmap вместо read(): файлы карты Беларуси ~250 МБ, а World создаётся на каждый кадр
        self._fh = open(path, "rb")
        self.size = os.path.getsize(path)
        self.data = (mmap.mmap(self._fh.fileno(), 0, access=mmap.ACCESS_READ)
                     if self.size else b"")
        self.w = world
        self.logical = 0
        self.fatfs_pos = 0
        self.last_end = -10          # конец последней передачи (в секторах)

    def seek(self, off):
        self.logical = off
        return off <= self.size

    def _load(self, s):
        w, st = self.w, self.w.stats
        key = (self.name, s)
        if w.cache.hit(key):
            st.hits += 1
            return
        L = w.line_for(self.name)
        base = (s // L) * L
        nsec = (self.size + SECTOR - 1) // SECTOR
        n = min(L, nsec - base)
        off = base * SECTOR
        if self.fatfs_pos != off:
            cs = w.cluster
            st.seeks += 1
            tgt = off // cs
            cur = self.fatfs_pos // cs
            if off >= self.fatfs_pos:
                steps = max(0, tgt - cur)
            else:
                steps = tgt
                st.seek_back += 1
            st.seek_clusters += steps
        elif off % w.cluster == 0 and off > 0:
            st.fat_cross += 1
        seq = (base == self.last_end) and w.card_last == self.name
        if seq:
            st.x_seq += 1
        else:
            st.x_rand += 1
        st.sectors += n
        layer, kind = self.name.rsplit('.', 1)
        st.by_file[layer + ':' + kind] = st.by_file.get(layer + ':' + kind, 0) + 1
        st.by_kind[kind] = st.by_kind.get(kind, 0) + 1
        self.last_end = base + n
        w.card_last = self.name
        for k in range(base, base + n):
            w.cache.put((self.name, k))
        self.fatfs_pos = min(off + n * SECTOR, self.size)

    def read(self, n):
        self.w.stats.fsreads += 1
        if self.logical >= self.size:
            return b""
        n = min(n, self.size - self.logical)
        first = self.logical // SECTOR
        last = (self.logical + n - 1) // SECTOR
        for s in range(first, last + 1):
            self._load(s)
        out = self.data[self.logical:self.logical + n]
        self.logical += n
        return out


class World:
    """Состояние между кадрами: открытые файлы + кэш (как в прошивке)."""

    def __init__(self, map_dir, cache_entries=LRU_ENTRIES, line=1, cluster=32768):
        self.map_dir = map_dir
        self.cache = Cache(cache_entries)
        # line: int (для всех файлов) или dict {"idx": n, "mlp": n, "db": n}
        self.line = line
        self.cluster = cluster
        self.card_last = None
        self.stats = Stats()
        self.files = {}

    def line_for(self, name):
        if isinstance(self.line, dict):
            return self.line.get(name.rsplit('.', 1)[1], 1)
        return self.line

    def F(self, name):
        if name not in self.files:
            p = os.path.join(self.map_dir, name)
            self.files[name] = SimFile(p, self) if os.path.exists(p) else None
        return self.files[name]

    def reset_stats(self):
        self.stats = Stats()
        for f in self.files.values():
            if f:
                f.last_end = -10
        self.card_last = None


def i32(b, o):
    return struct.unpack_from("<i", b, o)[0]


def u32(b, o):
    return struct.unpack_from("<I", b, o)[0]


def cos10k(lat_1e7):
    return int(math.cos(math.radians(lat_1e7 / 1e7)) * 10000)


def is_v4_file(idx):
    """V4 по сигнатуре SQT\\x02 по смещению LOD 0 (без учёта в статистике ввода-вывода)."""
    d = idx.data
    if len(d) < 36 or d[:3] != b"PGO":
        return False
    lod0 = u32(d, 8)
    return d[lod0:lod0 + 4] == V4_MAGIC


def map_format(map_dir):
    """'V3' / 'V4' по первому найденному .idx; None, если слоёв нет."""
    for layer in ("roads", "landuse", "pois"):
        p = os.path.join(map_dir, layer + ".idx")
        if os.path.exists(p):
            with open(p, "rb") as f:
                head = f.read(32)
                if len(head) < 32 or head[:3] != b"PGO":
                    return None
                f.seek(u32(head, 8))
                return "V4" if f.read(4) == V4_MAGIC else "V3"
    return None


def make_camera(lat_deg, lon_deg, width_m):
    lat = int(lat_deg * 1e7)
    lon = int(lon_deg * 1e7)
    w1e7 = min(width_m * PER_METER_1E7, 3600000000)
    rad_x = w1e7 // 2
    h1e7 = min(w1e7 * VP_H // VP_W, 1800000000)
    rad_y = h1e7 // 2
    c = max(cos10k(lat), 100)
    rad_x = rad_x * 10000 // c
    return dict(min_x=lon - rad_x, max_x=lon + rad_x,
                min_y=lat - rad_y, max_y=lat + rad_y)


def bbox_hit(xmin, ymin, xmax, ymax, cam):
    if ymax < cam["min_y"] or ymin > cam["max_y"]:
        return False
    return xmax >= cam["min_x"] and xmin <= cam["max_x"]


def target_lod(scale_m, details_high=True, lods=3):
    """LOD по масштабу. lods=3: как в прошивке сейчас (0..2).
    lods=4 (V4): LOD 3 по предложению docs/design/pgo-v4.md: при обычной
    детализации с масштаба > 20 км, при низкой > 10 км (пороги уточнить замером)."""
    if details_high:
        lod = 0 if scale_m <= 500 else (1 if scale_m <= 5000 else 2)
        if lods >= 4 and scale_m > 20000:
            lod = 3
    else:
        lod = 0 if scale_m <= 100 else (1 if scale_m <= 2000 else 2)
        if lods >= 4 and scale_m > 10000:
            lod = 3
    return lod


class Renderer:
    def __init__(self, world, params):
        self.w = world
        self.st = world.stats
        self.p = params

    def read_name(self, db, v2, is_poi):
        if db is None or v2 == 0 or (not is_poi and v2 == 1):
            return False
        db.seek(32 + 129 + (v2 - 1) * REC + 17)
        raw = db.read(63)
        self.st.labels += 1
        return len(raw) > 0 and raw[0] != 0

    def render_feature(self, mlp, v1):
        st = self.st
        base = 32 + v1
        mlp.seek(base)
        head = mlp.read(SECTOR - (base % SECTOR))
        if len(head) < 24:
            return
        num_parts = i32(head, 16)
        num_points = i32(head, 20)
        if not (0 <= num_parts <= MAX_PARTS and 0 < num_points <= MAX_POINTS):
            return
        total = 24 + num_parts * 4 + num_points * 8
        got = len(head)
        while got < total:
            r = mlp.read(min(total - got, SECTOR))
            if not r:
                break
            got += len(r)
        st.features += 1
        st.vertices += num_points

    def too_small(self, xmin, ymin, xmax, ymax, cam, layer):
        mp = self.p.get("min_px", 0)
        if layer == "landuse" and self.p.get("min_px_poly") is not None:
            mp = self.p["min_px_poly"]          # полигоны: свой порог (в прошивке 3, у линий 2)
        if not mp:
            return False
        sx = max(1, cam["max_x"] - cam["min_x"])
        sy = max(1, cam["max_y"] - cam["min_y"])
        return max((xmax - xmin) * VP_W / sx, (ymax - ymin) * VP_H / sy) < mp

    def data_node(self, buf, mlp, db, cam, layer):
        """Узел данных (25 Б), общий для V3 и V4."""
        ymin, ymax = i32(buf, 4), i32(buf, 12)
        xmin, xmax = i32(buf, 0), i32(buf, 8)
        if not bbox_hit(xmin, ymin, xmax, ymax, cam):
            return
        obj_type = buf[16]
        v1, v2 = u32(buf, 17), u32(buf, 21)
        if layer == "pois":
            if self.p["poi_mode"] == "no":
                return
            if self.p["labels"] and v2 > 0 and db is not None:
                self.read_name(db, v2, True)
            return
        if obj_type not in STYLED_CODES:
            return
        if v1 > 0:
            if self.too_small(xmin, ymin, xmax, ymax, cam, layer):
                self.st.skipped_small += 1
                return
            self.render_feature(mlp, v1)
            if layer == "landuse" and self.p["labels"] and v2 >= 2 and db is not None:
                self.read_name(db, v2, False)

    def parse_node(self, idx, state, mlp, db, is_nav, cam, layer, lod_end, depth):
        size = 28 if is_nav else 25
        if state["off"] + size > lod_end:
            return False
        buf = idx.read(size)
        if len(buf) != size:
            return False
        state["off"] += size
        if not is_nav:
            self.data_node(buf, mlp, db, cam, layer)
            return True
        jump = u32(buf, 0)
        cymin, cymax = i32(buf, 8), i32(buf, 16)
        level, count = u32(buf, 20), u32(buf, 24)
        cxmin, cxmax = i32(buf, 4), i32(buf, 12)
        if not bbox_hit(cxmin, cymin, cxmax, cymax, cam):
            if jump > 0:
                if state["off"] + jump > lod_end:
                    return False
                idx.seek(state["off"] + jump)
                state["off"] += jump
                self.st.jumps += 1
            return True
        if depth >= MAX_DEPTH:
            return False
        self.st.pass_nav += 1
        for _ in range(count):
            if not self.parse_node(idx, state, mlp, db, level > 0, cam, layer,
                                   lod_end, depth + 1):
                return False
        return True

    def visit_block(self, idx, mlp, db, off, level, count, cam, layer, end, depth):
        """V4: чтение одного блока 512 Б и переход в детей (раздел 7.2).
        level: уровень записей блока, -1 = листовой блок (узлы данных)."""
        if off % V4_BLOCK or off + V4_BLOCK > end or depth >= MAX_DEPTH:
            return False
        idx.seek(off)
        blk = idx.read(V4_BLOCK)
        if len(blk) != V4_BLOCK:
            return False
        if level == -1:
            for k in range(count):
                self.data_node(blk[k * 25:(k + 1) * 25], mlp, db, cam, layer)
            return True
        for k in range(count):
            child, xmin, ymin, xmax, ymax, lv, cnt = struct.unpack_from("<IiiiiII", blk, k * 28)
            if not bbox_hit(xmin, ymin, xmax, ymax, cam):
                continue
            if child <= off:                       # ограничение 3 раздела 7.1
                return False
            self.st.pass_nav += 1
            if not self.visit_block(idx, mlp, db, child, lv - 1, cnt, cam, layer, end,
                                    depth + 1):
                return False
        return True

    def render_layer_v4(self, idx, mlp, db, cam, layer, scale_m):
        idx.seek(0)
        b = idx.read(32)
        lods = [u32(b, 8 + 4 * i) for i in range(4)]
        if mlp is not None:
            mlp.seek(0)
            mlp.read(32)
        lod = target_lod(scale_m, True, 4)
        lod = min(lod, self.p.get("max_lod", 3))
        start = lods[lod]
        end = lods[lod + 1] if lod < 3 else idx.size
        idx.seek(start)
        sq = idx.read(28)
        depth, roots = u32(sq, 8), u32(sq, 12)
        self.st.depth[layer] = depth
        if depth > 0:
            self.visit_block(idx, mlp, db, start + V4_BLOCK, depth - 1, roots, cam, layer,
                             end, 0)
        return lod

    def render_layer(self, idx, mlp, db, cam, layer, scale_m):
        if is_v4_file(idx):
            return self.render_layer_v4(idx, mlp, db, cam, layer, scale_m)

        def header(f):
            f.seek(0)
            b = f.read(32)
            return u32(b, 4), [u32(b, 8), u32(b, 12), u32(b, 16)]
        payload, lods = header(idx)
        if mlp is not None:
            header(mlp)
        lod = min(target_lod(scale_m), self.p.get("max_lod", 3))
        end = lods[1] if lod == 0 else (lods[2] if lod == 1 else 32 + payload)
        idx.seek(lods[lod])
        sq = idx.read(16)
        mode, count = u32(sq, 8), u32(sq, 12)
        self.st.depth[layer] = mode
        state = {"off": lods[lod] + 16}
        for _ in range(count):
            if not self.parse_node(idx, state, mlp, db, mode > 0, cam, layer, end, 0):
                break
        return lod


def run_frame(world, lat, lon, scale_m, params):
    """Один кадр в состоянии world (кэш и позиции файлов сохраняются)."""
    world.reset_stats()
    r = Renderer(world, params)
    cam = make_camera(lat, lon, scale_m)
    lod = None
    for layer in ("landuse", "roads", "pois"):
        idx, mlp = world.F(layer + ".idx"), world.F(layer + ".mlp")
        db = world.F(layer + ".db") if layer in ("landuse", "pois") else None
        if idx is None or (layer != "pois" and mlp is None):
            continue
        lod = r.render_layer(idx, mlp if layer != "pois" else None, db, cam, layer, scale_m)
    return world.stats, lod


def time_ms(st, i, bus="spi", fastseek=False, kscale=1.0, f=1.0, extra_levels=0):
    """Время кадра по счётчикам. f - множитель счётчиков (экстраполяция),
    kscale - во сколько раз файлы больше, чем в образце (для шагов f_lseek)."""
    lat_r = MODEL["lat_rand_ms"][i]
    lat_s = MODEL["lat_seq_ms"][i]
    xfer = MODEL["xfer_%s_ms" % bus][i]
    us_v = MODEL["us_per_vertex_" + CPU][i]
    us_r = MODEL["us_per_fsread_" + CPU][i]
    avg_depth = sum(st.depth.values()) / max(1, len(st.depth))
    extra = st.jumps * f * extra_levels / (avg_depth + 1)
    steps = st.seek_clusters * f * kscale
    seek = steps * 0.0015 + steps / 128.0 * lat_r
    fat = st.fat_cross * f * lat_r * 0.5
    if fastseek:
        seek = fat = 0.0
    io = ((st.x_rand * f + extra) * lat_r + st.x_seq * f * lat_s
          + st.sectors * f * xfer + seek + fat)
    cpu = st.vertices * f * us_v / 1000.0 + st.fsreads * f * us_r / 1000.0
    return io, cpu


def frame(map_dir, a, params, line, sc):
    """Кадр на масштабе sc (с --pan: второй кадр после сдвига). -> (stats, lod, io, cpu)."""
    w = World(map_dir, a.cache, line, MODEL["cluster_kb"][1] * 1024)
    st, lod = run_frame(w, a.lat, a.lon, sc, params)
    if a.pan:
        cam = make_camera(a.lat, a.lon, sc)
        dlon = a.pan * (cam["max_x"] - cam["min_x"]) / 1e7
        st, lod = run_frame(w, a.lat, a.lon + dlon, sc, params)
    res = [time_ms(st, i, a.bus, a.fastseek) for i in range(3)]
    return st, lod, [r[0] for r in res], [r[1] for r in res]


def ratio(x, y):
    return "   -" if not x else f"{y / x:4.2f}"


def compare(a, params, line):
    """Сравнение двух карт (A = map_dir, B = --compare) на одних и тех же кадрах."""
    fa, fb = map_format(a.map_dir), map_format(a.compare)
    print(f"A={a.map_dir} [{fa}]   B={a.compare} [{fb}]   center=({a.lat},{a.lon}) "
          f"cpu={a.cpu}MHz spi_mhz={a.spi_mhz or 'модель'} cache={a.cache} line={line} "
          f"max_lod={a.max_lod} pan={a.pan}")
    print("Время - середина диапазона low/mid/high модели; B/A < 1 означает, что B лучше.")
    print(f"{'scale':>8} {'LOD A/B':>7} | {'xfers A':>7} {'B':>6} {'B/A':>5} | "
          f"{'sect A':>6} {'B':>6} {'B/A':>5} | {'idx xf A':>8} {'B':>6} | "
          f"{'verts A':>7} {'B':>7} {'B/A':>5} | {'total s A':>9} {'B':>6} {'B/A':>5}")
    for sc in [int(x) for x in a.scales.split(",")]:
        sa, la, ioa, cpua = frame(a.map_dir, a, params, line, sc)
        sb, lb, iob, cpub = frame(a.compare, a, params, line, sc)
        xa, xb = sa.x_seq + sa.x_rand, sb.x_seq + sb.x_rand
        ta, tb = (ioa[1] + cpua[1]) / 1000, (iob[1] + cpub[1]) / 1000
        print(f"{sc:>8} {la:>3}/{lb:<3} | {xa:>7} {xb:>6} {ratio(xa, xb):>5} | "
              f"{sa.sectors:>6} {sb.sectors:>6} {ratio(sa.sectors, sb.sectors):>5} | "
              f"{sa.by_kind.get('idx', 0):>8} {sb.by_kind.get('idx', 0):>6} | "
              f"{sa.vertices:>7} {sb.vertices:>7} {ratio(sa.vertices, sb.vertices):>5} | "
              f"{ta:>9.3f} {tb:>6.3f} {ratio(ta, tb):>5}")


def main():
    global CPU
    ap = argparse.ArgumentParser()
    ap.add_argument("map_dir")
    ap.add_argument("lat", type=float)
    ap.add_argument("lon", type=float)
    ap.add_argument("--scales", default=",".join(map(str, SCALES)))
    ap.add_argument("--poi-mode", default="no", choices=["no", "icons"])
    ap.add_argument("--labels", type=int, default=1)
    ap.add_argument("--bus", default="spi", choices=["spi"])
    ap.add_argument("--cpu", default="160", choices=["84", "160"],
                    help="~84 МГц (близко к текущим 80 МГц U585) или 160 МГц")
    ap.add_argument("--spi-mhz", type=float, default=0,
                    help="частота SPI SD (U585 сейчас SPI2 /8 = 10 МГц, после /4 = 20 МГц); "
                         "0 - значения MODEL (~20 МГц)")
    ap.add_argument("--fastseek", action="store_true",
                    help="модель с включённой CLMT (f_lseek(CREATE_LINKMAP))")
    ap.add_argument("--cache", type=int, default=LRU_ENTRIES, help="секторов в LRU")
    ap.add_argument("--line", type=int, default=1, help="секторов за одну передачу (read-ahead), для всех файлов")
    ap.add_argument("--line-idx", type=int, default=0, help="read-ahead только для .idx (перекрывает --line)")
    ap.add_argument("--line-mlp", type=int, default=0, help="read-ahead только для .mlp (перекрывает --line)")
    ap.add_argument("--line-db", type=int, default=0, help="read-ahead только для .db (перекрывает --line)")
    ap.add_argument("--breakdown", action="store_true",
                    help="печатать разбивку передач по idx/mlp/db, пройденные узлы дерева и пропуски")
    ap.add_argument("--min-px", type=float, default=0, help="не рисовать объекты мельче P пикселей")
    ap.add_argument("--min-px-poly", type=float, default=None,
                    help="порог пропуска для полигонов (landuse), по умолчанию как --min-px")
    ap.add_argument("--max-lod", type=int, default=3, choices=range(4),
                    help="не выбирать LOD выше N (например 2: V4-карта выбирает LOD как V3, "
                         "сравнение раскладки индекса без LOD 3)")
    ap.add_argument("--compare", metavar="DIR2", default=None,
                    help="вторая папка карты (обычно V4 против V3): те же кадры, таблица "
                         "сравнения; ключи --max-lod и остальные действуют на обе карты")
    ap.add_argument("--pan", type=float, default=0,
                    help="после кадра сдвинуть камеру на долю ширины и считать второй кадр")
    a = ap.parse_args()
    CPU = a.cpu
    if a.spi_mhz > 0:
        # 512 Б = 4096 бит; плюс ~0.04 мс на команду, токен и CRC
        mid = 4.096 / a.spi_mhz + 0.04
        MODEL["xfer_spi_ms"] = (mid * 0.85, mid, mid * 1.8)
    params = {"poi_mode": a.poi_mode, "labels": bool(a.labels), "min_px": a.min_px,
              "min_px_poly": a.min_px_poly, "max_lod": a.max_lod}
    line = a.line
    if a.line_idx or a.line_mlp or a.line_db:
        line = {"idx": a.line_idx or a.line, "mlp": a.line_mlp or a.line, "db": a.line_db or a.line}

    if a.compare:
        compare(a, params, line)
        return
    print(f"map={a.map_dir} [{map_format(a.map_dir)}] center=({a.lat},{a.lon}) bus={a.bus} cpu={a.cpu}MHz "
          f"spi_mhz={a.spi_mhz or 'модель'} fastseek={a.fastseek} cache={a.cache} line={line} min_px={a.min_px} pan={a.pan}")
    print(f"{'scale':>8} {'LOD':>3} {'xfers':>6} {'sect':>5} {'KB':>6} {'verts':>7} "
          f"{'skip':>5} {'bk-seek':>7} | {'I/O ms lo/mid/hi':>20} | {'CPU ms lo/mid/hi':>20} | "
          f"{'total s lo/mid/hi':>20}")
    for sc in [int(x) for x in a.scales.split(",")]:
        w = World(a.map_dir, a.cache, line, MODEL["cluster_kb"][1] * 1024)
        st, lod = run_frame(w, a.lat, a.lon, sc, params)
        if a.pan:
            dlon = a.pan * (make_camera(a.lat, a.lon, sc)["max_x"] -
                            make_camera(a.lat, a.lon, sc)["min_x"]) / 1e7
            st, lod = run_frame(w, a.lat, a.lon + dlon, sc, params)
        res = [time_ms(st, i, a.bus, a.fastseek) for i in range(3)]
        io = [r[0] for r in res]
        cpu = [r[1] for r in res]
        tot = [(r[0] + r[1]) / 1000 for r in res]
        print(f"{sc:>8} {lod:>3} {st.x_seq + st.x_rand:>6} {st.sectors:>5} {st.sectors * 0.5:>6.0f} "
              f"{st.vertices:>7} {st.skipped_small:>5} {st.seek_back:>7} | "
              f"{io[0]:>6.0f}/{io[1]:>6.0f}/{io[2]:>6.0f} | "
              f"{cpu[0]:>6.0f}/{cpu[1]:>6.0f}/{cpu[2]:>6.0f} | "
              f"{tot[0]:>6.2f}/{tot[1]:>6.2f}/{tot[2]:>6.2f}")
        if a.breakdown:
            k = st.by_kind
            print(f"{'':>8}   idx={k.get('idx', 0)} mlp={k.get('mlp', 0)} db={k.get('db', 0)} "
                  f"nav_passed={st.pass_nav} jumps={st.jumps} features={st.features} "
                  f"hits={st.hits} fsreads={st.fsreads}")


if __name__ == "__main__":
    main()
