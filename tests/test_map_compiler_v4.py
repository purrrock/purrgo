#!/usr/bin/env python3
"""Тесты компилятора карт PurrGO V4 (tools/map-compiler, docs/map-format-v4.md).

Компилирует небольшую синтетическую карту (--format 4 --search) и проверяет
результат независимым разбором по спецификации.

Запуск из корня репозитория:  python3 -m unittest tests/test_map_compiler_v4.py
"""
import json
import math
import random
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / "tools" / "map-compiler"
sys.path.insert(0, str(TOOLS))

import purrgo_search as ps  # noqa: E402

FEATURES = """Code;PG_class;STYLE;LOD;Layer;OSM_Tags;Description;Enabled;Icon
1;ROAD_MAJOR;DARK_GRAY_THICK_LINE;3;roads;highway=motorway;m;1;
2;ROAD_NORMAL;DARK_GRAY_SEMITHICK_LINE;2;roads;highway=primary;p;1;
3;ROAD_MINOR;DARK_GRAY_LINE;1;roads;highway=residential;r;1;
6;ROAD_PATH;DARK_GRAY_DOTTED_LINE;0;roads;highway=footway;f;1;
64;WATER;WATER_FILL;1;water;natural=water;w;1;
32;WATERWAY_MAJOR;WATER_THICK_LINE;2;waterways;waterway=river;r;1;
96;LANDUSE_NATURAL;LIGHT_GRAY_FILL;1;landuse;landuse=forest;f;1;
128;POI_BIG;DARK_GRAY_BIG_CIRCLE;3;pois;place=city;c;1;circle
133;POI_SMALL;DARK_GRAY_CIRCLE;0;pois;amenity=cafe;c;1;cup
167;POI_SMALL;DARK_GRAY_CIRCLE;0;pois;tourism=hotel;h;1;house
152;POI_SMALL;DARK_GRAY_CIRCLE;0;pois;barrier=*;b;1;
"""

LON0, LAT0 = 27.0, 53.0


class OsmBuilder:
    def __init__(self):
        self.nid = 0
        self.nodes, self.ways, self.rels = [], [], []   # OSM order: nodes, ways, relations

    def node(self, dlon, dlat, tags=None):
        self.nid += 1
        t = "".join(f'<tag k="{k}" v="{v}"/>' for k, v in (tags or {}).items())
        self.nodes.append(f'<node id="{self.nid}" lat="{LAT0 + dlat:.7f}" '
                        f'lon="{LON0 + dlon:.7f}">{t}</node>')
        return self.nid

    def way(self, refs, tags):
        self.nid += 1
        nd = "".join(f'<nd ref="{r}"/>' for r in refs)
        t = "".join(f'<tag k="{k}" v="{v}"/>' for k, v in tags.items())
        self.ways.append(f'<way id="{self.nid}">{nd}{t}</way>')
        return self.nid

    def relation(self, members, tags):
        self.nid += 1
        m = "".join(f'<member type="way" ref="{r}" role="{role}"/>' for r, role in members)
        t = "".join(f'<tag k="{k}" v="{v}"/>' for k, v in tags.items())
        self.rels.append(f'<relation id="{self.nid}">{m}{t}</relation>')

    def text(self):
        return ('<?xml version="1.0" encoding="UTF-8"?><osm version="0.6">'
                + "".join(self.nodes + self.ways + self.rels) + "</osm>")


