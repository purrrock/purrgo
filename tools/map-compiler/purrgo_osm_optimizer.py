#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
PurrGO OSM optimizer.

Назначение:
    Подготовить XML OSM перед передачей в PurrGO map compiler.

Вход:
    OSM XML, полученный из PBF, например через:

        osmium cat region.osm.pbf -o region.osm
либо скачаный с openstreetmap.org

Выход:
    Уменьшенный OSM XML, в котором:

    1. Удалены ненужные metadata и "мусорные" теги.
    2. Удалены здания и другие объекты, которые не нужны PurrGO.
    3. Здания с POI-тегами преобразуются в отдельные точечные POI.
    4. Сохраняются только nodes, реально используемые оставшимися Ways,
       плюс standalone POI nodes.

Таким образом pipeline выглядит так:

    OSM PBF
       |
       v
    osmium -> OSM XML
       |
       v
    этот optimizer
       |
       |  удаление мусора / ненужных объектов
       |  очистка тегов
       |  извлечение POI
       |
       v
    optimized.osm
       |
       v
    purrgo_osmparser.py
       |
       |  geometry > 2000 points -> skip
       |
       v
    PurrGO map compiler
"""

import osmium as o
import sys
import os
import tempfile
import shutil

from xml.sax.saxutils import escape
from typing import Any
from lxml import etree as ET

# ---------------------------------------------------------------------------
# PyOsmium handler
# ---------------------------------------------------------------------------
#
# PyOsmium выполняет первый проход по исходному OSM.
#
# На этом этапе мы:
#
#   - анализируем tags;
#   - определяем, нужен ли объект PurrGO;
#   - получаем координаты Way;
#   - записываем отфильтрованные Ways во временный файл;
#   - записываем сгенерированные POI nodes в другой временный файл;
#   - собираем множество node ID, которые понадобятся во втором проходе.
#
# Важно:
#
# Здесь НЕ выполняется изменение геометрии.
#
# Каждая исходная последовательность node references Way записывается
# полностью и в том же порядке.
# ---------------------------------------------------------------------------


class WayOptimizer(o.SimpleHandler):
    """
    PyOsmium handler для фильтрации и подготовки OSM Ways.

    Несмотря на историческое имя WayOptimizer, этот класс больше
    не выполняет геометрическую оптимизацию.

    Его задача сейчас:
        - фильтрация объектов;
        - очистка тегов;
        - извлечение POI;
        - перенос исходной геометрии Way без изменений.
    """

    def __init__(
        self,
        temp_ways_file: str,
        temp_nodes_file: str
    ) -> None:
        super().__init__()

        # Временный файл для подготовленных Ways.
        self.tmp_f = open(temp_ways_file, 'wb')

        # Временный файл для виртуальных nodes,
        # созданных из зданий с POI-тегами.
        self.tmp_nodes_f = open(temp_nodes_file, 'wb')

        # Node IDs, которые действительно нужны оставшимся Ways.
        #
        # Второй проход по исходному OSM использует этот set,
        # чтобы не копировать в результат миллионы неиспользуемых nodes.
        self.used_node_ids = set()

        # Количество Ways, записанных в optimized OSM.
        self.ways_count = 0

        # Количество зданий, преобразованных в точечные POI.
        self.converted_pois_count = 0

        # ------------------------------------------------------------------
        # Triggers for object deletion.
        #
        # Объект с одним из этих тегов может быть удалён,
        # если одновременно не имеет другого тега, показывающего,
        # что он нужен map compiler.
        # ------------------------------------------------------------------
        self.drop_way_triggers = {
            'building',
            'power'
        }

        # Безусловно удаляемые комбинации key=value.
        #
        # Эти объекты не используются как геометрические объекты карты.
        self.drop_way_kv = {
            'highway': {
                'corridor',
                'elevator'
            }
        }

        # ------------------------------------------------------------------
        # Survival keys.
        #
        # Если Way имеет хотя бы один такой key, он считается потенциально
        # полезным для PurrGO и не удаляется только из-за наличия
        # drop_way_trigger.
        # ------------------------------------------------------------------
        self.survival_keys = {
            'landuse',
            'natural',
            'amenity',
            'leisure',
            'tourism',
            'shop',
            'sport',
            'highway',
            'waterway',
            'barrier',
            'railway',
            'aeroway',
            'man_made',
            'historic',
            'route'
        }

        # ------------------------------------------------------------------
        # POI keys.
        #
        # Если объект одновременно является building/power и содержит
        # один из этих tags, его геометрия здания не сохраняется.
        #
        # Вместо неё создаётся виртуальный OSM node в центре объекта.
        # ------------------------------------------------------------------
        self.poi_keys = {
            'amenity',
            'shop',
            'leisure',
            'tourism',
            'sport',
            'historic',
            'craft',
            'office',
            'healthcare',
            'emergency'
        }

        # ------------------------------------------------------------------
        # Tags, которые не нужны PurrGO и удаляются из результата.
        #
        # Здесь находятся metadata и атрибуты, которые не используются
        # текущим PurrGO map compiler.
        # ------------------------------------------------------------------
        self.drop_tag_keys = {
            'wikidata',
            'wikipedia',
            'phone',
            'website',
            'url',
            'opening_hours',
            'email',
            'maxspeed',
            'lanes',
            'oneway',
            'note',
            'source',
            'fixme',
            'building',
            'power',
            'operator',
            'start_date'
        }

        # Удаляем также целые группы тегов по prefix.
        #
        # Например:
        #
        #     addr:street
        #     addr:housenumber
        #     contact:phone
        #
        # Они не нужны для текущего формата карты PurrGO.
        self.drop_tag_prefixes = (
            'addr:',
            'contact:',
            'payment:',
            'source:',
            'generator:',
            'plant:'
        )

    def way(self, w: Any) -> None:
        """
        Обработать один OSM Way.

        Основной принцип этой функции:

            исходная геометрия -> фильтрация -> запись целиком

        Здесь намеренно НЕТ:

            - Douglas-Peucker;
            - resampling;
            - decimation;
            - chunking;
            - генерации новых Way ID для частей.

        Если Way сохраняется, его node references записываются полностью.
        """

        has_drop_trigger = False
        has_survival_tag = False
        has_poi_tag = False
        is_linear_highway = False

        # Сюда попадут только теги, которые разрешено сохранить
        # в optimized OSM.
        valid_tags = []

        # ------------------------------------------------------------------
        # Анализ tags.
        # ------------------------------------------------------------------

        for tag in w.tags:

            # 1. Безусловное удаление отдельных комбинаций key=value.
            #
            # Например:
            #
            #     highway=corridor
            #     highway=elevator
            #
            # Такие объекты вообще не рассматриваются дальше.
            if (
                tag.k in self.drop_way_kv
                and tag.v in self.drop_way_kv[tag.k]
            ):
                return

            # 2. Запоминаем наличие trigger, например building или power.
            if tag.k in self.drop_way_triggers:
                has_drop_trigger = True

            # 3. Запоминаем наличие полезного для карты key.
            if tag.k in self.survival_keys:
                has_survival_tag = True

            # 4. Проверяем, может ли объект быть POI.
            if tag.k in self.poi_keys:
                has_poi_tag = True

            # highway используется для определения линейного объекта.
            if tag.k == 'highway':
                is_linear_highway = True

            # 5. Собираем очищенный набор tags.
            #
            # Здесь не удаляем теги из исходного PyOsmium объекта.
            # Вместо этого создаём новый список только из разрешённых tags.
            if (
                tag.k not in self.drop_tag_keys
                and not tag.k.startswith(self.drop_tag_prefixes)
            ):
                valid_tags.append((tag.k, tag.v))

        # ------------------------------------------------------------------
        # Получение геометрии Way.
        # ------------------------------------------------------------------
        #
        # locations=True в apply_file() гарантирует, что PyOsmium
        # предоставляет координаты nodes.
        #
        # InvalidLocationError означает, что координата конкретного node
        # отсутствует. Такой node просто не включается в geometry.
        #
        # Для нормального OSM Way это не должно происходить.
        # ------------------------------------------------------------------

        pts = []
        valid_nds = []

        for n in w.nodes:
            try:
                pts.append(
                    (
                        n.location.lon,
                        n.location.lat
                    )
                )

                valid_nds.append(n.ref)

            except o.InvalidLocationError:
                pass

        # Если после очистки tags ничего не осталось или geometry пустая,
        # объект не имеет смысла для PurrGO.
        if not valid_tags or len(pts) == 0:
            return

        # ------------------------------------------------------------------
        # POI extraction.
        # ------------------------------------------------------------------
        #
        # Например, building + shop=... .
        #
        # В этом случае нам не нужна геометрия всего здания.
        # Создаём вместо него одну виртуальную точку в математическом центре.
        #
        # Геометрия здания в optimized Ways не записывается.
        # ------------------------------------------------------------------

        if has_drop_trigger and has_poi_tag:

            # Среднее значение longitude/latitude всех точек.
            #
            # Это именно тот алгоритм, который использовался
            # исходной версией optimizer.
            center_lon = (
                sum(p[0] for p in pts)
                / len(pts)
            )

            center_lat = (
                sum(p[1] for p in pts)
                / len(pts)
            )

            # ID виртуального node.
            #
            # Смещение используется для того, чтобы не пересекаться
            # с обычными OSM node IDs.
            node_id = 20000000000 + w.id

            xml_str = (
                f'  <node id="{node_id}" '
                f'version="1" visible="true" '
                f'lat="{center_lat:.6f}" '
                f'lon="{center_lon:.6f}">\n'
            )

            for k, v in valid_tags:
                v_esc = escape(
                    v,
                    entities={'"': '&quot;'}
                )

                xml_str += (
                    f'    <tag k="{k}" v="{v_esc}"/>\n'
                )

            xml_str += '  </node>\n'

            self.tmp_nodes_f.write(
                xml_str.encode('utf-8')
            )

            self.converted_pois_count += 1

            # Здание уже представлено точкой.
            # Полигон здания в карту не переносим.
            return

        # ------------------------------------------------------------------
        # Удаление объекта, который имеет drop trigger, но не имеет
        # ни одного survival key.
        #
        # Например, обычный building без интересующих нас tags.
        # ------------------------------------------------------------------

        if has_drop_trigger and not has_survival_tag:
            return

        # ------------------------------------------------------------------
        # Определение типа геометрии.
        # ------------------------------------------------------------------
        #
        # Здесь переменная is_polygon нужна только для того, чтобы
        # сохранить замыкание polygon.
        #
        # Важно: никакой simplification здесь больше нет.
        #
        # Важно: никакого chunking здесь больше нет.
        # ------------------------------------------------------------------

        is_polygon = (
            w.is_closed()
            and not is_linear_highway
        )

        # ------------------------------------------------------------------
        # Для polygon OSM Way должен быть замкнут.
        #
        # Обычно w.is_closed() уже означает, что первый и последний
        # node совпадают. Но сохраняем существующую защиту:
        # если references по какой-либо причине не замкнуты,
        # добавляем первый node в конец.
        #
        # Это единственное изменение количества node references,
        # которое этот optimizer выполняет для polygon.
        #
        # Оно не является simplification или chunking.
        # ------------------------------------------------------------------

        if (
            is_polygon
            and valid_nds[0] != valid_nds[-1]
        ):
            valid_nds.append(valid_nds[0])
            pts.append(pts[0])

        # ------------------------------------------------------------------
        # Записываем BBox исходного Way.
        #
        # BBox сохраняется как атрибут XML Way.
        # Текущий PurrGO parser использует собственный bbox после чтения
        # geometry, но мы сохраняем эту часть исходного optimizer,
        # чтобы не менять промежуточный OSM format без необходимости.
        # ------------------------------------------------------------------

        lons = [p[0] for p in pts]
        lats = [p[1] for p in pts]

        min_lon = min(lons)
        max_lon = max(lons)
        min_lat = min(lats)
        max_lat = max(lats)

        # ------------------------------------------------------------------
        # Сейчас один OSM Way -> один Way в optimized OSM.
        # ------------------------------------------------------------------

        way_id = w.id

        xml_str = (
            f'  <way id="{way_id}" '
            f'version="1" visible="true" '
            f'min_lon="{min_lon:.6f}" '
            f'max_lon="{max_lon:.6f}" '
            f'min_lat="{min_lat:.6f}" '
            f'max_lat="{max_lat:.6f}">\n'
        )

        # ------------------------------------------------------------------
        # Записываем ВСЕ node references Way.
        #
        # Здесь нет:
        #
        #     simplified_nds
        #     selected_indices
        #     chunks
        #     chunk IDs
        #
        # valid_nds содержит полную исходную последовательность.
        # ------------------------------------------------------------------

        for nd_ref in valid_nds:
            xml_str += (
                f'    <nd ref="{nd_ref}"/>\n'
            )

            # Node понадобится во втором проходе.
            self.used_node_ids.add(nd_ref)

        # ------------------------------------------------------------------
        # Записываем очищенные tags.
        # ------------------------------------------------------------------

        for k, v in valid_tags:
            v_esc = escape(
                v,
                entities={'"': '&quot;'}
            )

            xml_str += (
                f'    <tag k="{k}" v="{v_esc}"/>\n'
            )

        xml_str += '  </way>\n'

        self.tmp_f.write(
            xml_str.encode('utf-8')
        )

        self.ways_count += 1

    def close(self) -> None:
        """
        Закрыть временные файлы.

        PyOsmium вызывает way() много раз, поэтому файлы остаются
        открытыми на протяжении всего первого прохода.
        """

        self.tmp_f.close()
        self.tmp_nodes_f.close()


# ---------------------------------------------------------------------------
# Очистка metadata и tags у исходных Node/Relation.
# ---------------------------------------------------------------------------

def clean_element_metadata(elem: ET.Element) -> None:
    """
    Удалить ненужные metadata и tags из Node или Relation.

    Эта функция используется во втором проходе, когда мы копируем
    элементы из исходного OSM XML в результирующий файл.
    """

    # ------------------------------------------------------------------
    # OSM editing metadata.
    #
    # Они не нужны PurrGO map compiler.
    # ------------------------------------------------------------------

    for attr in [
        'timestamp',
        'changeset',
        'uid',
        'user'
    ]:
        elem.attrib.pop(attr, None)

    # ------------------------------------------------------------------
    # Если этих атрибутов нет, добавляем стандартные значения,
    # как делал исходный optimizer.
    # ------------------------------------------------------------------

    if 'version' not in elem.attrib:
        elem.set('version', '1')

    if 'visible' not in elem.attrib:
        elem.set('visible', 'true')

    # ------------------------------------------------------------------
    # Tags, которые не нужны PurrGO.
    # ------------------------------------------------------------------

    drop_keys = {
        'wikidata',
        'wikipedia',
        'building',
        'power',
        'phone',
        'website',
        'url',
        'opening_hours',
        'email',
        'maxspeed',
        'lanes',
        'oneway',
        'note',
        'source',
        'fixme',
        'operator',
        'start_date'
    }

    # ------------------------------------------------------------------
    # Целые группы тегов.
    # ------------------------------------------------------------------

    drop_prefixes = (
        'addr:',
        'contact:',
        'payment:',
        'source:',
        'generator:',
        'plant:'
    )

    # ------------------------------------------------------------------
    # Удаляем tags непосредственно из XML element.
    # ------------------------------------------------------------------

    for tag in elem.findall('tag'):
        k = tag.get('k', '')

        if (
            k in drop_keys
            or k.startswith(drop_prefixes)
        ):
            elem.remove(tag)


# ---------------------------------------------------------------------------
# Основная функция оптимизации.
# ---------------------------------------------------------------------------

def optimize_osm_pyosmium(
    input_file: str,
    output_file: str
) -> None:
    """
    Подготовить OSM XML для PurrGO.

    Первый проход:
        PyOsmium читает OSM и формирует:
            - подготовленные Ways;
            - виртуальные POI nodes;
            - множество используемых node IDs.

    Второй проход:
        lxml/ElementTree читает исходный XML и формирует окончательный
        optimized XML:

            bounds
            nodes
            generated POI nodes
            prepared Ways
            relations

    Геометрия Ways на этом этапе не упрощается и не разбивается.
    """

    # ------------------------------------------------------------------
    # Временный файл подготовленных Ways.
    #
    # NamedTemporaryFile используется только как способ получить
    # безопасное уникальное имя файла. Сам дескриптор закрываем сразу,
    # потому что PyOsmium handler открывает файл самостоятельно.
    # ------------------------------------------------------------------

    temp_ways = tempfile.NamedTemporaryFile(
        delete=False,
        mode='wb'
    )

    temp_ways_name = temp_ways.name
    temp_ways.close()

    # ------------------------------------------------------------------
    # Временный файл виртуальных POI nodes.
    # ------------------------------------------------------------------

    temp_nodes = tempfile.NamedTemporaryFile(
        delete=False,
        mode='wb'
    )

    temp_nodes_name = temp_nodes.name
    temp_nodes.close()

    try:
        # ==============================================================
        # PHASE 1
        # ==============================================================

        print(
            "[*] Phase 1: PyOsmium starting C++ engine "
            "to filter OSM objects..."
        )

        handler = WayOptimizer(
            temp_ways_name,
            temp_nodes_name
        )

        # locations=True:
        #
        # PyOsmium разрешает node references Way в реальные координаты.
        #
        # idx='flex_mem':
        #
        # Используется существующий режим индексации PyOsmium.
        handler.apply_file(
            input_file,
            locations=True,
            idx='flex_mem'
        )

        handler.close()

        # ------------------------------------------------------------------
        # После завершения PyOsmium сохраняем результаты первого прохода.
        # ------------------------------------------------------------------

        used_node_ids = handler.used_node_ids
        ways_count = handler.ways_count
        converted_pois = handler.converted_pois_count

        print(
            f"    ... PyOsmium analysis complete! "
            f"Found {len(used_node_ids)} valid nodes, "
            f"{ways_count} ways."
        )

        print(
            f"    ... Extracted {converted_pois} POIs "
            f"from building polygons."
        )

        # ==============================================================
        # PHASE 2
        # ==============================================================

        print(
            "[*] Phase 2: Using iterparse to reconstruct "
            "the final XML file at high speed..."
        )

        with open(
            output_file,
            'wb'
        ) as out:

            # ----------------------------------------------------------
            # XML header.
            # ----------------------------------------------------------

            out.write(
                b'<?xml version="1.0" encoding="UTF-8"?>\n'
                b'<osm version="0.6">\n'
            )

            # ----------------------------------------------------------
            # Streaming parser.
            #
            # Мы не загружаем весь OSM XML в память.
            # Каждый element обрабатывается и затем освобождается.
            # ----------------------------------------------------------

            context = ET.iterparse(
                input_file,
                events=('start', 'end')
            )

            context = iter(context)

            # Первый event должен быть start osm.
            _, root = next(context)

            # ----------------------------------------------------------
            # Подготовленные Ways и generated POI nodes нужно вставить
            # только один раз.
            #
            # Они вставляются перед первым relation.
            # Это соответствует структуре OSM:
            #
            #     nodes
            #     ways
            #     relations
            #
            # ----------------------------------------------------------

            ways_written = False

            for event, elem in context:

                if event == 'end':

                    # --------------------------------------------------
                    # Bounds копируем без дополнительной обработки.
                    # --------------------------------------------------

                    if elem.tag == 'bounds':
                        out.write(
                            ET.tostring(
                                elem,
                                encoding='utf-8'
                            )
                            + b'\n'
                        )

                    # --------------------------------------------------
                    # Обычные Node.
                    #
                    # Копируем только:
                    #
                    #   1. nodes, используемые оставшимися Ways;
                    #   2. nodes, имеющие собственные tags.
                    #
                    # Второй случай сохраняет standalone POI и другие
                    # точечные объекты, присутствующие в исходном OSM.
                    # --------------------------------------------------

                    elif elem.tag == 'node':

                        clean_element_metadata(elem)

                        node_id = int(
                            elem.get('id')
                        )

                        if (
                            node_id in used_node_ids
                            or len(elem.findall('tag')) > 0
                        ):
                            out.write(
                                ET.tostring(
                                    elem,
                                    encoding='utf-8'
                                )
                                + b'\n'
                            )

                    # --------------------------------------------------
                    # Way или Relation.
                    # --------------------------------------------------

                    elif elem.tag in (
                        'way',
                        'relation'
                    ):

                        if not ways_written:

                            # --------------------------------------------------
                            # Сначала добавляем виртуальные POI nodes.
                            # --------------------------------------------------

                            print(
                                "    ... Injecting extracted "
                                "POI nodes..."
                            )

                            with open(
                                temp_nodes_name,
                                'rb'
                            ) as tn:

                                shutil.copyfileobj(
                                    tn,
                                    out
                                )

                            # --------------------------------------------------
                            # Затем добавляем подготовленные Ways.
                            #
                            # Каждый Way здесь является полной геометрией
                            # исходного Way. Chunking отсутствует.
                            # --------------------------------------------------

                            print(
                                "    ... Seamlessly merging "
                                "prepared ways..."
                            )

                            with open(
                                temp_ways_name,
                                'rb'
                            ) as tw:

                                shutil.copyfileobj(
                                    tw,
                                    out
                                )

                            ways_written = True

                        # --------------------------------------------------
                        # Relations оставляем из исходного OSM.
                        #
                        # Здесь удаляются только ненужные metadata/tags.
                        #
                        # Обработка multipolygon geometry выполняется
                        # уже purrgo_osmparser.py.
                        # --------------------------------------------------

                        if elem.tag == 'relation':

                            clean_element_metadata(
                                elem
                            )

                            # Relation без tags не нужен текущему pipeline.
                            if len(elem.findall('tag')) > 0:
                                out.write(
                                    ET.tostring(
                                        elem,
                                        encoding='utf-8'
                                    )
                                    + b'\n'
                                )

                    # --------------------------------------------------
                    # Освобождаем уже обработанный element.
                    #
                    # Это критично для больших региональных OSM файлов.
                    # --------------------------------------------------

                    if elem.tag in (
                        'node',
                        'way',
                        'relation',
                        'bounds'
                    ):

                        elem.clear()
                        root.clear()

            # ----------------------------------------------------------
            # Fallback.
            #
            # Если исходный XML вообще не содержит Way/Relation,
            # временные данные всё равно должны попасть в результат.
            # ----------------------------------------------------------

            if not ways_written:

                with open(
                    temp_nodes_name,
                    'rb'
                ) as tn:

                    shutil.copyfileobj(
                        tn,
                        out
                    )

                with open(
                    temp_ways_name,
                    'rb'
                ) as tw:

                    shutil.copyfileobj(
                        tw,
                        out
                    )

            # ----------------------------------------------------------
            # XML footer.
            # ----------------------------------------------------------

            out.write(
                b'</osm>\n'
            )

    finally:
        # ------------------------------------------------------------------
        # Удаляем временные файлы даже в случае исключения.
        #
        # Это безопаснее, чем удалять их только после успешной компиляции.
        # ------------------------------------------------------------------

        for temp_name in (
            temp_ways_name,
            temp_nodes_name
        ):
            try:
                os.remove(temp_name)
            except FileNotFoundError:
                pass

    # ----------------------------------------------------------------------
    # Итоговая статистика.
    # ----------------------------------------------------------------------

    print(
        "[*] Optimization Summary:"
    )

    print(
        f"    - Nodes kept: "
        f"{len(used_node_ids)} "
        f"(plus standalone/extracted POIs)"
    )

    print(
        f"    - Extracted POIs: "
        f"{converted_pois}"
    )

    print(
        f"    - Ways copied without simplification/chunking: "
        f"{ways_count}"
    )

    print(
        "[+] OSM preprocessing complete."
    )


# ---------------------------------------------------------------------------
# Command-line entry point.
# ---------------------------------------------------------------------------

if __name__ == "__main__":

    # Значения по умолчанию для запуска без аргументов.
    input_osm = "map.osm"
    output_osm = "map_optimized.osm"

    # При наличии двух аргументов:
    #
    #     python osm_optimizer.py input.osm output.osm
    #
    # используем указанные пути.
    if len(sys.argv) == 3:
        input_osm = sys.argv[1]
        output_osm = sys.argv[2]

    # Проверяем наличие входного файла до запуска PyOsmium.
    if not os.path.exists(input_osm):
        print(
            f"[-] File not found: {input_osm}"
        )
        sys.exit(1)

    optimize_osm_pyosmium(
        input_osm,
        output_osm
    )