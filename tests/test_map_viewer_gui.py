#!/usr/bin/env python3
"""Тесты tools/map-viewer-gui.py (логика сканирования и, при наличии дисплея, окно).

Запуск из корня репозитория:
    python3 -m unittest tests/test_map_viewer_gui.py
"""
import importlib.util
import os
import shutil
import sys
import tempfile
import time
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tests"))

import test_map_compiler_v4 as base          # noqa: E402
import test_map_viewer_v4 as viewer_tests    # noqa: E402

spec = importlib.util.spec_from_file_location("gui", ROOT / "tools" / "map-viewer-gui.py")
gui = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gui)

try:
    import tkinter as tk
    from tkinter import filedialog, messagebox, ttk
    from tkinter.scrolledtext import ScrolledText
    _root = tk.Tk()
    _root.destroy()
    HAVE_TK = True
except Exception:                                  # no tkinter or no display
    HAVE_TK = False

HAVE_PYGAME = viewer_tests.HAVE_PYGAME


class GuiTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        t = Path(cls.tmp.name)
        (t / "features.csv").write_text(base.FEATURES, encoding="utf-8")
        (t / "t.osm").write_text(base.make_osm(300), encoding="utf-8")
        cls.v4, cls.v3 = t / "v4", t / "v3"
        viewer_tests.compile_map(t / "t.osm", cls.v4, 4, ("--search",))
        viewer_tests.compile_map(t / "t.osm", cls.v3, 3)
        gui.SETTINGS_FILE = t / "settings.json"
        os.environ["SDL_VIDEODRIVER"] = "dummy"

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_scan_v4_and_v3(self):
        info = gui.scan_map_dir(self.v4)
        self.assertEqual([l.name for l in info.layers],
                         ["roads", "landuse", "water", "waterways", "pois"])
        self.assertEqual(info.formats, ["V4"])
        self.assertIsNotNone(info.search)
        self.assertEqual(info.title, "t")
        self.assertIsNone(info.layers[-1].mlp)         # pois has no .mlp
        self.assertEqual(gui.scan_map_dir(self.v3).formats, ["V3"])
        self.assertIsNone(gui.scan_map_dir(self.v3).search)
        self.assertEqual(gui.scan_map_dir(self.tmp.name).layers, [])

    def test_commands(self):
        info = gui.scan_map_dir(self.v4)
        roads, pois = info.layers[0], info.layers[-1]
        cmd = gui.build_view_command(info, roads, 2, 12.5, "/tmp/x.png")
        self.assertEqual(cmd[-6:], ["--size-km", "12.5", "--lod", "2", "--screenshot", "/tmp/x.png"])
        cmd = gui.build_view_command(info, pois, 1, 5)
        self.assertEqual(cmd[3], "-")                  # no .mlp
        v3 = gui.scan_map_dir(self.v3)
        self.assertNotIn("--lod", gui.build_view_command(v3, v3.layers[0], 3, 5))
        self.assertTrue(gui.build_search_command(info)[2].endswith("search.idx"))

    @unittest.skipUnless(HAVE_TK and HAVE_PYGAME, "needs tkinter, a display and pygame")
    def test_window_validate_all(self):
        app = gui.ViewerGui(tk, ttk, filedialog, messagebox, ScrolledText)
        try:
            app.size_var.set("80")
            app.set_folder(str(self.v4))
            self.assertEqual(len(app.tree.get_children()), 6)
            self.wait(app, app.validate_all)
            text = app.log.get("1.0", "end")
            self.assertIn("All layers valid.", text)
            self.assertNotIn("FAILED", text)

            bad = Path(self.tmp.name) / "bad"
            shutil.rmtree(bad, ignore_errors=True)
            shutil.copytree(self.v4, bad)
            data = bytearray((bad / "roads.idx").read_bytes())
            data[0x400 + 1] = 0x77
            (bad / "roads.idx").write_bytes(bytes(data))
            app.clear_log()
            app.set_folder(str(bad))
            self.wait(app, app.validate_all)
            text = app.log.get("1.0", "end")
            self.assertIn("roads: FAILED", text)
            self.assertIn("Failed: roads", text)
        finally:
            app.close()

    @unittest.skipUnless(HAVE_TK and HAVE_PYGAME, "needs tkinter, a display and pygame")
    def test_window_screenshot(self):
        app = gui.ViewerGui(tk, ttk, filedialog, messagebox, ScrolledText)
        try:
            shot = Path(self.tmp.name) / "gui.png"
            app.set_folder(str(self.v4))
            app.size_var.set("80")
            app.lod_var.set(3)
            app.shot_on.set(True)
            app.shot_var.set(str(shot))
            app.tree.selection_set("roads")
            self.wait(app, app.open_selected)
            self.assertTrue(shot.is_file() and shot.stat().st_size > 0)
            self.assertIn("LOD 3:", app.log.get("1.0", "end"))
        finally:
            app.close()

    @staticmethod
    def wait(app, action, timeout=120):
        action()
        end = time.time() + timeout
        while (app.busy or app.lines.qsize()) and time.time() < end:
            app.root.update()
            time.sleep(0.02)
        app.root.update()
        assert not app.busy, "timeout"


if __name__ == "__main__":
    unittest.main()