def make_osm(n_roads=7000):
    rnd = random.Random(7)
    b = OsmBuilder()
    # many short residential roads -> tree depth 3 at LOD 0/1
    for i in range(n_roads):
        x = rnd.uniform(0, 0.6)
        y = rnd.uniform(0, 0.4)
        a = b.node(x, y)
        c = b.node(x + 0.0005, y + 0.0003)
        b.way([a, c], {"highway": "residential"})
    # long wiggly motorway (simplification at LOD 2 and 3)
    refs = [b.node(0.01 + i * 0.002, 0.2 + 0.0004 * math.sin(i / 3)) for i in range(250)]
    b.way(refs, {"highway": "motorway", "name": "Трасса М1"})
    refs = [b.node(0.02 + i * 0.002, 0.25 + 0.0006 * math.sin(i / 4)) for i in range(120)]
    b.way(refs, {"highway": "primary"})
    refs = [b.node(0.05, 0.05 + i * 0.001) for i in range(30)]
    b.way(refs, {"waterway": "river", "name": "Свислочь"})
    b.way([b.node(0.3, 0.3), b.node(0.3003, 0.3002)], {"highway": "footway"})
    # lake with island (multipolygon)
    o = [b.node(0.1, 0.1), b.node(0.1, 0.12), b.node(0.12, 0.12), b.node(0.12, 0.1)]
    ow = b.way(o + [o[0]], {"natural": "water"})
    i_ = [b.node(0.108, 0.108), b.node(0.108, 0.112), b.node(0.112, 0.112), b.node(0.112, 0.108)]
    iw = b.way(i_ + [i_[0]], {})
    b.relation([(ow, "outer"), (iw, "inner")], {"type": "multipolygon", "natural": "water",
                                                  "name": "Озеро"})
    f = [b.node(0.2, 0.2), b.node(0.2, 0.21), b.node(0.21, 0.21), b.node(0.21, 0.2)]
    b.way(f + [f[0]], {"landuse": "forest"})
    # named street of three ways inside one search merge cell (0.05 deg) and one
    # way far away (another cell); the stop word moves to the end: "Ленина улица"
    for dlon, dlat in ((0.41, 0.31), (0.42, 0.32), (0.43, 0.33), (0.58, 0.05)):
        b.way([b.node(dlon, dlat), b.node(dlon + 0.001, dlat + 0.0005)],
              {"highway": "residential", "name": "улица Ленина"})
    # named forest polygon (landuse)
    f = [b.node(0.30, 0.10), b.node(0.30, 0.14), b.node(0.34, 0.14), b.node(0.34, 0.10)]
    b.way(f + [f[0]], {"landuse": "forest", "name": "Беловежская пуща"})
    # POIs
    b.node(0.15, 0.15, {"place": "city", "name": "Минск"})
    b.node(0.16, 0.15, {"amenity": "cafe", "name": "Кафе Пушкин"})
    b.node(0.17, 0.15, {"amenity": "cafe"})
    b.node(0.18, 0.15, {"tourism": "hotel", "name": "Hotel Belarus Grand Central Plaza"})
    b.node(0.19, 0.15, {"barrier": "gate"})
    b.node(0.20, 0.15, {"tourism": "hotel", "name": "Очень_длинное_название_" * 6})
    return b.text()


# ---------------------------------------------------------------------------
# Независимый разбор формата
# ---------------------------------------------------------------------------

def read_pgo(path):
    d = Path(path).read_bytes()
    assert d[:3] == b"PGO"
    ftype = d[3]
    payload = struct.unpack_from("<I", d, 4)[0]
    assert payload == len(d) - 32
    lods = struct.unpack_from("<4I", d, 8)
    assert d[0x18:0x20] == bytes(8)
    return d, ftype, lods


def walk_section(d, S, end, check):
    """Returns (depth, root_count, leaves[(bbox, type, v1, v2)]) and validates 5-7."""
    magic, topo, depth, roots, bsize, ssize, navf, leaff = struct.unpack_from("<4sIIIIIHH", d, S)
    assert magic == b"SQT\x02" and topo == 2 and bsize == 512
    assert (navf, leaff) == (18, 20)
    assert ssize == end - S and ssize % 512 == 0
    assert d[S + 28:S + 512] == bytes(484)
    if depth == 0:
        assert roots == 0 and ssize == 512
        return 0, 0, []
    assert 1 <= roots <= 18
    leaves = []
    seen_blocks = []          # (offset, level)

    # level-order traversal; blocks of one group follow one another
    queue = [(S + 512, depth - 1, roots)]
    qi = 0
    while qi < len(queue):
        off, level, count = queue[qi]
        qi += 1
        assert off % 512 == 0 and S + 512 <= off < S + ssize
        seen_blocks.append(off)
        blk = d[off:off + 512]
        if level == -1:
            assert 1 <= count <= 20
            for k in range(count):
                bb = struct.unpack_from("<iiii", blk, k * 25)
                t, v1, v2 = struct.unpack_from("<BII", blk, k * 25 + 16)
                leaves.append((bb, t, v1, v2))
            assert blk[count * 25:] == bytes(512 - count * 25)
        else:
            assert 1 <= count <= 18
            prev_child = None
            for k in range(count):
                child, x0, y0, x1, y1, v1, v2 = struct.unpack_from("<IiiiiII", blk, k * 28)
                assert v1 == level
                assert child % 512 == 0 and child > off, "child_block > B"
                assert S + 1024 <= child and child + 512 <= S + ssize
                if prev_child is not None:
                    assert child == prev_child + 512, "children blocks are consecutive"
                prev_child = child
                queue.append((child, level - 1, v2))
                check(("nav", (x0, y0, x1, y1), child, v2, level))
            assert blk[count * 28:] == bytes(512 - count * 28)
    # blocks are exactly 512 apart, in BFS order, covering the section
    assert seen_blocks == [S + 512 + 512 * i for i in range(len(seen_blocks))]
    assert len(seen_blocks) * 512 + 512 == ssize
    return depth, roots, leaves


