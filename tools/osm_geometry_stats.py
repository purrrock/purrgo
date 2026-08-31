#!/usr/bin/env python3
"""
osm_geometry_stats.py

Анализирует OSM XML-файл и выводит:
    - TOP-10 линейных way по количеству точек;
    - TOP-10 полигональных way по количеству точек.

Скрипт предназначен для исследования реальных размеров OSM-объектов
перед определением максимального размера geometry chunk в PurrGO.

Вход:
    OSM XML / .osm файл

Пример:
    python osm_geometry_stats.py germany.osm

Для больших OSM-файлов используется потоковый XML-парсер iterparse,
поэтому весь XML не загружается в RAM.
"""

import argparse
import heapq
import xml.etree.ElementTree as ET


TOP_N = 10


def classify_way(tags, node_count, is_closed):
    """
    Определяет, считать ли way линией или полигоном.

    ВАЖНО:
    Это исследовательская классификация, а не реализация
    классификатора features.csv PurrGO.

    Если way замкнут, он рассматривается как polygon.

    Незамкнутый way рассматривается как line.

    Это позволяет получить статистику размеров геометрии,
    независимо от конкретной PurrGO-классификации.
    """

    if node_count < 2:
        return None

    if is_closed:
        return "polygon"

    return "line"


def add_top(top_list, point_count, osm_id):
    """
    Добавляет объект в TOP-N.

    Используем min-heap, поэтому не приходится хранить
    все объекты OSM в памяти.
    """

    item = (point_count, osm_id)

    if len(top_list) < TOP_N:
        heapq.heappush(top_list, item)
    elif point_count > top_list[0][0]:
        heapq.heapreplace(top_list, item)


def analyze_osm(filename):
    """
    Потоково читает OSM XML и собирает TOP-10
    линий и полигонов.
    """

    top_lines = []
    top_polygons = []

    # Общая статистика
    line_count = 0
    polygon_count = 0

    # iterparse позволяет обрабатывать OSM файл
    # без загрузки всего XML в память.
    context = ET.iterparse(
        filename,
        events=("end",)
    )

    for event, element in context:

        # Нас интересуют только элементы <way>.
        if element.tag != "way":
            continue

        osm_id = element.attrib.get("id", "?")

        # Собираем node references.
        #
        # Каждый <nd ref="..."/> соответствует одной точке
        # исходной OSM-геометрии way.
        node_refs = []

        for child in element:
            if child.tag == "nd":
                ref = child.attrib.get("ref")

                if ref is not None:
                    node_refs.append(ref)

        point_count = len(node_refs)

        if point_count < 2:
            element.clear()
            continue

        # Собираем OSM tags.
        tags = {}

        for child in element:
            if child.tag == "tag":
                key = child.attrib.get("k")
                value = child.attrib.get("v")

                if key is not None:
                    tags[key] = value

        # Замкнутый way:
        #
        # первая и последняя node reference совпадают.
        is_closed = (
            point_count >= 4
            and node_refs[0] == node_refs[-1]
        )

        geometry_type = classify_way(
            tags,
            point_count,
            is_closed
        )

        if geometry_type == "line":
            line_count += 1
            add_top(top_lines, point_count, osm_id)

        elif geometry_type == "polygon":
            polygon_count += 1
            add_top(top_polygons, point_count, osm_id)

        # Очень важно для больших стран:
        # освобождаем уже обработанный <way>.
        element.clear()

    return (
        sorted(top_lines, reverse=True),
        sorted(top_polygons, reverse=True),
        line_count,
        polygon_count,
    )


def print_table(title, objects):
    """Печатает TOP-N в удобном для чтения виде."""

    print()
    print(title)
    print("-" * len(title))

    if not objects:
        print("Нет объектов.")
        return

    print(f"{'Rank':>4}  {'Points':>10}  {'OSM ID':>15}")
    print(f"{'-' * 4}  {'-' * 10}  {'-' * 15}")

    for rank, (point_count, osm_id) in enumerate(objects, 1):
        print(
            f"{rank:>4}  "
            f"{point_count:>10,}  "
            f"{osm_id:>15}"
        )


def main():
    parser = argparse.ArgumentParser(
        description=(
            "Find TOP-10 OSM lines and polygons "
            "by number of geometry points."
        )
    )

    parser.add_argument(
        "osm_file",
        help="Input OSM XML file"
    )

    args = parser.parse_args()

    print(f"Input: {args.osm_file}")
    print("Analyzing OSM ways...")

    (
        top_lines,
        top_polygons,
        line_count,
        polygon_count,
    ) = analyze_osm(args.osm_file)

    print()
    print("Objects analyzed:")
    print(f"  Lines:    {line_count:,}")
    print(f"  Polygons: {polygon_count:,}")

    print_table(
        "TOP-10 LINES BY NUMBER OF POINTS",
        top_lines
    )

    print_table(
        "TOP-10 POLYGONS BY NUMBER OF POINTS",
        top_polygons
    )


if __name__ == "__main__":
    main()