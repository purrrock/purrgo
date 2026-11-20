#!/usr/bin/env python3
"""Тесты tools/map-viewer.py: валидатор форматов V3 и V4 и search.idx.

Пропускается, если не установлен pygame. Запуск из корня репозитория:
    python3 -m unittest tests/test_map_viewer_v4.py
"""
import importlib.util
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / "tools" / "map-compiler"
VIEWER = ROOT / "tools" / "map-viewer.py"
sys.path.insert(0, str(ROOT / "tests"))

import test_map_compiler_v4 as base  # noqa: E402

HAVE_PYGAME = importlib.util.find_spec("pygame") is not None
ENV = dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy")


def run_viewer(*args):
    r = subprocess.run([sys.executable, str(VIEWER), *map(str, args)],
                       capture_output=True, text=True, env=ENV)
    return r.returncode, r.stdout + r.stderr


def compile_map(osm, out, fmt, extra=()):
    r = subprocess.run(
        [sys.executable, str(TOOLS / "purrgo_map_compiler.py"), str(osm),
         "--format", str(fmt), "-o", str(out),
         "--features", str(osm.parent / "features.csv"), *extra],
        capture_output=True, text=True)
    assert r.returncode == 0, r.stdout + r.stderr


@unittest.skipUnless(HAVE_PYGAME, "pygame is not installed")
class ViewerTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        t = Path(cls.tmp.name)
        (t / "features.csv").write_text(base.FEATURES, encoding="utf-8")
        (t / "t.osm").write_text(base.make_osm(800), encoding="utf-8")
        cls.v4 = t / "v4"
        cls.v3 = t / "v3"
        compile_map(t / "t.osm", cls.v4, 4, ("--search",))
        compile_map(t / "t.osm", cls.v3, 3)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def view(self, d, layer, *extra):
        db = d / f"{layer}.db"
        return run_viewer(d / f"{layer}.idx", d / f"{layer}.mlp", d / "map.name",
                          db if db.exists() else "", "--size-km", 80, "--screenshot",
                          Path(self.tmp.name) / "shot.png", *extra)

    def test_v4_layers_all_lods(self):
        for layer in ("roads", "water", "waterways", "landuse"):
            for lod in range(4):
                rc, out = self.view(self.v4, layer, "--lod", lod)
                self.assertEqual(rc, 0, out)
                self.assertIn("All four V4 IDX LOD sections validated", out)
        rc, out = self.view(self.v4, "roads", "--lod", 3)
        self.assertIn("lines=1,", out)           # only the motorway at LOD 3

    def test_v4_pois_without_mlp(self):
        d = self.v4
        rc, out = run_viewer(d / "pois.idx", "-", d / "map.name", "--size-km", 80,
                             "--screenshot", Path(self.tmp.name) / "p.png")
        self.assertEqual(rc, 0, out)
        self.assertIn("pois=6", out)

    def test_v3_still_works(self):
        rc, out = self.view(self.v3, "roads")
        self.assertEqual(rc, 0, out)
        self.assertIn("All three IDX LOD sections validated", out)

    def corrupt(self, name, offset, value, new_name=None):
        d = Path(self.tmp.name) / "bad"
        shutil.rmtree(d, ignore_errors=True)
        shutil.copytree(self.v4, d)
        p = d / name
        data = bytearray(p.read_bytes())
        data[offset] = value
        p.write_bytes(bytes(data))
        return d

    def test_v4_detects_corruption(self):
        # child_block of the first navigation node of LOD 0 (first block at 0x400)
        d = self.corrupt("roads.idx", 0x400 + 1, 0x77)
        rc, out = self.view(d, "roads")
        self.assertEqual(rc, 1)
        self.assertIn("child_block", out)
        # non-zero padding in the SQT header
        d = self.corrupt("roads.idx", 0x200 + 100, 1)
        rc, out = self.view(d, "roads")
        self.assertEqual(rc, 1)
        self.assertIn("reserved", out)
        # LOD 3 offset must stay a multiple of 512
        d = self.corrupt("roads.idx", 0x14, 0x01)
        rc, out = self.view(d, "roads")
        self.assertEqual(rc, 1)

    def test_search_idx(self):
        d = self.v4
        rc, out = run_viewer(d / "search.idx", d / "pois.db")
        self.assertEqual(rc, 0, out)
        self.assertIn("search.idx validation successful", out)
        # break sorting of the dictionary: change the first byte of word #0
        d2 = self.corrupt("search.idx", 0x60 + 8, 0x7A)
        rc, out = run_viewer(d2 / "search.idx", d2 / "pois.db")
        self.assertEqual(rc, 1)
        self.assertIn("FORMAT ERROR", out)


if __name__ == "__main__":
    unittest.main()
