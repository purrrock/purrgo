#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys
import gc
import re
import array
import bisect
import itertools
import struct
from decimal import Decimal
from lxml import etree as ET
from functools import lru_cache
from typing import List, Tuple, Dict, Optional, Set

from purrgo_models import MapFeature, HWConfig, pgo_encode
from purrgo_geometry import is_clockwise
from purrgo_lookup import LookupTables


# Maximum number of points allowed in one PurrGO geometry.
#
# This is a defensive input-data limit.
# It is not a limit imposed by the MLP binary format itself.
#
# OpenStreetMap currently limits normal Ways to 1999 node references.
# If a geometry exceeds this limit, the object is considered invalid for
# PurrGO compilation and is skipped rather than being split into chunks.
MAX_GEOMETRY_POINTS = 1999


# Global immutable tuple of toponymic descriptors.
# Strictly sorted by descending length for the startswith algorithm.
_STOP_WORDS = (
    "памятная доска","restaurant", "praspiekt", "boulevard", "проспект", "переулок", "ресторан",
    "праспект", "рэстаран", "praspekt", "stancyya", "prypynak", "restaran",
    "площадь", "бульвар", "станция", "магазин", "завулак", "станцыя", "highway",
    "grocery", "station", "zavulak", "voziera", "вуліца", "плошча", "возера",
    "vulica", "plošča", "bulvar", "alieja", "skvier", "улица", "street", "avenue",
    "square", "shoppe", "market", "пр-кт", "шоссе", "аллея", "озеро", "сквер",
    "крама", "blvd.", "drive", "alley", "hotel", "river", "pr-kt", "krama",
    "кафе", "парк", "шаша", "алея", "вул.", "зав.", "кафэ", "šaša", "vul.",
    "zav.", "kafe", "road", "lane", "cafe", "shop", "mall", "lake", "ave.",
    "ул.", "пер.", "пл.", "st.", "rd.", "ln.", "dr.", "sq.", "way", "pl."
)

_sorted_stop_words = sorted(_STOP_WORDS, key=len, reverse=True)
_STOP_WORDS_RX = re.compile(
    r"^(" + "|".join(re.escape(w) for w in _sorted_stop_words) + r")\s*(.*)$",
    re.IGNORECASE
)


@lru_cache(maxsize=32768)
def _sanitize_name_cached(name: str) -> str:
    """Cached name sanitization for POI centroid extraction and UI rendering."""
    if not name:
        return ""

    name = name.strip()
    m = _STOP_WORDS_RX.match(name)
    if m:
        word, core_name = m.groups()
        core_name = core_name.strip()
        if core_name:
            core_name = core_name[0].upper() + core_name[1:]
            name = f"{core_name} {word.lower()}"

    name = name.replace(" ", "_")

    # Limit for UI (characters)
    if len(name) > 22:
        name = name[:20].rstrip('_') + ".."

    # Limit for DB .db (1 символ = 1 байт в PGO-256)
    if len(name) > 100:
        name = name[:100].rstrip('_')

    return name


# Приоритет тегов для названия объекта
_NAME_TAG_PRIORITY = (
    'short_name:ru','name:ru','short_name:be', 'name:be', 'int_name',
    'short_name:en', 'name:en', 'short_name', 'name',
)


def _resolve_name(tags: dict, rule=None, is_poi: bool = False) -> str:
    """
    Название объекта: name:ru > name:be > int_name > short_name:en >
    name:en > short_name > name.

    Если у POI названия нет, в качестве названия используется значение
    тега типа из правила (например shop=clothes -> "clothes").
    Фолбэк не проходит через стоп-слова (иначе "marketplace" -> "Place market").

    Исключение: барьеры (правила с тегом barrier=...) без названия остаются
    безымянными — тип барьера (gate, bollard, ...) в название не подставляется.
    """
    for key in _NAME_TAG_PRIORITY:
        value = tags.get(key)
        if value and value.strip():
            return _sanitize_name_cached(value)

    if (
        is_poi
        and rule is not None
        and rule.osm_tags
        and not any(k == 'barrier' for k, _ in rule.osm_tags)
    ):
        type_name = rule.osm_tags[0][1]
        if len(type_name) > 22:
            type_name = type_name[:20].rstrip('_') + ".."
        return type_name

    return ""


