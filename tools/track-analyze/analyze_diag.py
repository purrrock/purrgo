#!/usr/bin/env python3
"""
Анализ GPX, записанного диагностической сборкой (PURRGO_TRACK_DIAG=1): сверка порогов
фильтров track_logger.c по реальным данным. См. docs/track-field-test.md.

    python3 tools/track-analyze/analyze_diag.py track1.gpx [track2.gpx ...]
    python3 tools/track-analyze/analyze_diag.py track.gpx --from 2026-11-12T08:30 --to 2026-11-12T09:00

--from/--to (UTC, ISO) оставляют один сценарий теста; без них анализируется весь файл.

Читает на каждую точку: fix, sat, hdop, pg:spd (сотые доли узла), pg:rej (маска фильтров).
"""
import math
import re
import sys
from collections import Counter
from datetime import datetime, timezone

REJ = [(0x01, "fix_type < 3"), (0x02, "HDOP > 5.0"), (0x04, "спутников < 4"),
       (0x08, "стоянка (< 0.5 уз)"), (0x10, "выброс (> 500 м/с)"), (0x20, "прореживание")]


def load(path):
    pts = []
    text = open(path, encoding="utf-8").read()
    for seg in text.split("<trkpt")[1:]:
        m = re.match(r'\s+lat="([-\d.]+)"\s+lon="([-\d.]+)"', seg)
        t = re.search(r"<time>([^<]+)</time>", seg)
        if not m or not t:
            continue

        def tag(name, cast=float):
            r = re.search(r"<%s>([^<]+)</%s>" % (name, name), seg)
            return cast(r.group(1)) if r else None

        pts.append(dict(
            lat=float(m.group(1)), lon=float(m.group(2)),
            t=datetime.fromisoformat(t.group(1).replace("Z", "+00:00")).timestamp(),
            fix=tag("fix", str), sat=tag("sat", int), hdop=tag("hdop"),
            spd=tag("pg:spd", int), rej=tag("pg:rej", int)))
    return pts


def dist(a, b):
    dy = (b["lat"] - a["lat"]) * 111195.0
    dx = (b["lon"] - a["lon"]) * 111195.0 * math.cos(math.radians(a["lat"]))
    return math.hypot(dx, dy)


def pct(values, q):
    v = sorted(values)
    return v[min(len(v) - 1, int(len(v) * q))] if v else float("nan")


