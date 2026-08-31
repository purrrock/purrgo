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
from typing import List, Tuple, Dict, Optional

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
    "restaurant", "praspiekt", "boulevard", "проспект", "переулок", "ресторан",
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
        self.pois: List[MapFeature] = []

        # Statistics for invalid oversized geometries.
        #
        # These counters are deliberately kept separate for diagnostics.
        # They do not affect the binary format.
        self.skipped_oversized_roads = 0
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
        List[MapFeature]
    ]:
        self._pass1_cache_nodes()
        self._pass2_build_features()

        return self.roads, self.landuse, self.water, self.pois

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
            f"{len(self.pois)} points (POI).      "
        )

        # Report invalid geometries that were deliberately skipped.
        skipped_total = (
            self.skipped_oversized_roads
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

        raw_name = (
            tags.get('short_name:en')
            or tags.get('int_name')
            or tags.get('name:en')
            or tags.get('short_name')
            or tags.get('name')
            or ""
        )

        name = _sanitize_name_cached(raw_name)

        # отключил замену имени на тег
        # if not name:
        #    name = str(rule.pg_class)

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

        raw_name = (
            tags.get('short_name:en')
            or tags.get('int_name')
            or tags.get('name:en')
            or tags.get('short_name')
            or tags.get('name')
            or ""
        )

        name = _sanitize_name_cached(raw_name)
        osm_id = str(way_id)

        rule = LookupTables.match_feature(tags)

        if not rule:
            return

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
                # отключил подстановку тега вместо имени
                # poi_name = (
                    # name
                    # if name
                    # else str(rule.pg_class)
                # )

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
                else:
                    self.landuse.append(feature)

    def _process_relation(self, elem: ET.Element) -> None:
        tags = self._extract_tags(elem)

        if tags.get('type') != 'multipolygon':
            return

        rule = LookupTables.match_feature(tags)

        if not rule or rule.layer not in ('landuse', 'water'):
            return

        raw_name = (
            tags.get('short_name:en')
            or tags.get('int_name')
            or tags.get('name:en')
            or tags.get('short_name')
            or tags.get('name')
            or ""
        )

        name = _sanitize_name_cached(raw_name)

        combined_points = []
        parts = []
        current_index = 0

        members = list(elem.iterchildren('member'))

        sorted_members = (
            [
                m
                for m in members
                if m.get('role', 'outer') == 'outer'
            ]
            +
            [
                m
                for m in members
                if m.get('role', 'outer') == 'inner'
            ]
        )

        for member in sorted_members:
            if (
                member.get('type') == 'way'
                and member.get('ref') is not None
            ):
                try:
                    ref = int(member.get('ref'))
                    role = member.get('role', 'outer')

                    idx = bisect.bisect_left(
                        self.way_cache_ids,
                        ref
                    )

                    if (
                        idx < len(self.way_cache_ids)
                        and self.way_cache_ids[idx] == ref
                    ):
                        # [C-ARRAY UNFLATTENING]:
                        # Reconstructing geometries at memory-safe speed.
                        start_idx = self.way_cache_starts[idx]
                        length = self.way_cache_lengths[idx]
                        end_idx = start_idx + (length * 2)

                        flat_coords = self.way_coords_pool[
                            start_idx:end_idx
                        ]

                        ring_points = [
                            (
                                flat_coords[i],
                                flat_coords[i + 1]
                            )
                            for i in range(
                                0,
                                length * 2,
                                2
                            )
                        ]

                        if (
                            len(ring_points) >= 4
                            and ring_points[0] == ring_points[-1]
                        ):
                            is_cw = self._is_clockwise(
                                ring_points
                            )

                            if (
                                role == 'outer'
                                and not is_cw
                            ):
                                ring_points.reverse()

                            elif (
                                role == 'inner'
                                and is_cw
                            ):
                                ring_points.reverse()

                            parts.append(current_index)
                            combined_points.extend(ring_points)
                            current_index += len(ring_points)

                except ValueError:
                    continue

        osm_id = elem.get('id')

        if not combined_points or not parts or not osm_id:
            return

        # ------------------------------------------------------------------
        # Defensive OSM geometry limit for multipolygons.
        #
        # num_points in the PurrGO MLP geometry record represents the total
        # number of points in all parts. Therefore the check is performed
        # after all rings have been flattened into combined_points.
        #
        # We deliberately do not split the multipolygon.
        # ------------------------------------------------------------------
        if len(combined_points) > MAX_GEOMETRY_POINTS:
            self.skipped_oversized_multipolygons += 1
            return

        points_bytes = b''.join(
            struct.pack("<ii", p[0], p[1])
            for p in combined_points
        )

        feature = MapFeature(
            osm_id=osm_id,
            code=rule.code,
            name=name,
            points=points_bytes,
            parts=parts,
            lod=rule.lod
        )

        feature.calculate_bbox()

        if rule.layer == 'water':
            self.water.append(feature)
        else:
            self.landuse.append(feature)