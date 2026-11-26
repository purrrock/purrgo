#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
PurrGO map format V4 writer (docs/map-format-v4.md).

Reuses the V3 MapCompiler for .db and map.name; replaces the PGO container
(LOD 3 offset, search.idx type), .mlp (LOD 2/3 geometry variants) and .idx
(block layout, four LODs).
"""

import struct
from typing import Any, Dict, List, Optional, Sequence, Tuple

from purrgo_bin_writer import MapCompiler
from purrgo_idx_v4 import (VariantLeaf, build_section, empty_section, str_pack,
                           LEAF_FANOUT, BLOCK)
from purrgo_models import MapFeature, HWConfig
from purrgo_search import build_search_index
from purrgo_simplify import (DROPPED, UNCHANGED, simplify_line, simplify_polygon)

LOD_COUNT = HWConfig.LOD_COUNT_V4

# Variant tables: id(feature) -> {lod: Variant | None}; missing lod = full
# geometry, None = the object is absent from that LOD.
Variants = Dict[int, Dict[int, Optional[Tuple[bytes, Tuple[int, ...], Tuple[int, int, int, int]]]]]


def check_lods(features: Sequence[MapFeature], layer: str) -> None:
    for f in features:
        if not 0 <= f.lod < LOD_COUNT:
            raise ValueError(f"{layer}: object {f.osm_id} has LOD {f.lod}, "
                             f"V4 allows 0..{LOD_COUNT - 1}")


class MapCompilerV4(MapCompiler):

    @staticmethod
    def named_db_records(features: Sequence[MapFeature]) -> int:
        """Records of the layer .db after compile_db (with the dummy one); 0 - no .db."""
        named = sum(1 for f in features if f.v2 >= 2)
        return named + 1 if named else 0

    @staticmethod
    def _write_pgo_container(filepath: str, payload: bytes, file_type: int,
                             lod_offsets: Sequence[int]) -> None:
        if file_type not in (1, 2, 3, 4):
            raise ValueError(f"Invalid file_type: {file_type}")
        offs = list(lod_offsets) + [0] * (4 - len(lod_offsets))
        if len(offs) != 4 or any(o < 0 or o > 0xFFFFFFFF for o in offs):
            raise ValueError("LOD offsets must be four uint32 values")
        if len(payload) > 0xFFFFFFFF:
            raise ValueError("Payload size exceeds uint32 limit.")
        header = (b"PGO" + bytes([file_type]) + struct.pack("<I", len(payload))
                  + struct.pack("<4I", *offs) + bytes(8))
        assert len(header) == HWConfig.PGO_HEADER_SIZE
        with open(filepath, "wb") as f:
            f.write(header)
            f.write(payload)

    # ------------------------------------------------------------------
    # Geometry variants (3.5)
    # ------------------------------------------------------------------

    @staticmethod
    def build_variants(features: Sequence[MapFeature], polygons: bool,
                       tol_lod2_m: float, tol_lod3_m: float) -> Variants:
        simplify = simplify_polygon if polygons else simplify_line
        variants: Variants = {}
        for f in features:
            if f.lod < 2 or not f.points:
                continue
            entry: Dict[int, Any] = {}
            for lod, tol in ((2, tol_lod2_m), (3, tol_lod3_m)):
                if f.lod < lod:
                    break
                r = simplify(f.points, f.parts, tol)
                if r == UNCHANGED:
                    continue
                entry[lod] = None if r == DROPPED else r
            if entry:
                variants[id(f)] = entry
        return variants

    @classmethod
    def build_spatial_order_v4(cls, features: List[MapFeature]) -> None:
        """Order for .mlp/.db: by LOD class 3,2,1,0, inside a class by STR."""
        if not features:
            return
        groups: Dict[int, List[MapFeature]] = {i: [] for i in range(LOD_COUNT)}
        for f in features:
            groups[min(LOD_COUNT - 1, max(0, f.lod))].append(f)
        ordered: List[MapFeature] = []
        for lod in range(LOD_COUNT - 1, -1, -1):
            if groups[lod]:
                for node in str_pack(groups[lod], LEAF_FANOUT, 0):
                    ordered.extend(node.children)
        features.clear()
        features.extend(ordered)

    @classmethod
    def compile_mlp_v4(cls, features: List[MapFeature], filepath: str,
                       variants: Variants) -> Dict[int, Dict[int, Optional[int]]]:
        """
        Writes .mlp: LOD 3 variants, then LOD 2 variants, then full geometry
        (so every LOD reads a compact region). Returns variant v1 pointers:
        id(feature) -> {lod: v1 or None}. Sets feature.v1 for full geometry.
        """
        print(f"[>] Compiling geometry: {filepath}...")
        out = bytearray()
        number = 1
        var_v1: Dict[int, Dict[int, Optional[int]]] = {}

        def write(bbox, parts, points: bytes) -> int:
            nonlocal number
            body = bytearray(struct.pack("<iiii", *bbox))
            body += struct.pack("<II", len(parts), len(points) // 8)
            for p in parts:
                body += struct.pack("<I", p)
            body += points
            out.extend(struct.pack(">I", number) + struct.pack("<I", len(body)))
            v1 = len(out)                      # body starts right after 8-byte header
            out.extend(body)
            number += 1
            return v1

        for lod in (3, 2):
            for f in features:
                entry = variants.get(id(f))
                if not entry or lod not in entry:
                    continue
                var = entry[lod]
                slot = var_v1.setdefault(id(f), {})
                if var is None:
                    slot[lod] = None
                    continue
                # identical to the LOD 3 variant already written: reuse it
                other = entry.get(3) if lod == 2 else None
                if lod == 2 and other is not None and other == var and slot.get(3) is not None:
                    slot[2] = slot[3]
                    continue
                points, parts, bbox = var
                slot[lod] = write(bbox, parts, points)

        for f in features:
            f.v1 = write(f.bbox, f.parts, f.points)
            f.v2 = 1            # dummy .db record, as in V3
            f.mlp_size = 0
        cls._write_pgo_container(filepath, bytes(out), 2, (0, 0, 0, 0))
        return var_v1

    # ------------------------------------------------------------------
    # .idx (4-7, 14)
    # ------------------------------------------------------------------

    @classmethod
    def compile_idx_v4(cls, features: List[MapFeature], filepath: str,
                       var_v1: Optional[Dict[int, Dict[int, Optional[int]]]] = None,
                       variants: Optional[Variants] = None,
                       is_poi: bool = False) -> None:
        print(f"[>] Compiling block SQT index (V4): {filepath}...")
        var_v1 = var_v1 or {}
        variants = variants or {}
        if is_poi:
            for f in features:
                f.v1 = 0

        payload = bytearray(HWConfig.BLOCK_SIZE - HWConfig.PGO_HEADER_SIZE)  # zeros to 0x200
        offsets: List[int] = []
        for lod in range(LOD_COUNT):
            start = HWConfig.PGO_HEADER_SIZE + len(payload)
            offsets.append(start)
            leaves: List[Any] = []
            for f in features:
                if f.lod < lod:
                    continue
                if lod >= 2 and id(f) in variants and lod in variants[id(f)]:
                    var = variants[id(f)][lod]
                    if var is None:
                        continue               # object absent in this LOD
                    leaves.append(VariantLeaf(var[2], f.code,
                                              var_v1[id(f)][lod], f.v2))
                else:
                    leaves.append(f)
            payload += build_section(leaves, start) if leaves else empty_section()
        cls._write_pgo_container(filepath, bytes(payload), 1, tuple(offsets))

    @classmethod
    def create_empty_layer_v4(cls, layer_prefix: str) -> None:
        print(f"[>] Creating system dummy layers: {layer_prefix}...")
        cls._write_pgo_container(f"{layer_prefix}.mlp", b"", 2, (0, 0, 0, 0))
        payload = bytearray(HWConfig.BLOCK_SIZE - HWConfig.PGO_HEADER_SIZE)
        offsets = []
        for _ in range(LOD_COUNT):
            offsets.append(HWConfig.PGO_HEADER_SIZE + len(payload))
            payload += empty_section()
        cls._write_pgo_container(f"{layer_prefix}.idx", bytes(payload), 1, tuple(offsets))

    # ------------------------------------------------------------------
    # search.idx (21)
    # ------------------------------------------------------------------

    @classmethod
    def compile_search(cls, pois: Sequence[MapFeature], filepath: str,
                       layers: Sequence[Tuple[Sequence[MapFeature], int]] = ()) -> None:
        """
        pois: POI after compile_db. layers: [(features, db_records)] of the line and
        polygon layers after compile_db (db_records includes the dummy record 1).
        """
        print(f"[>] Compiling search index: {filepath}...")
        data = build_search_index(pois, db_records=len(pois), layers=layers)
        with open(filepath, "wb") as f:
            f.write(data)