def report(pts):
    n = len(pts)
    if n < 2 or pts[0]["rej"] is None:
        print("Нет pg:rej: файл записан не диагностической сборкой.")
        return
    hours = (pts[-1]["t"] - pts[0]["t"]) / 3600
    print(f"Точек {n}, длительность {hours:.2f} ч")

    print("\n1. Что отбросили бы фильтры (точка может попасть в несколько строк)")
    kept = sum(1 for p in pts if p["rej"] == 0)
    print(f"   записано обычной сборкой: {kept} ({100 * kept / n:.1f} %)")
    for bit, name in REJ:
        c = sum(1 for p in pts if p["rej"] & bit)
        print(f"   {name:24s} {c:7d} ({100 * c / n:5.1f} %)")

    print("\n2. Качество приёма (все эпохи)")
    print("   fix:", dict(Counter(p["fix"] for p in pts)))
    for key, label in (("sat", "спутников в решении"), ("hdop", "HDOP")):
        vals = [p[key] for p in pts if p[key] is not None]
        if vals:
            print(f"   {label}: медиана {pct(vals, .5)}, p90 {pct(vals, .9)}, p99 {pct(vals, .99)}, max {max(vals)}")
    for thr in (2.0, 3.0, 5.0, 8.0):
        c = sum(1 for p in pts if p["hdop"] is not None and p["hdop"] > thr)
        print(f"   HDOP > {thr}: {100 * c / n:.2f} % эпох")
    for thr in (3, 4, 5, 6):
        c = sum(1 for p in pts if p["sat"] is not None and p["sat"] < thr)
        print(f"   спутников < {thr}: {100 * c / n:.2f} % эпох")

    print("\n3. Влияние HDOP на дрейф: скачок позиции за 1 с (эпохи подряд, скорость < 1 уз)")
    by = {}
    for a, b in zip(pts, pts[1:]):
        if b["t"] - a["t"] != 1 or a["hdop"] is None or (a["spd"] or 0) >= 100 or (b["spd"] or 0) >= 100:
            continue
        key = "<1.0" if a["hdop"] < 1 else "1-2" if a["hdop"] < 2 else "2-5" if a["hdop"] <= 5 else ">5"
        by.setdefault(key, []).append(dist(a, b))
    for key in ("<1.0", "1-2", "2-5", ">5"):
        if key in by:
            v = by[key]
            print(f"   HDOP {key:5s}: эпох {len(v):6d}, шаг медиана {pct(v, .5):.1f} м, p95 {pct(v, .95):.1f} м, max {max(v):.1f} м")

    print("\n4. Порог стоянки: шаг позиции за 1 с в зависимости от скорости по приёмнику")
    bins = [(0, 10), (10, 25), (25, 50), (50, 100), (100, 200), (200, 500)]
    for lo, hi in bins:
        v = [dist(a, b) for a, b in zip(pts, pts[1:])
             if b["t"] - a["t"] == 1 and b["spd"] is not None and lo <= b["spd"] < hi]
        if v:
            print(f"   {lo / 100:.2f}-{hi / 100:.2f} уз: эпох {len(v):6d}, шаг медиана {pct(v, .5):.2f} м, p95 {pct(v, .95):.2f} м")
    print("   (порог: ниже него шаг должен быть шумом, т. е. сравним с шагом при ~0 уз)")

    print("\n5. Выбросы: самые быстрые скачки между соседними эпохами")
    jumps = sorted(((dist(a, b) / max(b["t"] - a["t"], 1), i) for i, (a, b) in enumerate(zip(pts, pts[1:]))), reverse=True)[:5]
    for v, i in jumps:
        print(f"   точка {i}: {v:.1f} м/с, hdop {pts[i + 1]['hdop']}, sat {pts[i + 1]['sat']}, rej {pts[i + 1]['rej']}")

    print("\n6. Прореживание")
    k = [p for p in pts if p["rej"] == 0]
    if len(k) > 1:
        dts = [b["t"] - a["t"] for a, b in zip(k, k[1:])]
        print(f"   записанных точек: {len(k)} ({100 * len(k) / n:.1f} % эпох); интервал: медиана {pct(dts, .5):.0f} с, "
              f"p90 {pct(dts, .9):.0f} с, max {max(dts):.0f} с")
        print("   Точность ломаной по записанным точкам относительно ВСЕХ эпох:")
        idx = [i for i, p in enumerate(pts) if p["rej"] == 0]
        devs = []
        for a, b in zip(idx, idx[1:]):
            for i in range(a + 1, b):
                if pts[i]["rej"] & 0x1f:
                    continue           # забракованные по качеству эпохи в оценку не входят
                devs.append(seg_dist(pts[i], pts[a], pts[b]))
        if devs:
            print(f"   отклонение: медиана {pct(devs, .5):.1f} м, p99 {pct(devs, .99):.1f} м, max {max(devs):.1f} м")


def seg_dist(p, a, b):
    k = math.cos(math.radians(a["lat"]))
    ax, ay = 0.0, 0.0
    bx, by = (b["lon"] - a["lon"]) * 111195.0 * k, (b["lat"] - a["lat"]) * 111195.0
    px, py = (p["lon"] - a["lon"]) * 111195.0 * k, (p["lat"] - a["lat"]) * 111195.0
    l2 = bx * bx + by * by
    if l2 == 0:
        return math.hypot(px, py)
    u = max(0.0, min(1.0, (px * bx + py * by) / l2))
    return math.hypot(px - u * bx, py - u * by)


def parse_utc(text):
    dt = datetime.fromisoformat(text)
    return (dt.replace(tzinfo=timezone.utc) if dt.tzinfo is None else dt).timestamp()


if __name__ == "__main__":
    args = sys.argv[1:]
    t_from, t_to, files = None, None, []
    while args:
        a = args.pop(0)
        if a == "--from" and args:
            t_from = parse_utc(args.pop(0))
        elif a == "--to" and args:
            t_to = parse_utc(args.pop(0))
        else:
            files.append(a)
    if not files:
        sys.exit(__doc__)
    allpts = []
    for path in files:
        allpts += load(path)
    allpts = [p for p in allpts
              if (t_from is None or p["t"] >= t_from) and (t_to is None or p["t"] <= t_to)]
    allpts.sort(key=lambda p: p["t"])
    report(allpts)
