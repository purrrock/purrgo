#!/usr/bin/env python3
"""
Строит tests/host/data/search-map/: маленькую карту V4 (search.idx и базы названий) для
host-теста умного поиска (tests/host/test_map_search.c). Карта собирается настоящим компилятором
(tools/map-compiler) с боевыми features.csv и search_categories.csv, поэтому тест проверяет
согласованность писателя (Python) и читателя (C) формата search.idx.

Запуск из корня репозитория:  python3 tests/host/data/make_search_map.py
Тест tests/test_search_fixture.py проверяет, что файлы в репозитории совпадают с результатом.
Также пишет search_fold_vectors.txt - общие векторы нормализации ключа (Python и C).
"""
import pathlib
import shutil
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[3]
TOOLS = ROOT / "tools" / "map-compiler"
OUT = ROOT / "tests" / "host" / "data" / "search-map"
FILES = ("search.idx", "pois.db", "roads.db", "waterways.db", "water.db", "landuse.db")

LON0, LAT0 = 27.5, 53.9     # начало отсчёта тестов


class Osm:
    def __init__(self):
        self.n = 0
        self.nodes, self.ways = [], []

    def node(self, dlon, dlat, tags=None):
        self.n += 1
        t = "".join(f'<tag k="{k}" v="{v}"/>' for k, v in (tags or {}).items())
        self.nodes.append(f'<node id="{self.n}" lat="{LAT0 + dlat:.7f}" lon="{LON0 + dlon:.7f}">{t}</node>')
        return self.n

    def way(self, refs, tags):
        self.n += 1
        nd = "".join(f'<nd ref="{r}"/>' for r in refs)
        t = "".join(f'<tag k="{k}" v="{v}"/>' for k, v in tags.items())
        self.ways.append(f'<way id="{self.n}">{nd}{t}</way>')

    def line(self, dlon, dlat, tags):
        self.way([self.node(dlon, dlat), self.node(dlon + 0.002, dlat + 0.001),
                  self.node(dlon + 0.004, dlat + 0.002)], tags)

    def poly(self, x0, y0, x1, y1, tags):
        ring = [self.node(x0, y0), self.node(x0, y1), self.node(x1, y1), self.node(x1, y0)]
        self.way(ring + [ring[0]], tags)

    def text(self):
        return ('<?xml version="1.0" encoding="UTF-8"?><osm version="0.6">'
                + "".join(self.nodes + self.ways) + "</osm>")


def make_osm():
    b = Osm()
    # POI
    b.node(0.002, 0.002, {"amenity": "cafe"})                              # без названия
    b.node(0.010, 0.010, {"amenity": "cafe", "name": "Кафе Пушкин"})
    b.node(0.050, 0.020, {"amenity": "cafe", "name": "Кафе Берёзка"})
    b.node(1.000, 1.000, {"amenity": "cafe", "name": "Кафе Пушкин"})       # то же имя, далеко
    b.node(0.020, 0.000, {"amenity": "pharmacy", "name": "Аптека Пушкина"})
    b.node(0.030, 0.030, {"tourism": "hotel", "name": "Hotel Belarus"})
    b.node(0.500, 0.500, {"place": "city", "name": "Минск"})
    # улицы: две в одной клетке слияния (0.05 градуса) и одна далеко
    b.line(0.004, 0.006, {"highway": "residential", "name": "улица Пушкина"})
    b.line(0.008, 0.007, {"highway": "residential", "name": "улица Пушкина"})
    b.line(0.200, 0.200, {"highway": "residential", "name": "улица Пушкина"})
    b.line(0.020, 0.010, {"highway": "primary", "name": "проспект Независимости"})
    # река, озеро, парк, лес
    b.way([b.node(0.030 + i * 0.001, 0.040 + i * 0.002) for i in range(6)],
          {"waterway": "river", "name": "Свислочь"})
    b.poly(0.060, 0.050, 0.070, 0.060, {"natural": "water", "name": "Озеро Лошицкое"})
    b.poly(0.010, 0.030, 0.020, 0.040, {"leisure": "park", "name": "Парк Горького"})
    b.poly(0.300, 0.100, 0.340, 0.140, {"landuse": "forest", "name": "Пушкинский лес"})
    return b.text()


VECTORS = [
    "Кафе Пушкин", "ПАРК Горького", "Ёлка", "ёлка-палка", "Hotel Belarus", "A1 b2", "М1",
    "Берёзка", "улица  Ленина,", "  ", "", "x", "Очень_длинное_слово_названия_больше_шестнадцати",
    "Лошицкое озеро", "ЁЁ", "кафе-бар №5",
]


def write_vectors(path):
    sys.path.insert(0, str(TOOLS))
    import purrgo_search as ps
    lines = []
    for text in VECTORS:
        raw = ps.pgo_encode(text, 100).rstrip(b"\x00")
        words = ps.split_words(raw)
        lines.append(raw.hex() + "|" + ",".join(w[:16].hex() for w in words))
    path.write_text("\n".join(lines) + "\n", encoding="ascii")


def main():
    with tempfile.TemporaryDirectory() as tmp:
        tmp = pathlib.Path(tmp)
        (tmp / "map.osm").write_text(make_osm(), encoding="utf-8")
        subprocess.run([sys.executable, str(TOOLS / "purrgo_map_compiler.py"), str(tmp / "map.osm"),
                        "--format", "4", "--search", "-o", str(tmp / "out"),
                        "--features", str(TOOLS / "features.csv"),
                        "--search-categories", str(TOOLS / "search_categories.csv")],
                       check=True, stdout=subprocess.DEVNULL)
        out = tmp / "out"
        found = list(out.rglob("search.idx"))
        if len(found) != 1:
            sys.exit("search.idx not produced")
        src = found[0].parent
        if OUT.exists():
            shutil.rmtree(OUT)
        OUT.mkdir(parents=True)
        for name in FILES:
            shutil.copy(src / name, OUT / name)
    write_vectors(OUT.parent / "search_fold_vectors.txt")
    print("written", OUT)


if __name__ == "__main__":
    main()
