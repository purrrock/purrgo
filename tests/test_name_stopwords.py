#!/usr/bin/env python3
"""Тесты переноса типа объекта в конец названия (purrgo_osmparser._sanitize_name_cached).

Запуск из корня репозитория:  python3 -m unittest tests/test_name_stopwords.py
"""
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools" / "map-compiler"))

from purrgo_osmparser import _sanitize_name_cached as sanitize  # noqa: E402


class StopWordTests(unittest.TestCase):
    def test_whole_word_is_moved(self):
        self.assertEqual(sanitize("Кафе Пушкин"), "Пушкин_кафе")
        self.assertEqual(sanitize("проспект Независимости"), "Независимости_проспект")

    def test_abbreviation_may_be_attached(self):
        self.assertEqual(sanitize("ул.Ленина"), "Ленина_ул.")
        self.assertEqual(sanitize("ул. Ленина"), "Ленина_ул.")

    def test_prefix_of_longer_word_is_not_split(self):
        for name in ("Кафетерий", "Паркинг", "Магазинчик", "Станционная", "Озерцо",
                     "Кафе-бар", "Marketplace", "Shopping", "Hotelier"):
            self.assertEqual(sanitize(name), name, name)

    def test_only_stop_word_is_unchanged(self):
        self.assertEqual(sanitize("Кафе"), "Кафе")


if __name__ == "__main__":
    unittest.main()
