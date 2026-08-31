#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import json
import math
import struct
from typing import List, Tuple, Any

from purrgo_models import MapFeature, RTreeNode, HWConfig, pgo_encode
from purrgo_lookup import LookupTables


class MapCompiler:
    """Generator of  binary structures (PGO/SQT/DBF)."""

    @staticmethod
    def _write_pgo_container(filepath: str, payload: bytes, file_type: int, lod_offsets: Tuple[int, int, int]) -> None:
        """Encapsulate data in the new V3 PGO container."""
        if file_type not in (1, 2, 3):
            raise ValueError(f"Invalid file_type: {file_type}. Must be 1, 2, or 3.")

        payload_size = len(payload)
        if payload_size > 0xFFFFFFFF:
            raise ValueError("Payload size exceeds uint32 limit.")

        for offset in lod_offsets:
            if offset < 0 or offset > 0xFFFFFFFF:
                raise ValueError("LOD offset must fit in uint32.")

        header = bytearray(b'PGO')
        header.append(file_type)
        header.extend(struct.pack("<I", payload_size))
        header.extend(struct.pack("<I", lod_offsets[0]))
        header.extend(struct.pack("<I", lod_offsets[1]))
        header.extend(struct.pack("<I", lod_offsets[2]))
        header.extend(struct.pack("<I", 0)) # Future extension 1
        header.extend(struct.pack("<I", 0)) # Future extension 2
        header.extend(struct.pack("<I", 0)) # Future extension 3

        if len(header) != HWConfig.PGO_HEADER_SIZE:
            raise ValueError(f"Generated header size {len(header)} != 32")

        with open(filepath, 'wb') as f:
            f.write(header)
            f.write(payload)

    @staticmethod
    def _pad(text: Any, length: int) -> bytes:
        """Pad text to fixed length using PGO-256 encoding."""
        if isinstance(text, bytes):
            return text[:length].ljust(length, b'\x00')
        return pgo_encode(text, length)

    @staticmethod
    def _desc(name: str, length: int) -> bytes:
        """Pack dBase III field descriptor."""
        return name.encode('ascii')[:11].ljust(11, b'\x00') + b'C' + b'\x00' * 4 + bytes([length]) + b'\x00' * 15

    @classmethod
    def compile_mlp(cls, features: List[MapFeature], filepath: str) -> None:
        """Serializes raw geometry points into the .mlp binary format."""
        print(f"[>] Compiling geometry: {filepath}...")
        bin_records = bytearray()
        record_number = 1

        for feature in features:
            body = bytearray(struct.pack("<iiii", feature.bbox[0], feature.bbox[1], feature.bbox[2], feature.bbox[3]))
            body += struct.pack("<II", len(feature.parts), len(feature.points) // 8)

            for part_idx in feature.parts:
                body += struct.pack("<I", part_idx)
            body += feature.points

            header = struct.pack(">I", record_number) + struct.pack("<I", len(body))
            record_bin = header + body

            feature.v1 = len(bin_records) + 8
            feature.v2 = 1
            feature.mlp_size = len(record_bin)

            bin_records += record_bin
            record_number += 1

        cls._write_pgo_container(filepath, bin_records, file_type=2, lod_offsets=(0, 0, 0))

    @classmethod
    def compile_db(cls, features: List[MapFeature], filepath: str, is_poi: bool = False) -> None:
        """Serializes text attributes into a dBase III (.db) format encapsulated in PGO."""
        if not is_poi and not any(f.name for f in features):
            print(f"[~] Layer {filepath} contains no named objects. .db file creation skipped.")
            for f in features:
                f.v2 = 0
            return
        if is_poi and not features:
            return

        print(f"[>] Compiling attributes: {filepath}...")

        bin_records = bytearray() if is_poi else bytearray(b'\x00' * HWConfig.DBF_RECORD_LEN)
        db_counter = 1 if is_poi else 2
        total_records = 0 if is_poi else 1

        for feature in features:
            if is_poi or feature.name:
                feature.v2 = db_counter
                db_counter += 1
                total_records += 1

                r_bytes = bytearray(b'\x20')
                r_bytes += cls._pad(feature.osm_id, 12) + cls._pad(str(feature.code), 4) + cls._pad(feature.name, 100)
                bin_records += r_bytes

        dbf_header = (
            bytearray(b'\x03\x00\x00\x00')
            + struct.pack('<I', total_records)
            + struct.pack('<H', HWConfig.DBF_HEADER_LEN)
            + struct.pack('<H', HWConfig.DBF_RECORD_LEN)
            + b'\x00' * 20
            + cls._desc("osm_id", 12)
            + cls._desc("code", 4)
            + cls._desc("name", 100)
            + b'\x0D'
        )
        cls._write_pgo_container(filepath, dbf_header + bin_records, file_type=3, lod_offsets=(0, 0, 0))

    @classmethod
    def _build_str_layer(cls, items: List[Any], level: int) -> List[RTreeNode]:
        """
        Sort-Tile-Recursive (STR) Bulk Loading Algorithm.
        Divides a flat array into square matrices (Tiles) for hardware Z-Culling.
        """
        if not items:
            return []

        # 1. Sort objects by X axis (Centroid longitude)
        items.sort(key=lambda item: (item.bbox[0] + item.bbox[2]) / 2.0)

        # 2. Calculate mathematical slice limits
        num_nodes = math.ceil(len(items) / HWConfig.CHUNK_SIZE)
        num_slices = math.ceil(math.sqrt(num_nodes))

        if num_slices == 0:
            return []

        slice_capacity = num_slices * HWConfig.CHUNK_SIZE

        nodes = []
        # 3. Cut vertical slices and sort them by Y axis (Centroid latitude)
        for i in range(0, len(items), slice_capacity):
            slc = items[i:i + slice_capacity]
            slc.sort(key=lambda item: (item.bbox[1] + item.bbox[3]) / 2.0)

            # 4. Pack into C-Union clusters (CHUNK_SIZE elements each)
            for j in range(0, len(slc), HWConfig.CHUNK_SIZE):
                chunk = slc[j:j + HWConfig.CHUNK_SIZE]
                nodes.append(RTreeNode(level=level, children=chunk))

        return nodes

    @classmethod
    def _build_rtree(cls, features: List[MapFeature]) -> Tuple[int, List[RTreeNode]]:
        """
        Recursively builds a tree hierarchy of Macro-nodes (Nav Nodes).
        Returns: (Tree depth, List of root macro-nodes)
        """
        if not features:
            return 0, []

        current_layer = features
        level = 0

        while True:
            current_layer = cls._build_str_layer(current_layer, level)
            level += 1
            # Stop if the entire layer fits into one array of root nodes
            if len(current_layer) <= HWConfig.CHUNK_SIZE:
                break

        return level, current_layer

    @classmethod
    def compile_idx(cls, features: List[MapFeature], filepath: str, is_poi: bool = False) -> None:
        """Serializes the multi-level SQT hardware index using R-Tree Hierarchy."""
        print(f"[>] Compiling Hierarchical SQT index: {filepath}...")
        idx_buffer = bytearray()

        if is_poi:
            for f in features:
                f.v1 = 0

        # Standard multi-level GIS geometry (LOD 0, 1, 2)
        lod_filters = [
            lambda f: f.lod >= 0,
            lambda f: f.lod >= 1,
            lambda f: f.lod >= 2
        ]

        lod_offsets = [0, 0, 0]
        prev_len = -1
        cached_packed = b""
        for lod_index, condition in enumerate(lod_filters):
            lod_offsets[lod_index] = HWConfig.PGO_HEADER_SIZE + len(idx_buffer)
            lod_records = [f for f in features if condition(f)]

            if not lod_records:
                idx_buffer.extend(b'SQT\x01\x01\x00\x00\x00' + struct.pack("<II", 0, 0))
            else:
                if len(lod_records) == prev_len:
                    idx_buffer.extend(cached_packed)
                else:
                    depth, root_nodes = cls._build_rtree(lod_records)

                    # Dynamic recording of a 16-byte SQT header
                    header = b'SQT\x01\x01\x00\x00\x00' + struct.pack("<II", depth, len(root_nodes))
                    packed_data = bytearray(header)
                    for node in root_nodes:
                        packed_data.extend(node.pack())

                    idx_buffer.extend(packed_data)

                    cached_packed = packed_data
                    prev_len = len(lod_records)

        cls._write_pgo_container(filepath, idx_buffer, file_type=1, lod_offsets=tuple(lod_offsets))

    @classmethod
    def create_empty_layer(cls, layer_prefix: str) -> None:
        """Generates system dummy layers for missing geometry types."""
        print(f"[>] Creating system dummy layers: {layer_prefix}...")

        cls._write_pgo_container(f"{layer_prefix}.mlp", b'', file_type=2, lod_offsets=(0, 0, 0))

        idx_buffer = bytearray()
        lod_offsets = [0, 0, 0]
        empty_sqt = b'SQT\x01\x01\x00\x00\x00' + struct.pack("<II", 0, 0)

        for i in range(3):
            lod_offsets[i] = HWConfig.PGO_HEADER_SIZE + len(idx_buffer)
            idx_buffer.extend(empty_sqt)

        cls._write_pgo_container(f"{layer_prefix}.idx", idx_buffer, file_type=1, lod_offsets=tuple(lod_offsets))

    @staticmethod
    def create_map_name(name: str, meta_records: List[MapFeature], out_file: str = "map.name") -> None:
        """Generates the JSON camera centering file."""
        if not meta_records:
            # Для пустой карты устанавливаем координаты по умолчанию
            center_lat, center_lon = 53.52351455, 28.4119479
        else:
            center_lat = (min(r.bbox[1] for r in meta_records) + max(r.bbox[3] for r in meta_records)) / 2.0 / 10000000.0
            center_lon = (min(r.bbox[0] for r in meta_records) + max(r.bbox[2] for r in meta_records)) / 2.0 / 10000000.0
            
        with open(out_file, "w", encoding="utf-8") as f:
            json.dump({"centerLat": center_lat, "centerLon": center_lon, "mapName": name}, f, separators=(',', ':'))