def read_geometry(mlp, v1):
    o = 0x20 + v1
    bbox = struct.unpack_from("<iiii", mlp, o)
    nparts, npts = struct.unpack_from("<ii", mlp, o + 16)
    parts = struct.unpack_from(f"<{nparts}I", mlp, o + 24)
    pts = [struct.unpack_from("<ii", mlp, o + 24 + nparts * 4 + i * 8) for i in range(npts)]
    # local header check
    ln = struct.unpack_from("<I", mlp, o - 4)[0]
    assert ln == 24 + nparts * 4 + npts * 8
    return bbox, parts, pts


class CompilerV4Test(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        t = Path(cls.tmp.name)
        (t / "features.csv").write_text(FEATURES, encoding="utf-8")
        (t / "t.osm").write_text(make_osm(), encoding="utf-8")
        cls.out = t / "out"
        r = subprocess.run(
            [sys.executable, str(TOOLS / "purrgo_map_compiler.py"), str(t / "t.osm"),
             "--format", "4", "--search", "-o", str(cls.out),
             "--features", str(t / "features.csv"),
             "--search-categories", str(TOOLS / "search_categories.csv")],
            capture_output=True, text=True)
        cls.log = r.stdout + r.stderr
        assert r.returncode == 0, cls.log

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_package_files(self):
        names = {p.name for p in self.out.iterdir()}
        for n in ("map.name", "roads.idx", "roads.mlp", "roads.db", "water.idx",
                  "water.mlp", "landuse.idx", "waterways.idx", "waterways.mlp",
                  "pois.idx", "pois.db", "search.idx"):
            self.assertIn(n, names)
        meta = json.loads((self.out / "map.name").read_text(encoding="utf-8"))
        self.assertEqual(set(meta), {"centerLat", "centerLon", "mapName"})

    def _layer(self, name):
        d, ftype, lods = read_pgo(self.out / f"{name}.idx")
        self.assertEqual(ftype, 1)
        self.assertEqual(lods[0], 0x200)
        self.assertEqual(d[0x20:0x200], bytes(480))
        self.assertEqual(len(d) % 512, 0)
        self.assertTrue(lods[0] < lods[1] < lods[2] < lods[3] < len(d))
        ends = list(lods[1:]) + [len(d)]
        res = []
        navs = []
        for S, E in zip(lods, ends):
            depth, roots, leaves = walk_section(d, S, E, navs.append)
            res.append((depth, roots, leaves))
        return res, navs

    def test_roads_tree_and_lod_membership(self):
        res, _ = self._layer("roads")
        counts = [len(r[2]) for r in res]
        self.assertGreater(res[0][0], 2, "depth 3 expected for 7000+ objects")
        self.assertEqual(counts[0], 7000 + 3 + 4)       # all roads (+4 ways of the named street)
        self.assertEqual(counts[1], 7000 + 2 + 4)       # lod >= 1
        self.assertEqual(counts[2], 2)                  # motorway, primary
        self.assertEqual(counts[3], 1)                  # motorway
        for depth, roots, leaves in res:
            for bb, t, v1, v2 in leaves:
                self.assertTrue(bb[0] <= bb[2] and bb[1] <= bb[3])

    def test_nav_bbox_contains_children(self):
        d, _, lods = read_pgo(self.out / "roads.idx")
        # collect (child_block -> bbox) and check each child's entries inside
        for S in lods[:2]:
            ssize = struct.unpack_from("<I", d, S + 0x14)[0]
            depth = struct.unpack_from("<I", d, S + 8)[0]
            queue = [(S + 512, depth - 1, struct.unpack_from("<I", d, S + 0xC)[0], None)]
            while queue:
                off, level, count, parent_bb = queue.pop()
                for k in range(count):
                    if level == -1:
                        bb = struct.unpack_from("<iiii", d, off + k * 25)
                    else:
                        child, *bb, v1, v2 = struct.unpack_from("<Iiiii II", d, off + k * 28)
                        bb = tuple(bb)
                        queue.append((child, v1 - 1, v2, bb))
                    if parent_bb:
                        self.assertTrue(parent_bb[0] <= bb[0] and parent_bb[1] <= bb[1]
                                        and bb[2] <= parent_bb[2] and bb[3] <= parent_bb[3])

    def test_variants_are_point_subsets(self):
        res, _ = self._layer("roads")
        mlp = Path(self.out / "roads.mlp").read_bytes()
        full = {}
        for bb, t, v1, v2 in res[0][2]:
            full[(t, bb)] = v1
        # motorway (code 1)
        mot = [l for l in res[0][2] if l[1] == 1][0]
        _, _, full_pts = read_geometry(mlp, mot[2])
        self.assertEqual(len(full_pts), 250)
        for lod, tol_m in ((2, 50), (3, 250)):
            node = [l for l in res[lod][2] if l[1] == 1][0]
            bbox, parts, pts = read_geometry(mlp, node[2])
            self.assertEqual(node[0], bbox, "node bbox == record bbox")
            self.assertLess(len(pts), len(full_pts))
            it = iter(full_pts)
            self.assertTrue(all(p in it for p in pts), "subset in original order")
            self.assertEqual((pts[0], pts[-1]), (full_pts[0], full_pts[-1]))
            self.assertGreaterEqual(len(pts), 2)
        # LOD 0 and LOD 1 use the full geometry (the same v1)
        n1 = [l for l in res[1][2] if l[1] == 1][0]
        self.assertEqual(n1[2], mot[2])
        # residential roads: all LODs >= 1 share full geometry
        self.assertTrue(all(l[2] > 0 for l in res[0][2]))

    def test_db_and_names(self):
        d, ftype, _ = read_pgo(self.out / "pois.db")
        self.assertEqual(ftype, 3)
        self.assertEqual(d[0x20], 3)
        nrec = struct.unpack_from("<I", d, 0x24)[0]
        self.assertEqual(struct.unpack_from("<HH", d, 0x28), (129, 117))
        self.assertEqual(nrec, 6)
        names = []
        for i in range(nrec):
            rec = d[0x20 + 129 + i * 117:0x20 + 129 + (i + 1) * 117]
            self.assertEqual(rec[0], 0x20)
            names.append(rec[17:117].rstrip(b"\0"))
        enc = lambda t: ps.pgo_encode(t, 100).rstrip(b"\0")
        cut = [n for n in names if n.endswith(b"..")]
        self.assertEqual(len(cut), 1)
        self.assertTrue(90 <= len(cut[0]) <= 100, "98 chars + '..' (trailing '_' removed)")
        self.assertIn(enc("Belarus_Grand_Central_Plaza_hotel"),
                      names, "no 22-char shortening; stop word moved to the end")
        self.assertIn(enc("Пушкин_кафе"), names)
        self.assertIn(b"cafe", names, "unnamed POI gets the rule tag value")
        # POIs: every v2 >= 1
        res, _ = self._layer("pois")
        v2s = [l[3] for l in res[0][2]]
        self.assertTrue(all(v >= 1 for v in v2s))
        self.assertEqual(sorted(v2s), list(range(1, 7)))
        self.assertEqual([len(r[2]) for r in res], [6, 1, 1, 1])
        # roads .db: dummy record 1 + named
        d, _, _ = read_pgo(self.out / "roads.db")
        self.assertEqual(d[0x20 + 129:0x20 + 129 + 117], bytes(117))

    def test_polygons_water(self):
        res, _ = self._layer("water")
        mlp = Path(self.out / "water.mlp").read_bytes()
        leaves = res[0][2]
        self.assertEqual(len(leaves), 1, "multipolygon replaces standalone outer way")
        bbox, parts, pts = read_geometry(mlp, leaves[0][2])
        self.assertEqual(len(parts), 2)
        for a, b in zip(parts, list(parts[1:]) + [len(pts)]):
            self.assertEqual(pts[a], pts[b - 1])

    def _search(self):
        """search.idx parsed independently: (bytes, header dict, names, cats)."""
        d, ftype, lods = read_pgo(self.out / "search.idx")
        self.assertEqual(ftype, 4)
        self.assertEqual(lods, (0, 0, 0, 0))
        (magic, ver, rec, flags, dbrec, noff, ncnt, coff, ccnt, doff, dsz,
         roff, rcnt, l_roads, l_ww, l_water, l_land) = struct.unpack_from(
             "<4sHHIIIIIIIIII4I", d, 0x20)
        self.assertEqual((magic, ver, rec, flags, dbrec), (b"SRCH", 1, 32, 0, 6))
        self.assertEqual((doff, roff, rcnt), (0x60, 0, 0))
        self.assertEqual(noff % 512, 0)
        self.assertEqual(coff % 512, 0)
        self.assertEqual(len(d) % 512, 0)
        self.assertLessEqual(noff + 32 * ncnt, coff)
        self.assertLessEqual(coff + 32 * ccnt, len(d))
        n, esz, res0 = struct.unpack_from("<HHI", d, doff)
        self.assertEqual((esz, res0, dsz), (48, 0, 8 + 48 * n))
        words = [d[doff + 8 + i * 48:doff + 8 + i * 48 + 16] for i in range(n)]
        self.assertEqual(words, sorted(set(words)))

        def entries(off, cnt):
            out = []
            for i in range(cnt):
                lon, lat, db_idx, code, sub, fl, kl, key = struct.unpack_from(
                    "<iiIBBBB16s", d, off + i * 32)
                out.append((lon, lat, db_idx, code, sub, fl, kl, key))
            return out

        hdr = {"layer_records": (l_roads, l_ww, l_water, l_land), "dict_words": set(
            w.rstrip(b"\0") for w in words)}
        return d, hdr, entries(noff, ncnt), entries(coff, ccnt)

    def test_search_idx(self):
        d, hdr, names, cats = self._search()
        # .db records (with the dummy one): roads - "Трасса М1" and four ways of the
        # street (merging is done by the index, not by the .db); waterways - river;
        # water - lake; landuse - named forest
        self.assertEqual(hdr["layer_records"], (6, 2, 2, 2))
        layer_of = lambda c: ps.layer_index_for_code(c)

        # order: key, layer (POI first), db_idx, primary before secondary
        lk = lambda e: layer_of(e[3]) + 1 if e[3] < 128 else 0
        keys = [(e[7], lk(e), e[2], e[5] & 1) for e in names]
        self.assertEqual(keys, sorted(keys))
        for e in names:
            self.assertTrue(e[6] <= 16)
            self.assertEqual(e[7][e[6]:], bytes(16 - e[6]))
            self.assertNotIn(0xFF, e[7])
            self.assertEqual(e[5] & ~7, 0)
        pois = [e for e in names if e[3] >= 128]
        # barrier (code 152) is not searchable, substituted names are not in NAME
        self.assertTrue(all(e[3] != 152 for e in names))
        self.assertTrue(all(1 <= e[2] <= 6 for e in pois))
        # "Пушкин кафе": primary 'пушкин', secondary 'кафе'
        by_key = {e[7].rstrip(b"\0") for e in pois}
        enc = lambda s: ps.pgo_encode(s, 16).rstrip(b"\0")
        self.assertIn(ps.split_words(enc("Пушкин"))[0], by_key)
        self.assertIn(ps.split_words(enc("кафе"))[0], by_key)
        self.assertTrue([e for e in pois if e[5] & 1])
        order = [(e[4], ps.morton(e[0], e[1]), lk(e), e[2]) for e in cats]
        self.assertEqual(order, sorted(order))
        self.assertTrue(all(e[4] != 0 for e in cats))
        self.assertTrue(any(e[5] & 2 and e[6] == 0 for e in cats), "unnamed cafe in CAT")
        self.assertTrue(all(not (e[5] & 2) for e in cats if e[3] < 128),
                        "unnamed objects of the line and polygon layers are not indexed")
        # cross-check with pois.idx: lon/lat/code agree for every db_idx
        res, _ = self._layer("pois")
        node = {l[3]: (l[0][0], l[0][1], l[1]) for l in res[0][2]}
        for e in [x for x in names + cats if x[3] >= 128]:
            self.assertEqual(node[e[2]], (e[0], e[1], e[3]))

    def test_search_idx_layers(self):
        """Streets, rivers, lakes and landuse: layer from the code, point on the object."""
        d, hdr, names, cats = self._search()
        enc = lambda t: ps.split_words(ps.pgo_encode(t, 100).rstrip(b"\0"))
        layer_files = ("roads", "waterways", "water", "landuse")
        non_poi = [e for e in names if e[3] < 128]
        self.assertTrue(non_poi)
        for e in non_poi:
            lname = layer_files[ps.layer_index_for_code(e[3])]
            res, _ = self._layer(lname)
            leaf = [l for l in res[0][2] if l[3] == e[2]]
            self.assertEqual(len(leaf), 1, f"{lname}: db_idx {e[2]} is a leaf of LOD 0")
            self.assertEqual(leaf[0][1], e[3], "code is the code of the object")
            mlp = (self.out / f"{lname}.mlp").read_bytes()
            _, _, pts = read_geometry(mlp, leaf[0][2])
            self.assertIn((e[0], e[1]), pts, "representative point is a vertex")
            db, _, _ = read_pgo(self.out / f"{lname}.db")
            rec = db[0x20 + 129 + (e[2] - 1) * 117:0x20 + 129 + e[2] * 117]
            self.assertEqual(rec[0], 0x20)
            full = ps.split_words(rec[17:117].rstrip(b"\0"))
            self.assertIn(e[7].rstrip(b"\0"), [w[:16] for w in full])
        # .db record counts of the layers match layer_records
        for i, lname in enumerate(layer_files):
            db, _, _ = read_pgo(self.out / f"{lname}.db")
            self.assertEqual(struct.unpack_from("<I", db, 0x24)[0], hdr["layer_records"][i])

        # merging: three ways of one street in one cell -> one entry; the far way -> another
        lenina = enc("Ленина")[0]
        streets = [e for e in names if e[3] < 32 and e[7].rstrip(b"\0") == lenina]
        self.assertEqual(len(streets), 2)
        self.assertTrue(all(e[5] & 1 == 0 for e in streets))
        # "улица" is a dictionary keyword: not indexed as a secondary word of a street
        self.assertIn(enc("улица")[0], hdr["dict_words"])
        self.assertFalse([e for e in non_poi if e[5] & 1 and e[7].rstrip(b"\0") == enc("улица")[0]])
        # but "М1" is: second word of "Трасса М1"
        self.assertTrue([e for e in non_poi if e[5] & 1 and e[7].rstrip(b"\0") == enc("м1")[0]])
        # subtypes: street 130, river 140, lake 150, forest 160
        sub = {ps.layer_index_for_code(e[3]): e[4] for e in names if e[3] < 128 and not e[5] & 1}
        self.assertEqual(sub, {0: 130, 1: 140, 2: 150, 3: 160})
        # CAT holds the named non-POI objects once each
        cat_non_poi = [e for e in cats if e[3] < 128]
        self.assertEqual(len(cat_non_poi), 6)   # M1, 2 x Ленина, river, lake, forest

    def test_deterministic(self):
        t = Path(self.tmp.name)
        out2 = t / "out2"
        r = subprocess.run(
            [sys.executable, str(TOOLS / "purrgo_map_compiler.py"), str(t / "t.osm"),
             "--format", "4", "--search", "-o", str(out2),
             "--features", str(t / "features.csv"),
             "--search-categories", str(TOOLS / "search_categories.csv")],
            capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        for p in self.out.iterdir():
            self.assertEqual(p.read_bytes(), (out2 / p.name).read_bytes(), p.name)


class SearchHelpers(unittest.TestCase):
    def test_fold_and_words(self):
        enc = lambda s: ps.pgo_encode(s, 100).rstrip(b"\0")
        self.assertEqual(ps.split_words(enc("Ёж_и-Ёлка")), [enc("еж"), enc("и"), enc("елка")])
        self.assertEqual(ps.split_words(enc("ABC def")), [b"abc", b"def"])
        self.assertEqual(ps.split_words(b""), [])

    def test_layer_index_and_anchor(self):
        self.assertEqual([ps.layer_index_for_code(c) for c in (0, 31, 32, 63, 64, 95, 96, 127, 128)],
                         [0, 0, 1, 1, 2, 2, 3, 3, -1])

        class F:
            pass

        line = F()
        line.code, line.parts = 1, (0,)
        line.points = struct.pack("<8i", 0, 0, 10, 0, 20, 0, 30, 0)
        line.bbox = (0, 0, 30, 0)
        self.assertEqual(ps.anchor_point(line), (20, 0), "middle vertex of a line")
        poly = F()
        poly.code, poly.parts = 64, (0,)
        poly.points = struct.pack("<10i", 0, 0, 0, 100, 40, 100, 100, 100, 0, 0)
        poly.bbox = (0, 0, 100, 100)
        self.assertEqual(ps.anchor_point(poly), (40, 100), "vertex nearest to the bbox centre")

    def test_morton(self):
        self.assertEqual(ps.morton(-1_800_000_000, -900_000_000), 0)
        self.assertEqual(ps.morton(-1_800_000_000 + 1, -900_000_000), 1)
        self.assertEqual(ps.morton(-1_800_000_000, -900_000_000 + 1), 2)


if __name__ == "__main__":
    unittest.main()
