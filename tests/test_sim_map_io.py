#!/usr/bin/env python3
"""Тесты tools/map-compiler/sim_map_io.py: карты V3 и V4 и режим --compare.

Запуск из корня репозитория:  python3 -m unittest tests/test_sim_map_io.py
"""
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / "tools" / "map-compiler"
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(ROOT / "tests"))

import sim_map_io as sim                    # noqa: E402
import test_map_compiler_v4 as base         # noqa: E402

SCALES = "100,2000,10000,50000,200000"


def compile_map(osm, out, fmt):
    r = subprocess.run(
        [sys.executable, str(TOOLS / "purrgo_map_compiler.py"), str(osm), "--format", str(fmt),
         "-o", str(out), "--features", str(osm.parent / "features.csv")],
        capture_output=True, text=True)
    assert r.returncode == 0, r.stdout + r.stderr


def run_sim(*args):
    r = subprocess.run([sys.executable, str(TOOLS / "sim_map_io.py"), *map(str, args)],
                       capture_output=True, text=True)
    assert r.returncode == 0, r.stdout + r.stderr
    return r.stdout


def rows(out):
    """scale -> list of numeric fields of the first table row (--compare or plain)."""
    res = {}
    for line in out.splitlines():
        m = re.match(r"\s*(\d+)\s+(\d)(?:/(\d))?\s*\|?\s*(.*)", line)
        if m and int(m.group(1)) in (100, 2000, 10000, 50000, 200000):
            res[int(m.group(1))] = (m.group(2), m.group(3), m.group(4))
    return res


class SimTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        t = Path(cls.tmp.name)
        (t / "features.csv").write_text(base.FEATURES, encoding="utf-8")
        (t / "t.osm").write_text(base.make_osm(1500), encoding="utf-8")
        cls.v3, cls.v4 = t / "v3", t / "v4"
        compile_map(t / "t.osm", cls.v3, 3)
        compile_map(t / "t.osm", cls.v4, 4)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_target_lod(self):
        self.assertEqual([sim.target_lod(s) for s in (500, 2000, 50000, 10 ** 6)], [0, 1, 2, 2])
        self.assertEqual([sim.target_lod(s, True, 4) for s in (500, 5000, 20000, 20001)],
                         [0, 1, 2, 3])
        self.assertEqual([sim.target_lod(s, False, 4) for s in (100, 2000, 10000, 10001)],
                         [0, 1, 2, 3])

    def test_format_detection(self):
        self.assertEqual(sim.map_format(str(self.v3)), "V3")
        self.assertEqual(sim.map_format(str(self.v4)), "V4")
        self.assertIsNone(sim.map_format(self.tmp.name))

    def test_v4_selects_lod3_and_simplified_geometry(self):
        out3 = run_sim(self.v3, 53.2, 27.3, "--scales", SCALES)
        out4 = run_sim(self.v4, 53.2, 27.3, "--scales", SCALES)
        self.assertIn("[V3]", out3)
        self.assertIn("[V4]", out4)
        r3, r4 = rows(out3), rows(out4)
        self.assertEqual([r3[s][0] for s in sorted(r3)], ["0", "1", "2", "2", "2"])
        self.assertEqual([r4[s][0] for s in sorted(r4)], ["0", "1", "2", "3", "3"])
        verts = lambda r, s: int(r[s][2].split()[3])
        self.assertEqual(verts(r3, 100), verts(r4, 100))       # same geometry on LOD 0
        self.assertLess(verts(r4, 10000), verts(r3, 10000))    # LOD 2 is simplified
        self.assertLess(verts(r4, 200000), verts(r3, 200000))  # LOD 3 is simplified

    def test_max_lod_limits_v4(self):
        out = run_sim(self.v4, 53.2, 27.3, "--scales", "200000", "--max-lod", 2)
        self.assertEqual(rows(out)[200000][0], "2")

    def test_compare(self):
        out = run_sim(self.v3, 53.2, 27.3, "--scales", SCALES, "--compare", self.v4)
        self.assertIn("A=", out)
        self.assertIn("[V3]", out)
        self.assertIn("[V4]", out)
        r = rows(out)
        self.assertEqual({k: (v[0], v[1]) for k, v in r.items()},
                         {100: ("0", "0"), 2000: ("1", "1"), 10000: ("2", "2"),
                          50000: ("2", "3"), 200000: ("2", "3")})
        # fewer SD transfers for the same frame at small scales
        fields = r[100][2].split()
        xfers_a, xfers_b = int(fields[0]), int(fields[1])
        self.assertLess(xfers_b, xfers_a)


if __name__ == "__main__":
    unittest.main()
