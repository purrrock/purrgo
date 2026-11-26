"""
Фикстура host-теста умного поиска (tests/host/data/search-map, search_fold_vectors.txt) лежит в репозитории
готовой: C-тест (tests/host/test_map_search.c) не запускает Python. Этот тест следит, чтобы файлы совпадали
с тем, что строит компилятор сейчас; после изменения формата или правил поиска их нужно пересобрать:
    python3 tests/host/data/make_search_map.py
"""
import importlib.util
import pathlib
import sys
import tempfile
import unittest
from unittest import mock

ROOT = pathlib.Path(__file__).resolve().parents[1]
GEN = ROOT / "tests" / "host" / "data" / "make_search_map.py"


def load_generator():
    spec = importlib.util.spec_from_file_location("make_search_map", GEN)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


class SearchFixtureTest(unittest.TestCase):
    def test_fixture_is_up_to_date(self):
        gen = load_generator()
        with tempfile.TemporaryDirectory() as tmp:
            out = pathlib.Path(tmp) / "search-map"
            with mock.patch.object(gen, "OUT", out):
                with mock.patch("sys.stdout"):
                    gen.main()
            for name in gen.FILES:
                fresh = (out / name).read_bytes()
                kept = (gen.OUT / name).read_bytes()
                self.assertEqual(fresh, kept, f"{name} is stale: run tests/host/data/make_search_map.py")
            self.assertEqual((out.parent / "search_fold_vectors.txt").read_text(),
                             (gen.OUT.parent / "search_fold_vectors.txt").read_text())


if __name__ == "__main__":
    unittest.main()