class OSMParser:
    """Two-pass streaming OSM parser with hybrid memory optimization (C-Arrays + lxml)."""

    RESTRICTED_ACCESS_VALUES = {'private', 'permit', 'no'}

    def __init__(self, osm_file: str):
        self.osm_file = osm_file

        # [MEMORY OPTIMIZATION 1]: Node Arrays
        self.node_ids = array.array('q')
        self.node_coords = array.array('i')

        # [MEMORY OPTIMIZATION 2]: Way Pool (Flattened coordinates to save 4-5GB of RAM)
        self.way_coords_pool = array.array('i')

        # ways_cache replaced with parallel arrays to prevent dict memory bloat
        self.way_cache_ids = array.array('q')
        self.way_cache_starts = array.array('q')
        self.way_cache_lengths = array.array('i')

        self._ways_sorted = True
        self._last_way_id = -1

        self._nodes_freed = False

        self.roads: List[MapFeature] = []
        self.landuse: List[MapFeature] = []
        self.water: List[MapFeature] = []
        self.waterways: List[MapFeature] = []
        self.pois: List[MapFeature] = []

        # Statistics for invalid oversized geometries.
        #
        # These counters are deliberately kept separate for diagnostics.
        # They do not affect the binary format.
        # Water polygons made from plain closed ways, by OSM way id.
        # A multipolygon relation (lake with islands, ...) replaces the
        # standalone polygon of its outer way: otherwise the solid polygon
        # would cover the holes and islands would be painted over.
        self._water_way_features: Dict[str, MapFeature] = {}
        self._superseded_features: Set[int] = set()

        self.skipped_oversized_roads = 0
        self.skipped_oversized_waterways = 0
        self.skipped_oversized_polygons = 0
        self.skipped_oversized_multipolygons = 0

    @staticmethod
    def _is_clockwise(points: List[Tuple[int, int]]) -> bool:
        return is_clockwise(points)

    def _analyze_road_surface(self, tags: Dict[str, str]) -> Optional[str]:
        smoothness = tags.get("smoothness")

        if smoothness in {
            "bad",
            "very_bad",
            "horrible",
            "very_horrible",
            "impassable"
        }:
            return "unpaved"

        if smoothness in {"excellent", "good", "intermediate"}:
            return "paved"

        surface = tags.get("surface")

        if surface in {
            "unpaved",
            "grass_paver",
            "sett",
            "unhewn_cobblestone",
            "cobblestone",
            "bricks",
            "metal_grid",
            "wood",
            "stepping_stones",
            "tiles",
            "fibre_reinforced_polymer_grate",
            "compacted",
            "fine_gravel",
            "gravel",
            "shells",
            "rock",
            "pebblestone",
            "ground",
            "dirt",
            "earth",
            "laterite",
            "grass",
            "mud",
            "sand",
            "woodchips",
            "snow",
            "ice",
            "salt"
        }:
            return "unpaved"

        if surface in {
            "paved",
            "asphalt",
            "chipseal",
            "concrete",
            "paving_stones",
            "metal"
        }:
            return "paved"

        return None

    def parse(
        self
    ) -> Tuple[
        List[MapFeature],
        List[MapFeature],
        List[MapFeature],
        List[MapFeature],
        List[MapFeature]
    ]:
        self._pass1_cache_nodes()
        self._pass2_build_features()

        if self._superseded_features:
            superseded = self._superseded_features
            self.water = [
                f for f in self.water if id(f) not in superseded
            ]

        self._water_way_features = {}
        self._superseded_features = set()

        return (
            self.roads,
            self.landuse,
            self.water,
            self.waterways,
            self.pois
        )

    def _get_node_coord(
        self,
        node_id: int
    ) -> Optional[Tuple[int, int]]:
        # Protection in case relation/way calls for node after nodes are freed
        if self.node_ids is None:
            return None

        idx = bisect.bisect_left(self.node_ids, node_id)

        if idx < len(self.node_ids) and self.node_ids[idx] == node_id:
            coord_idx = idx * 2
            return (
                self.node_coords[coord_idx],
                self.node_coords[coord_idx + 1]
            )

        return None

    def _pass1_cache_nodes(self) -> None:
        total_nodes_str = os.environ.get('TOTAL_NODES', None)
        total_nodes = (
            int(total_nodes_str)
            if total_nodes_str and total_nodes_str.isdigit()
            else None
        )

        if total_nodes:
            print(f"[>] Pass 1: Caching nodes... (0 / {total_nodes})")
        else:
            print("[>] Pass 1: Caching nodes...")

        gc.disable()

        context = ET.iterparse(
            self.osm_file,
            events=('end',)
        )

        node_ids_append = self.node_ids.append
        node_coords_append = self.node_coords.append

        count = 0
        is_sorted = True
        last_id = -1

        TARGET_TAGS = {'node', 'way', 'relation'}

        for event, elem in context:
            if elem.tag not in TARGET_TAGS:
                continue

            if elem.tag == 'node':
                try:
                    nid = int(elem.get('id'))

                    node_ids_append(nid)

                    node_coords_append(
                        int(Decimal(elem.get('lon')) * 10000000)
                    )
                    node_coords_append(
                        int(Decimal(elem.get('lat')) * 10000000)
                    )

                    if is_sorted and nid < last_id:
                        is_sorted = False

                    last_id = nid

                except (TypeError, ValueError):
                    pass

                count += 1

                if not (count & 0xFFFFF):
                    # sys.getsizeof gives the size of the continuous C-arrays
                    mem_mb = (
                        sys.getsizeof(self.node_ids)
                        + sys.getsizeof(self.node_coords)
                    ) / (1024 * 1024)

                    print(
                        f"\r    Nodes cached: {count:,} | "
                        f"Arrays RAM: {mem_mb:.1f} MB",
                        end="",
                        flush=True
                    )

            elif elem.tag == 'way':
                break

            elem.clear()

            while elem.getprevious() is not None:
                del elem.getparent()[0]

        if not is_sorted:
            print(
                "\r    [!] Nodes are unsorted. "
                "Indexing arrays (may take some memory)..."
            )

            indices = list(range(len(self.node_ids)))
            indices.sort(key=lambda i: self.node_ids[i])

            new_ids = array.array(
                'q',
                [0]
            ) * len(self.node_ids)

            new_coords = array.array(
                'i',
                [0]
            ) * len(self.node_coords)

            for new_i, old_i in enumerate(indices):
                new_ids[new_i] = self.node_ids[old_i]
                new_coords[new_i * 2] = self.node_coords[old_i * 2]
                new_coords[new_i * 2 + 1] = self.node_coords[old_i * 2 + 1]

            self.node_ids = new_ids
            self.node_coords = new_coords

            del indices

        gc.enable()
        gc.collect()

        print(
            f"\r    Nodes loaded into Arrays: "
            f"{len(self.node_ids):,}        "
        )

    def _pass2_build_features(self) -> None:
        print(
            "[>] Pass 2: Normalizing geometry, "
            "multipolygons and POIs..."
        )

        context = ET.iterparse(
            self.osm_file,
            events=('end',)
        )

        processors = {
            'way': self._process_way,
            'relation': self._process_relation,
            'node': self._process_node
        }

        TARGET_TAGS = {'node', 'way', 'relation'}
        count = 0

        for event, elem in context:
            if elem.tag not in TARGET_TAGS:
                continue

            # [MEMORY OPTIMIZATION 3]: EARLY NODE EJECTION
            #
            # The exact moment we hit the first relation, we know nodes/ways
            # are done. We free up RAM immediately to give relations room
            # to breathe.
            if elem.tag == 'relation' and not self._nodes_freed:
                arrays_mb = (
                    sys.getsizeof(self.node_ids)
                    + sys.getsizeof(self.node_coords)
                ) / (1024 * 1024)

                print(
                    f"\n    [!] Reaching Relations. "
                    f"Ejecting Node Cache to free ~{arrays_mb:.1f} MB...",
                    flush=True
                )

                self.node_ids = None
                self.node_coords = None

                if not self._ways_sorted:
                    print(
                        "\r    [!] Ways are unsorted. "
                        "Indexing arrays (may take some memory)...",
                        flush=True
                    )

                    indices = list(range(len(self.way_cache_ids)))
                    indices.sort(
                        key=lambda i: self.way_cache_ids[i]
                    )

                    new_ids = array.array(
                        'q',
                        [0]
                    ) * len(self.way_cache_ids)

                    new_starts = array.array(
                        'q',
                        [0]
                    ) * len(self.way_cache_starts)

                    new_lengths = array.array(
                        'i',
                        [0]
                    ) * len(self.way_cache_lengths)

                    for new_i, old_i in enumerate(indices):
                        new_ids[new_i] = self.way_cache_ids[old_i]
                        new_starts[new_i] = self.way_cache_starts[old_i]
                        new_lengths[new_i] = self.way_cache_lengths[old_i]

                    self.way_cache_ids = new_ids
                    self.way_cache_starts = new_starts
                    self.way_cache_lengths = new_lengths

                    del indices

                gc.collect()
                self._nodes_freed = True

            processor = processors.get(elem.tag)

            if processor:
                processor(elem)
                count += 1

                if not (count & 0x7FFFF):
                    ways_pool_mb = (
                        sys.getsizeof(self.way_coords_pool)
                        / (1024 * 1024)
                    )

                    features_count = (
                        len(self.roads)
                        + len(self.landuse)
                        + len(self.pois)
                    )

                    # Получение фактического использования RAM процессом
                    # (только для Linux/GitHub Actions)
                    rss_mb = 0.0

                    if sys.platform.startswith('linux'):
                        try:
                            with open(
                                f'/proc/{os.getpid()}/status',
                                'r'
                            ) as f:
                                for line in f:
                                    if line.startswith('VmRSS:'):
                                        rss_mb = (
                                            int(line.split()[1])
                                            / 1024
                                        )
                                        break
                        except Exception:
                            pass

                    mem_str = (
                        f" | Sys VmRSS: {rss_mb:.1f} MB"
                        if rss_mb
                        else ""
                    )

                    print(
                        f"\r    Processed: {count:,} | "
                        f"Features: {features_count:,} | "
                        f"WayPool: {ways_pool_mb:.1f} MB"
                        f"{mem_str}",
                        end="",
                        flush=True
                    )

            elem.clear()

            while elem.getprevious() is not None:
                del elem.getparent()[0]

        print(
            f"\r    Assembled: "
            f"{len(self.roads)} roads, "
            f"{len(self.landuse)} polygons, "
            f"{len(self.waterways)} waterways, "
            f"{len(self.pois)} points (POI).      "
        )

        # Report invalid geometries that were deliberately skipped.
        skipped_total = (
            self.skipped_oversized_roads
            + self.skipped_oversized_waterways
            + self.skipped_oversized_polygons
            + self.skipped_oversized_multipolygons
        )

        if skipped_total:
            print(
                "[!] Oversized geometries skipped:"
            )
            print(
                f"    - Roads:          "
                f"{self.skipped_oversized_roads}"
            )
            print(
                f"    - Waterways:      "
                f"{self.skipped_oversized_waterways}"
            )
            print(
                f"    - Polygons:       "
                f"{self.skipped_oversized_polygons}"
            )
            print(
                f"    - Multipolygons:  "
                f"{self.skipped_oversized_multipolygons}"
            )
            print(
                f"    - Total:          "
                f"{skipped_total}"
            )
            print(
                f"    - Limit:          "
                f"{MAX_GEOMETRY_POINTS} points"
            )

    def _extract_tags(
        self,
        elem: ET.Element
    ) -> Dict[str, str]:
        return {
            child.get('k'): child.get('v')
            for child in elem.iterchildren('tag')
            if child.get('k') and child.get('v')
        }

    def _process_node(self, elem: ET.Element) -> None:
        tags = self._extract_tags(elem)

        if not tags:
            return

        rule = LookupTables.match_feature(tags)

        if not rule:
            return

        name = _resolve_name(tags, rule, is_poi=(rule.layer == 'pois'))

        try:
            osm_id = elem.get('id')
            node_coord = self._get_node_coord(int(osm_id))

            if not node_coord:
                return

        except (TypeError, ValueError):
            return

        # points field is unused for native POIs but we'll set it
        # for backwards compatibility if needed, or just let bbox
        # take over.
        points_bytes = struct.pack(
            "<ii",
            node_coord[0],
            node_coord[1]
        )

        feature = MapFeature(
            osm_id=osm_id,
            code=rule.code,
            name=name,
            points=points_bytes,
            lod=rule.lod
        )

        feature.calculate_bbox()

        # Override bbox for POIs
        # (xmin == xmax, ymin == ymax)
        feature.bbox = (
            node_coord[0],
            node_coord[1],
            node_coord[0],
            node_coord[1]
        )

        if rule.layer == 'pois':
            self.pois.append(feature)

        elif rule.layer == 'roads':
            self.roads.append(feature)

        elif rule.layer == 'landuse':
            self.landuse.append(feature)

        elif rule.layer == 'water':
            self.water.append(feature)

    def _process_way(self, elem: ET.Element) -> None:
        tags = self._extract_tags(elem)

        if tags.get('access') in self.RESTRICTED_ACCESS_VALUES:
            if not any(
                k in tags
                for k in ('landuse', 'leisure', 'natural')
            ):
                return

        points = []

        for nd in elem.iterchildren('nd'):
            ref = nd.get('ref')

            if ref is not None:
                try:
                    coord = self._get_node_coord(int(ref))

                    if coord:
                        points.append(coord)

                except ValueError:
                    continue

        if not points:
            return

        try:
            way_id = int(elem.get('id'))

            # [C-ARRAY FLATTENING]:
            # Replaces massive List[Tuple] overhead with ultra-fast
            # contiguous memory.
            start_idx = len(self.way_coords_pool)

            self.way_coords_pool.extend(
                itertools.chain.from_iterable(points)
            )

            self.way_cache_ids.append(way_id)
            self.way_cache_starts.append(start_idx)
            self.way_cache_lengths.append(len(points))

            if self._ways_sorted and way_id < self._last_way_id:
                self._ways_sorted = False

            self._last_way_id = way_id

        except (TypeError, ValueError):
            return

        osm_id = str(way_id)

        rule = LookupTables.match_feature(tags)

        if not rule:
            return

        name = _resolve_name(tags, rule, is_poi=(rule.layer == 'pois'))

        is_closed = (
            len(points) >= 4
            and points[0] == points[-1]
        )

        if rule.layer == 'pois':
            # Way POIs logic: calculate centroid
            if is_closed:
                unique_points = points[:-1]

                avg_lon = (
                    sum(p[0] for p in unique_points)
                    // len(unique_points)
                )
                avg_lat = (
                    sum(p[1] for p in unique_points)
                    // len(unique_points)
                )
                
                
                poi_name = name

                points_bytes = struct.pack(
                    "<ii",
                    avg_lon,
                    avg_lat
                )

                poi_feature = MapFeature(
                    osm_id=f"v{osm_id}",
                    code=rule.code,
                    name=poi_name,
                    points=points_bytes,
                    lod=rule.lod
                )

                poi_feature.calculate_bbox()

                poi_feature.bbox = (
                    avg_lon,
                    avg_lat,
                    avg_lon,
                    avg_lat
                )

                self.pois.append(poi_feature)

            return

        # ------------------------------------------------------------------
        # Defensive OSM geometry limit.
        #
        # We do NOT split the Way.
        #
        # A Way with more than 2000 points is considered invalid for
        # PurrGO compilation and is simply omitted from the resulting map.
        #
        # The Way remains in the internal way cache because it may still
        # be referenced by a multipolygon relation.
        # ------------------------------------------------------------------
        if len(points) > MAX_GEOMETRY_POINTS:
            if rule.layer == 'roads':
                self.skipped_oversized_roads += 1

            elif rule.layer == 'waterways':
                self.skipped_oversized_waterways += 1

            elif rule.layer in ('landuse', 'water'):
                self.skipped_oversized_polygons += 1

            return

        if rule.layer == 'roads' and len(points) >= 2:
            points_bytes = self.way_coords_pool[
                start_idx:start_idx + len(points) * 2
            ].tobytes()

            feature = MapFeature(
                osm_id=osm_id,
                code=rule.code,
                name=name,
                points=points_bytes,
                lod=rule.lod
            )

            feature.calculate_bbox()

            self.roads.append(feature)

        elif rule.layer == 'waterways' and len(points) >= 2:
            # Linear water objects (rivers, streams, canals).
            #
            # Same geometry as roads: an open polyline, stored as is
            # (no ring orientation, no closing). A closed waterway
            # (rare, e.g. a canal loop) is still a line.
            points_bytes = self.way_coords_pool[
                start_idx:start_idx + len(points) * 2
            ].tobytes()

            feature = MapFeature(
                osm_id=osm_id,
                code=rule.code,
                name=name,
                points=points_bytes,
                lod=rule.lod
            )

            feature.calculate_bbox()

            self.waterways.append(feature)

        elif (
            rule.layer in ('landuse', 'water')
            and len(points) >= 4
        ):
            if is_closed:
                if not self._is_clockwise(points):
                    points.reverse()

                points_bytes = b''.join(
                    struct.pack("<ii", p[0], p[1])
                    for p in points
                )

                feature = MapFeature(
                    osm_id=osm_id,
                    code=rule.code,
                    name=name,
                    points=points_bytes,
                    lod=rule.lod
                )

                feature.calculate_bbox()

                if rule.layer == 'water':
                    self.water.append(feature)
                    self._water_way_features[osm_id] = feature
                else:
                    self.landuse.append(feature)

    # ------------------------------------------------------------------
    # Multipolygon helpers
    # ------------------------------------------------------------------

    @staticmethod
    def _assemble_rings(
        ways: List[List[Tuple[int, int]]]
    ) -> List[List[Tuple[int, int]]]:
        """Join member ways into closed rings.

        A ring of a multipolygon is often split into several ways that
        share end nodes. Closed ways are taken as they are, open ways are
        chained by matching end points (either direction). Chains that do
        not close (incomplete data at the edge of the extract) are dropped.
        """
        rings: List[List[Tuple[int, int]]] = []
        open_ways: List[List[Tuple[int, int]]] = []

        for way in ways:
            if len(way) < 2:
                continue

            if way[0] == way[-1]:
                if len(way) >= 4:
                    rings.append(list(way))
            else:
                open_ways.append(list(way))

        while open_ways:
            chain = open_ways.pop()
            progress = True

            while chain[0] != chain[-1] and progress:
                progress = False

                for i, way in enumerate(open_ways):
                    if way[0] == chain[-1]:
                        chain.extend(way[1:])
                    elif way[-1] == chain[-1]:
                        chain.extend(reversed(way[:-1]))
                    elif way[-1] == chain[0]:
                        chain = way[:-1] + chain
                    elif way[0] == chain[0]:
                        chain = list(reversed(way[1:])) + chain
                    else:
                        continue

                    open_ways.pop(i)
                    progress = True
                    break

            if chain[0] == chain[-1] and len(chain) >= 4:
                rings.append(chain)

        return rings

    @staticmethod
    def _point_in_ring(
        pt: Tuple[int, int],
        ring: List[Tuple[int, int]]
    ) -> bool:
        """Even-odd ray casting test."""
        x, y = pt
        inside = False
        n = len(ring)
        j = n - 1

        for i in range(n):
            xi, yi = ring[i]
            xj, yj = ring[j]

            if (yi > y) != (yj > y):
                x_cross = xi + (xj - xi) * (y - yi) / (yj - yi)

                if x < x_cross:
                    inside = not inside

            j = i

        return inside

    @classmethod
    def _ring_inside(
        cls,
        inner: List[Tuple[int, int]],
        outer: List[Tuple[int, int]]
    ) -> bool:
        # An island may touch the outline of the lake at a vertex, so a
        # single vertex can fall on the border. Several vertices are tried.
        step = max(1, len(inner) // 4)

        for k in range(0, len(inner) - 1, step):
            if cls._point_in_ring(inner[k], outer):
                return True

        return False

    def _member_way_points(
        self,
        ref: int
    ) -> Optional[List[Tuple[int, int]]]:
        idx = bisect.bisect_left(self.way_cache_ids, ref)

        if idx >= len(self.way_cache_ids) or self.way_cache_ids[idx] != ref:
            return None

        start_idx = self.way_cache_starts[idx]
        length = self.way_cache_lengths[idx]
        flat = self.way_coords_pool[start_idx:start_idx + length * 2]

        return [
            (flat[i], flat[i + 1])
            for i in range(0, length * 2, 2)
        ]

    def _orient_rings(
        self,
        outers: List[List[Tuple[int, int]]],
        inners: List[List[Tuple[int, int]]]
    ) -> Tuple[List[Tuple[int, int]], List[int]]:
        """Flatten rings: outers clockwise first, then inners counter-clockwise."""
        combined: List[Tuple[int, int]] = []
        parts: List[int] = []

        for ring in outers:
            ring = list(ring)
            if not self._is_clockwise(ring):
                ring.reverse()
            parts.append(len(combined))
            combined.extend(ring)

        for ring in inners:
            ring = list(ring)
            if self._is_clockwise(ring):
                ring.reverse()
            parts.append(len(combined))
            combined.extend(ring)

        return combined, parts

    def _emit_multipolygon(
        self,
        layer: str,
        osm_id: str,
        code: int,
        name: str,
        lod: int,
        outers: List[List[Tuple[int, int]]],
        inners: List[List[Tuple[int, int]]]
    ) -> Optional[MapFeature]:
        combined, parts = self._orient_rings(outers, inners)

        # Defensive OSM geometry limit for multipolygons: num_points in the
        # MLP record is the total over all parts. A multipolygon is not split.
        if len(combined) > MAX_GEOMETRY_POINTS:
            self.skipped_oversized_multipolygons += 1
            return None

        points_bytes = b''.join(
            struct.pack("<ii", p[0], p[1])
            for p in combined
        )

        feature = MapFeature(
            osm_id=osm_id,
            code=code,
            name=name,
            points=points_bytes,
            parts=parts,
            lod=lod
        )

        feature.calculate_bbox()

        if layer == 'water':
            self.water.append(feature)
        else:
            self.landuse.append(feature)

        return feature

    def _process_relation(self, elem: ET.Element) -> None:
        tags = self._extract_tags(elem)

        if tags.get('type') != 'multipolygon':
            return

        rule = LookupTables.match_feature(tags)

        if rule and rule.layer not in ('landuse', 'water'):
            return

        osm_id = elem.get('id')

        if not osm_id:
            return

        members = list(elem.iterchildren('member'))

        if rule is None:
            # Untagged multipolygons are only interesting when the outer way
            # is a water polygon already emitted (old-style tagging).
            if not self._water_way_features:
                return

            if not any(
                m.get('type') == 'way'
                and m.get('role') != 'inner'
                and m.get('ref') in self._water_way_features
                for m in members
            ):
                return

        # Collect member ways. An empty role is an outer ring.
        outer_ways: List[List[Tuple[int, int]]] = []
        outer_refs: List[str] = []
        inner_ways: List[List[Tuple[int, int]]] = []

        for member in members:
            if member.get('type') != 'way' or member.get('ref') is None:
                continue

            try:
                ref = int(member.get('ref'))
            except ValueError:
                continue

            way_points = self._member_way_points(ref)

            if way_points is None:
                continue

            if member.get('role') == 'inner':
                inner_ways.append(way_points)
            else:
                outer_ways.append(way_points)
                outer_refs.append(member.get('ref'))

        outers = self._assemble_rings(outer_ways)

        if not outers:
            # No closed outer ring (incomplete data): do not keep islands
            # alone, they would be filled as ordinary polygons.
            return

        inners = [
            ring
            for ring in self._assemble_rings(inner_ways)
            if any(self._ring_inside(ring, outer) for outer in outers)
        ]

        if rule is not None:
            # Tagged relation: one feature with all rings.
            feature = self._emit_multipolygon(
                rule.layer,
                osm_id,
                rule.code,
                _resolve_name(tags, rule),
                rule.lod,
                outers,
                inners
            )

            if feature is not None and rule.layer == 'water':
                for ref in outer_refs:
                    standalone = self._water_way_features.get(ref)

                    if standalone is not None:
                        self._superseded_features.add(id(standalone))

            return

        # Old-style multipolygon: the relation carries no tags, they stay
        # on the outer way. The way was already emitted as a solid polygon;
        # when the relation adds holes, replace it with a polygon that has
        # them.
        if not inners:
            return

        for ref in outer_refs:
            standalone = self._water_way_features.get(ref)

            if standalone is None or id(standalone) in self._superseded_features:
                continue

            own_ring = self._member_way_points(int(ref))

            if own_ring is None or len(own_ring) < 4 or own_ring[0] != own_ring[-1]:
                continue

            holes = [
                ring for ring in inners
                if self._ring_inside(ring, own_ring)
            ]

            if not holes:
                continue

            feature = self._emit_multipolygon(
                'water',
                standalone.osm_id,
                standalone.code,
                standalone.name,
                standalone.lod,
                [own_ring],
                holes
            )

            if feature is not None:
                self._superseded_features.add(id(standalone))
