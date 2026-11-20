#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
PurrGO Map Format V4 — Binary Fixture Auditor

Audits all compiled map fixtures below a directory.

The version of every .idx file is detected by the SQT magic: V4 files
are audited by map-format-v4.md, legacy V3 files (SQT\\x01) by
map-format-v3.md. All other files are common to both versions.

Current V4 format (docs/map-format-v4.md):

    PGO
      ├── IDX
      │    ├── LOD 0 .. LOD 3
      │    └── SQT section (512-byte header sector + 512-byte blocks)
      │          ├── root block (Navigation Nodes)
      │          ├── Navigation blocks, level by level
      │          └── leaf blocks (Data Nodes)
      │
      ├── MLP
      │    └── Geometry Records (full geometry + simplified variants)
      │
      ├── DB
      │    └── DBF
      │
      └── search.idx (optional; only the PGO header is checked for now)

The auditor intentionally follows the current PurrGO V4 writer
and binary-format specification.

Important current semantics:

    Data Node:
        <iiiiBII>
        25 bytes

    Navigation Node:
        <IiiiiII>
        28 bytes

        child_block:
            absolute file offset of the block with the children,
            multiple of 512, greater than the offset of its own block.

    SQT header sector:
        512 bytes, magic SQT\\x02, topology 2.

    Blocks:
        512 bytes; at most 18 Navigation Nodes or 20 Data Nodes;
        stored level by level, child blocks of one block are consecutive.

    LODs:
        four sections; LOD n holds the objects with features.csv LOD >= n.
        LOD 2 and LOD 3 may reference simplified geometry variants.

    MLP local header:
        >I + <I
        8 bytes

    MLP v1:
        relative to the beginning of the MLP payload
        and points to the Geometry Body.

        absolute_body_offset = 32 + v1

    DB standard layer:
        v2 = 0  -> no DB exists
        v2 = 1  -> dummy record
        v2 >= 2 -> physical named record

    DB POI layer:
        v2 >= 1 -> physical record (every POI has a record)

    map.name:
        UTF-8 JSON:
            {
                "centerLat": number,
                "centerLon": number,
                "mapName": string
            }

Only Python standard library is used.
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
from collections import Counter, deque
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Tuple


# ============================================================================
# Constants
# ============================================================================

PGO_HEADER_SIZE = 32
SQT_HEADER_SIZE = 512
SQT_HEADER_SIZE_V3 = 16
BLOCK_SIZE = 512

LOD_COUNT = 4
FIRST_LOD_OFFSET = 0x200

NAV_FANOUT = 18
LEAF_FANOUT = 20

DATA_NODE_SIZE = 25
NAV_NODE_SIZE = 28

MLP_LOCAL_HEADER_SIZE = 8
MLP_BODY_FIXED_SIZE = 24

DBF_HEADER_SIZE = 129
DBF_RECORD_SIZE = 117
DBF_DESCRIPTOR_SIZE = 32
DBF_FIELD_COUNT = 3

MAX_UINT32 = 0xFFFFFFFF

FILE_TYPE_IDX = 1
FILE_TYPE_MLP = 2
FILE_TYPE_DB = 3
FILE_TYPE_SEARCH = 4

PGO_MAGIC = b"PGO"
SQT_MAGIC = b"SQT\x02"
SQT_MAGIC_V3 = b"SQT\x01"
SQT_TOPOLOGY_BLOCK = 2

DBF_MAGIC = 0x03
DBF_HEADER_TERMINATOR = 0x0D
DBF_RECORD_ACTIVE = 0x20

EXPECTED_DB_FIELDS = (
    ("osm_id", 12),
    ("code", 4),
    ("name", 100),
)

MAP_NAME_REQUIRED_FIELDS = (
    "centerLat",
    "centerLon",
    "mapName",
)


# ============================================================================
# Result structures
# ============================================================================

@dataclass
class AuditResult:
    passed: bool = True
    errors: List[str] = field(default_factory=list)
    warnings: List[str] = field(default_factory=list)

    def fail(self, message: str) -> None:
        self.passed = False
        self.errors.append(message)

    def warn(self, message: str) -> None:
        self.warnings.append(message)


@dataclass
class PGOHeader:
    file_type: int
    payload_size: int
    lod_offsets: Tuple[int, ...]
    extension_fields: Tuple[int, ...]
    version: int = 4


@dataclass
class DataNode:
    offset: int
    xmin: int
    ymin: int
    xmax: int
    ymax: int
    feature_type: int
    v1: int
    v2: int
    lod: int = -1


@dataclass
class NavNode:
    offset: int
    xmin: int
    ymin: int
    xmax: int
    ymax: int
    level: int
    child_count: int
    children: List[object] = field(default_factory=list)
    lod: int = -1
    child_block: int = 0
    v3_jump: int = 0


@dataclass
class SQTInfo:
    offset: int
    depth: int
    root_count: int
    section_size: int = 0
    block_count: int = 0
    roots: List[object] = field(default_factory=list)


@dataclass
class GeometryRecord:
    record_offset: int
    sequence: int
    content_length: int
    body_offset: int

    xmin: int
    ymin: int
    xmax: int
    ymax: int

    num_parts: int
    num_points: int

    parts: List[int]
    points: List[Tuple[int, int]]

    record_size: int


@dataclass
class DBRecord:
    index: int
    offset: int
    active: bool
    osm_id: bytes
    code: bytes
    name: bytes


@dataclass
class DBInfo:
    record_count: int
    header_size: int
    record_size: int
    fields: List[Tuple[str, int]]
    records: Dict[int, DBRecord]


@dataclass
class IndexAuditData:
    version: int = 4
    data_nodes: List[DataNode] = field(default_factory=list)
    nav_nodes: List[NavNode] = field(default_factory=list)
    sqts: List[SQTInfo] = field(default_factory=list)


@dataclass
class LayerStats:
    idx_size: int = 0
    mlp_size: int = 0
    db_size: int = 0

    sqt_count: int = 0
    nav_count: int = 0
    data_count: int = 0

    mlp_records: int = 0
    db_records: int = 0

    lod_stats: List[Tuple[int, int, int]] = field(default_factory=list)


# ============================================================================
# Binary helpers
# ============================================================================

def u16(data: bytes, offset: int) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def i32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<i", data, offset)[0]


def be_u32(data: bytes, offset: int) -> int:
    return struct.unpack_from(">I", data, offset)[0]


def checked_add(
    a: int,
    b: int,
    what: str,
    result: AuditResult,
) -> Optional[int]:
    if a < 0 or b < 0:
        result.fail(
            "{}: negative operand".format(what)
        )
        return None

    value = a + b

    if value > MAX_UINT32:
        result.fail(
            "{}: uint32 overflow ({} + {})".format(
                what,
                a,
                b,
            )
        )
        return None

    return value


def checked_mul(
    a: int,
    b: int,
    what: str,
    result: AuditResult,
) -> Optional[int]:
    if a < 0 or b < 0:
        result.fail(
            "{}: negative operand".format(what)
        )
        return None

    value = a * b

    if value > MAX_UINT32:
        result.fail(
            "{}: uint32 overflow ({} * {})".format(
                what,
                a,
                b,
            )
        )
        return None

    return value


def require_range(
    data: bytes,
    offset: int,
    size: int,
    what: str,
    result: AuditResult,
) -> bool:
    if offset < 0:
        result.fail(
            "{}: negative offset {}".format(
                what,
                offset,
            )
        )
        return False

    if size < 0:
        result.fail(
            "{}: negative size {}".format(
                what,
                size,
            )
        )
        return False

    if offset > len(data):
        result.fail(
            "{}: offset={} beyond file_size={}".format(
                what,
                offset,
                len(data),
            )
        )
        return False

    if size > len(data) - offset:
        result.fail(
            "{}: out of bounds "
            "(offset={}, size={}, file_size={})".format(
                what,
                offset,
                size,
                len(data),
            )
        )
        return False

    return True


# ============================================================================
# PGO header
# ============================================================================

def detect_idx_version(data: bytes) -> int:
    """
    The version of an .idx file is identified by the magic of the
    LOD 0 SQT section: SQT\\x01 is V3, SQT\\x02 is V4
    (map-format-v4.md, section 0.3). Anything else is checked as V4
    and reported there.
    """

    if len(data) >= PGO_HEADER_SIZE:
        lod0 = u32(data, 8)

        if (
            PGO_HEADER_SIZE <= lod0
            and lod0 + 4 <= len(data)
            and data[lod0:lod0 + 4] == SQT_MAGIC_V3
        ):
            return 3

    return 4


def parse_pgo_header(
    data: bytes,
    expected_type: int,
    result: AuditResult,
) -> Optional[PGOHeader]:
    """
    Parse and validate the 32-byte PGO header.
    """

    if len(data) < PGO_HEADER_SIZE:
        result.fail(
            "PGO header: file is shorter than {} bytes".format(
                PGO_HEADER_SIZE
            )
        )
        return None

    if data[0:3] != PGO_MAGIC:
        result.fail(
            "PGO header: invalid magic {!r}, expected b'PGO'".format(
                data[0:3]
            )
        )
        return None

    file_type = data[3]

    if file_type not in (
        FILE_TYPE_IDX,
        FILE_TYPE_MLP,
        FILE_TYPE_DB,
        FILE_TYPE_SEARCH,
    ):
        result.fail(
            "PGO header: invalid file type {}".format(
                file_type
            )
        )
        return None

    if file_type != expected_type:
        result.fail(
            "PGO header: file type={}, expected={}".format(
                file_type,
                expected_type,
            )
        )
        return None

    payload_size = u32(data, 4)
    actual_payload_size = len(data) - PGO_HEADER_SIZE

    if payload_size != actual_payload_size:
        result.fail(
            "PGO header: Payload Size={}, actual file payload={}".format(
                payload_size,
                actual_payload_size,
            )
        )

    version = 4

    if expected_type == FILE_TYPE_IDX:
        version = detect_idx_version(data)

    if version == 3:
        # V3: three LOD offsets, three reserved fields.
        lod_offsets = (
            u32(data, 8),
            u32(data, 12),
            u32(data, 16),
        )

        extension_fields = (
            u32(data, 20),
            u32(data, 24),
            u32(data, 28),
        )
    else:
        lod_offsets = (
            u32(data, 8),
            u32(data, 12),
            u32(data, 16),
            u32(data, 20),
        )

        extension_fields = (
            u32(data, 24),
            u32(data, 28),
        )

    for index, value in enumerate(
        extension_fields,
        start=1,
    ):
        if value != 0:
            result.fail(
                "PGO header: reserved field {} is {}, "
                "expected 0".format(
                    index,
                    value,
                )
            )

    if expected_type == FILE_TYPE_IDX:
        if version == 3:
            validate_idx_lod_offsets_v3(
                data,
                lod_offsets,
                result,
            )
        else:
            validate_idx_lod_offsets(
                data,
                lod_offsets,
                result,
            )
    else:
        if lod_offsets != (0, 0, 0, 0):
            result.fail(
                "PGO header: non-IDX file has LOD offsets {}, "
                "expected (0, 0, 0, 0)".format(
                    lod_offsets
                )
            )

    return PGOHeader(
        file_type=file_type,
        payload_size=payload_size,
        lod_offsets=lod_offsets,
        extension_fields=extension_fields,
        version=version,
    )


def validate_idx_lod_offsets(
    data: bytes,
    lod_offsets: Tuple[int, ...],
    result: AuditResult,
) -> None:
    """
    LOD offsets are absolute offsets from the beginning of the file.

    V4 rules (map-format-v4.md, sections 1.3 and 4.1):

        - LOD 0 offset is 0x200;
        - every offset is a multiple of 512;
        - offsets are strictly increasing, four LODs;
        - the file size is a multiple of 512;
        - bytes 0x20..0x1FF are zero.

    Each offset points to a 512-byte SQT header sector.
    """

    payload_end = len(data)

    if len(data) % BLOCK_SIZE != 0:
        result.fail(
            "IDX: file size {} is not a multiple of {}".format(
                len(data),
                BLOCK_SIZE,
            )
        )

    if len(data) >= FIRST_LOD_OFFSET:
        if any(data[PGO_HEADER_SIZE:FIRST_LOD_OFFSET]):
            result.fail(
                "IDX: padding bytes 0x{:X}..0x{:X} are not zero".format(
                    PGO_HEADER_SIZE,
                    FIRST_LOD_OFFSET - 1,
                )
            )

    if lod_offsets[0] != FIRST_LOD_OFFSET:
        result.fail(
            "IDX LOD 0: offset={}, expected 0x{:X}".format(
                lod_offsets[0],
                FIRST_LOD_OFFSET,
            )
        )

    for index, offset in enumerate(lod_offsets):
        if offset % BLOCK_SIZE != 0:
            result.fail(
                "IDX LOD {}: offset={} is not a multiple of {}".format(
                    index,
                    offset,
                    BLOCK_SIZE,
                )
            )

        if offset < FIRST_LOD_OFFSET:
            result.fail(
                "IDX LOD {}: offset={} is before 0x{:X}".format(
                    index,
                    offset,
                    FIRST_LOD_OFFSET,
                )
            )
            continue

        if offset >= payload_end:
            result.fail(
                "IDX LOD {}: offset={} is outside file".format(
                    index,
                    offset,
                )
            )
            continue

        if offset + SQT_HEADER_SIZE > payload_end:
            result.fail(
                "IDX LOD {}: SQT header sector at offset={} "
                "does not fit in file".format(
                    index,
                    offset,
                )
            )

    for index in range(len(lod_offsets) - 1):
        if lod_offsets[index] >= lod_offsets[index + 1]:
            result.fail(
                "IDX LOD offsets are not strictly increasing: {}".format(
                    lod_offsets
                )
            )
            break


# ============================================================================
# IDX / SQT
# ============================================================================

def parse_idx(
    data: bytes,
    result: AuditResult,
) -> Optional[IndexAuditData]:
    """
    Parse all LOD sections: four in V4, three in V3.
    """

    header = parse_pgo_header(
        data,
        FILE_TYPE_IDX,
        result,
    )

    if header is None:
        return None

    audit = IndexAuditData(version=header.version)

    lod_offsets = header.lod_offsets

    lod_ends = lod_offsets[1:] + (len(data),)

    for lod_index in range(len(lod_offsets)):
        if header.version == 3:
            nav_before = len(audit.nav_nodes)
            data_before = len(audit.data_nodes)

            parse_sqt_v3(
                data=data,
                offset=lod_offsets[lod_index],
                section_end=lod_ends[lod_index],
                lod_index=lod_index,
                audit=audit,
                result=result,
            )

            for node in audit.nav_nodes[nav_before:]:
                node.lod = lod_index

            for node in audit.data_nodes[data_before:]:
                node.lod = lod_index
        else:
            parse_sqt(
                data=data,
                offset=lod_offsets[lod_index],
                section_end=lod_ends[lod_index],
                lod_index=lod_index,
                audit=audit,
                result=result,
            )

    return audit


def parse_sqt(
    data: bytes,
    offset: int,
    section_end: int,
    lod_index: int,
    audit: IndexAuditData,
    result: AuditResult,
) -> Optional[SQTInfo]:
    """
    Parse one complete V4 SQT section.

    Header sector (512 bytes):

        0x00  magic SQT\\x02
        0x04  topology = 2
        0x08  depth
        0x0C  root_count
        0x10  block size = 512
        0x14  section size
        0x18  nav_fanout = 18   (uint16)
        0x1A  leaf_fanout = 20  (uint16)
        0x1C  zeros

    Blocks follow, in level order (map-format-v4.md, section 7.3).
    The traversal is breadth-first, which is exactly the group order
    of the specification: the k-th child block must start at
    section_start + 1024 + 512 * k.
    """

    context = "LOD {}".format(lod_index)

    if section_end < offset:
        result.fail(
            "{}: section end {} before start {}".format(
                context,
                section_end,
                offset,
            )
        )
        return None

    if not require_range(
        data,
        offset,
        SQT_HEADER_SIZE,
        "{} SQT header sector".format(context),
        result,
    ):
        return None

    if offset + SQT_HEADER_SIZE > section_end:
        result.fail(
            "{}: SQT header sector crosses LOD boundary".format(
                context
            )
        )
        return None

    header = data[offset:offset + SQT_HEADER_SIZE]

    if header[0:4] == SQT_MAGIC_V3:
        result.fail(
            "{}: SQT magic is V3 (SQT\\x01) in a V4 file "
            "(mixed versions in one .idx)".format(context)
        )
        return None

    if header[0:4] != SQT_MAGIC:
        result.fail(
            "{}: invalid SQT magic {!r}".format(
                context,
                header[0:4],
            )
        )
        return None

    topology = u32(header, 4)
    depth = u32(header, 8)
    root_count = u32(header, 12)
    block_size = u32(header, 16)
    section_size = u32(header, 20)
    nav_fanout = u16(header, 24)
    leaf_fanout = u16(header, 26)

    if topology != SQT_TOPOLOGY_BLOCK:
        result.fail(
            "{}: unsupported SQT topology={}, expected {}".format(
                context,
                topology,
                SQT_TOPOLOGY_BLOCK,
            )
        )

    if block_size != BLOCK_SIZE:
        result.fail(
            "{}: block size={}, expected {}".format(
                context,
                block_size,
                BLOCK_SIZE,
            )
        )

    if section_size != section_end - offset:
        result.fail(
            "{}: section size={}, but LOD boundary gives {}".format(
                context,
                section_size,
                section_end - offset,
            )
        )

    if section_size % BLOCK_SIZE != 0:
        result.fail(
            "{}: section size={} is not a multiple of {}".format(
                context,
                section_size,
                BLOCK_SIZE,
            )
        )

    if nav_fanout != NAV_FANOUT:
        result.fail(
            "{}: nav_fanout={}, expected {}".format(
                context,
                nav_fanout,
                NAV_FANOUT,
            )
        )

    if leaf_fanout != LEAF_FANOUT:
        result.fail(
            "{}: leaf_fanout={}, expected {}".format(
                context,
                leaf_fanout,
                LEAF_FANOUT,
            )
        )

    if any(header[28:]):
        result.fail(
            "{}: reserved bytes of the SQT header sector are not zero".format(
                context
            )
        )

    sqt = SQTInfo(
        offset=offset,
        depth=depth,
        root_count=root_count,
        section_size=section_size,
    )

    audit.sqts.append(sqt)

    if depth == 0:
        if root_count != 0:
            result.fail(
                "{}: depth=0 but root_count={}".format(
                    context,
                    root_count,
                )
            )

        if section_end - offset != SQT_HEADER_SIZE:
            result.fail(
                "{}: empty LOD must consist of the header sector only, "
                "section size={}".format(
                    context,
                    section_end - offset,
                )
            )

        return sqt

    if root_count < 1 or root_count > NAV_FANOUT:
        result.fail(
            "{}: root_count={} is outside 1..{}".format(
                context,
                root_count,
                NAV_FANOUT,
            )
        )
        return sqt

    if section_end - offset < SQT_HEADER_SIZE + BLOCK_SIZE:
        result.fail(
            "{}: section has no root block".format(context)
        )
        return sqt

    # (block offset, level of the entries, entry count, is Navigation,
    #  parent Navigation Node or None for the root block)
    queue = deque()

    queue.append(
        (
            offset + SQT_HEADER_SIZE,
            depth - 1,
            root_count,
            True,
            None,
        )
    )

    next_block = offset + SQT_HEADER_SIZE + BLOCK_SIZE
    block_count = 0
    broken = False

    while queue and not broken:
        (
            block_offset,
            level,
            count,
            is_nav,
            parent,
        ) = queue.popleft()

        block_count += 1

        block_end = block_offset + BLOCK_SIZE

        if block_end > section_end or not require_range(
            data,
            block_offset,
            BLOCK_SIZE,
            "{} block @{}".format(context, block_offset),
            result,
        ):
            result.fail(
                "{}: block @{} is outside the section".format(
                    context,
                    block_offset,
                )
            )
            break

        entry_size = NAV_NODE_SIZE if is_nav else DATA_NODE_SIZE

        used = count * entry_size

        if any(data[block_offset + used:block_end]):
            result.fail(
                "{}: block @{}: bytes after the last entry "
                "are not zero".format(
                    context,
                    block_offset,
                )
            )

        for entry_index in range(count):
            entry_offset = block_offset + entry_index * entry_size

            entry_context = "{} block @{} {}[{}]".format(
                context,
                block_offset,
                "NAV" if is_nav else "DATA",
                entry_index,
            )

            if is_nav:
                node = parse_nav_node(
                    data=data,
                    offset=entry_offset,
                    block_end=block_end,
                    context=entry_context,
                    audit=audit,
                    result=result,
                    expected_level=level,
                )

                if node is None:
                    broken = True
                    break

                node.lod = lod_index

                register_child(
                    parent,
                    node,
                    sqt,
                    entry_context,
                    result,
                )

                limit = LEAF_FANOUT if node.level == 0 else NAV_FANOUT

                if node.child_count < 1 or node.child_count > limit:
                    result.fail(
                        "{}: child_count={} is outside 1..{}".format(
                            entry_context,
                            node.child_count,
                            limit,
                        )
                    )
                    broken = True
                    break

                child = node.child_block

                if child % BLOCK_SIZE != 0:
                    result.fail(
                        "{}: child_block={} is not a multiple "
                        "of {}".format(
                            entry_context,
                            child,
                            BLOCK_SIZE,
                        )
                    )
                    broken = True
                    break

                if (
                    child < offset + SQT_HEADER_SIZE + BLOCK_SIZE
                    or child + BLOCK_SIZE > section_end
                ):
                    result.fail(
                        "{}: child_block={} is outside the blocks of "
                        "the section [{}, {})".format(
                            entry_context,
                            child,
                            offset + SQT_HEADER_SIZE + BLOCK_SIZE,
                            section_end,
                        )
                    )
                    broken = True
                    break

                if child <= block_offset:
                    result.fail(
                        "{}: child_block={} is not after its own "
                        "block @{}".format(
                            entry_context,
                            child,
                            block_offset,
                        )
                    )
                    broken = True
                    break

                if child != next_block:
                    result.fail(
                        "{}: child_block={}, expected {} "
                        "(level order of blocks, section 7.3)".format(
                            entry_context,
                            child,
                            next_block,
                        )
                    )
                    broken = True
                    break

                next_block += BLOCK_SIZE

                queue.append(
                    (
                        child,
                        level - 1 if level > 0 else -1,
                        node.child_count,
                        level > 0,
                        node,
                    )
                )

            else:
                node, _ = parse_data_node(
                    data=data,
                    offset=entry_offset,
                    section_end=block_end,
                    context=entry_context,
                    audit=audit,
                    result=result,
                )

                if node is None:
                    broken = True
                    break

                node.lod = lod_index

                register_child(
                    parent,
                    node,
                    sqt,
                    entry_context,
                    result,
                )

    sqt.block_count = block_count

    if not broken and next_block != section_end:
        result.fail(
            "{}: blocks end at {}, but the section ends at {}; "
            "unreferenced bytes={}".format(
                context,
                next_block,
                section_end,
                section_end - next_block,
            )
        )

    return sqt


def register_child(
    parent: Optional[NavNode],
    node: object,
    sqt: SQTInfo,
    context: str,
    result: AuditResult,
) -> None:
    """
    Attach a node to its parent and check that the parent BBox
    encloses it (map-format-v4.md, section 14).
    """

    if parent is None:
        sqt.roots.append(node)
        return

    parent.children.append(node)

    if (
        node.xmin < parent.xmin
        or node.ymin < parent.ymin
        or node.xmax > parent.xmax
        or node.ymax > parent.ymax
    ):
        result.fail(
            "{}: BBox {} is not enclosed by the parent BBox {}".format(
                context,
                (node.xmin, node.ymin, node.xmax, node.ymax),
                (parent.xmin, parent.ymin, parent.xmax, parent.ymax),
            )
        )


def parse_data_node(
    data: bytes,
    offset: int,
    section_end: int,
    context: str,
    audit: IndexAuditData,
    result: AuditResult,
) -> Tuple[Optional[DataNode], int]:
    """
    Current V4 Data Node:

        <iiiiBII>

    25 bytes.
    """

    if offset + DATA_NODE_SIZE > section_end:
        result.fail(
            "{}: Data Node out of bounds "
            "(offset={}, size={}, section_end={})".format(
                context,
                offset,
                DATA_NODE_SIZE,
                section_end,
            )
        )
        return None, offset

    if not require_range(
        data,
        offset,
        DATA_NODE_SIZE,
        context,
        result,
    ):
        return None, offset

    xmin = i32(data, offset)
    ymin = i32(data, offset + 4)
    xmax = i32(data, offset + 8)
    ymax = i32(data, offset + 12)

    feature_type = data[offset + 16]

    v1 = u32(data, offset + 17)
    v2 = u32(data, offset + 21)

    if xmin > xmax:
        result.fail(
            "{}: invalid BBox xmin={} > xmax={}".format(
                context,
                xmin,
                xmax,
            )
        )

    if ymin > ymax:
        result.fail(
            "{}: invalid BBox ymin={} > ymax={}".format(
                context,
                ymin,
                ymax,
            )
        )

    node = DataNode(
        offset=offset,
        xmin=xmin,
        ymin=ymin,
        xmax=xmax,
        ymax=ymax,
        feature_type=feature_type,
        v1=v1,
        v2=v2,
    )

    audit.data_nodes.append(node)

    return node, offset + DATA_NODE_SIZE


def parse_nav_node(
    data: bytes,
    offset: int,
    block_end: int,
    context: str,
    audit: IndexAuditData,
    result: AuditResult,
    expected_level: Optional[int] = None,
) -> Optional[NavNode]:
    """
    Current V4 Navigation Node:

        <IiiiiII>

    28 bytes.

    child_block is the absolute file offset of the block with the
    children; its constraints are checked by the caller.
    """

    if offset + NAV_NODE_SIZE > block_end:
        result.fail(
            "{}: Navigation Node out of bounds "
            "(offset={}, size={}, block_end={})".format(
                context,
                offset,
                NAV_NODE_SIZE,
                block_end,
            )
        )
        return None

    if not require_range(
        data,
        offset,
        NAV_NODE_SIZE,
        context,
        result,
    ):
        return None

    child_block = u32(data, offset)

    xmin = i32(data, offset + 4)
    ymin = i32(data, offset + 8)
    xmax = i32(data, offset + 12)
    ymax = i32(data, offset + 16)

    level = u32(data, offset + 20)
    child_count = u32(data, offset + 24)

    if xmin > xmax:
        result.fail(
            "{}: invalid BBox xmin={} > xmax={}".format(
                context,
                xmin,
                xmax,
            )
        )

    if ymin > ymax:
        result.fail(
            "{}: invalid BBox ymin={} > ymax={}".format(
                context,
                ymin,
                ymax,
            )
        )

    if expected_level is not None and level != expected_level:
        result.fail(
            "{}: level={}, expected={}".format(
                context,
                level,
                expected_level,
            )
        )

    node = NavNode(
        offset=offset,
        child_block=child_block,
        xmin=xmin,
        ymin=ymin,
        xmax=xmax,
        ymax=ymax,
        level=level,
        child_count=child_count,
    )

    audit.nav_nodes.append(node)

    return node


# ============================================================================
# IDX / SQT, legacy V3 layout (map-format-v3.md)
# ============================================================================

def validate_idx_lod_offsets_v3(
    data: bytes,
    lod_offsets: Tuple[int, int, int],
    result: AuditResult,
) -> None:
    """
    LOD offsets are absolute offsets from the beginning of the file.

    Each offset points to a 16-byte SQT header.
    """

    payload_start = PGO_HEADER_SIZE
    payload_end = len(data)

    for index, offset in enumerate(lod_offsets):
        if offset < payload_start:
            result.fail(
                "IDX LOD {}: offset={} is before payload start={}".format(
                    index,
                    offset,
                    payload_start,
                )
            )
            continue

        if offset >= payload_end:
            result.fail(
                "IDX LOD {}: offset={} is outside file".format(
                    index,
                    offset,
                )
            )
            continue

        if offset + SQT_HEADER_SIZE_V3 > payload_end:
            result.fail(
                "IDX LOD {}: SQT header at offset={} "
                "does not fit in file".format(
                    index,
                    offset,
                )
            )

    if lod_offsets[0] >= lod_offsets[1]:
        result.fail(
            "IDX LOD offsets are not strictly increasing: {}".format(
                lod_offsets
            )
        )

    if lod_offsets[1] >= lod_offsets[2]:
        result.fail(
            "IDX LOD offsets are not strictly increasing: {}".format(
                lod_offsets
            )
        )


def parse_sqt_v3(
    data: bytes,
    offset: int,
    section_end: int,
    lod_index: int,
    audit: IndexAuditData,
    result: AuditResult,
) -> Optional[SQTInfo]:
    """
    Parse one complete SQT block.

    mode == 0:
        roots are Data Nodes.

    mode > 0:
        roots are Navigation Nodes.

    For a non-empty tree the root level must be mode - 1.
    """

    if section_end < offset:
        result.fail(
            "LOD {}: section end {} before start {}".format(
                lod_index,
                section_end,
                offset,
            )
        )
        return None

    if not require_range(
        data,
        offset,
        SQT_HEADER_SIZE_V3,
        "LOD {} SQT header".format(lod_index),
        result,
    ):
        return None

    if offset + SQT_HEADER_SIZE_V3 > section_end:
        result.fail(
            "LOD {}: SQT header crosses LOD boundary".format(
                lod_index
            )
        )
        return None

    header = data[
        offset:
        offset + SQT_HEADER_SIZE_V3
    ]

    if header[0:4] != SQT_MAGIC_V3:
        result.fail(
            "LOD {}: invalid SQT magic {!r}".format(
                lod_index,
                header[0:4],
            )
        )
        return None

    topology = u32(header, 4)
    mode = u32(header, 8)
    root_count = u32(header, 12)

    if topology != 1:
        result.fail(
            "LOD {}: unsupported SQT topology={}, expected 1".format(
                lod_index,
                topology,
            )
        )

    sqt = SQTInfo(
        offset=offset,
        depth=mode,
        root_count=root_count,
    )

    audit.sqts.append(sqt)

    current = offset + SQT_HEADER_SIZE_V3

    for root_index in range(root_count):
        if mode == 0:
            node, new_current = parse_data_node(
                data=data,
                offset=current,
                section_end=section_end,
                context=(
                    "LOD {} root DATA[{}]".format(
                        lod_index,
                        root_index,
                    )
                ),
                audit=audit,
                result=result,
            )
        else:
            node, new_current = parse_nav_node_v3(
                data=data,
                offset=current,
                section_end=section_end,
                context=(
                    "LOD {} root NAV[{}]".format(
                        lod_index,
                        root_index,
                    )
                ),
                audit=audit,
                result=result,
                expected_level=mode - 1,
            )

        if node is None:
            return sqt

        sqt.roots.append(node)
        current = new_current

    if current != section_end:
        result.fail(
            "LOD {}: parsed section ends at {}, "
            "but LOD boundary is {}; unparsed bytes={}".format(
                lod_index,
                current,
                section_end,
                section_end - current,
            )
        )

    return sqt


def parse_nav_node_v3(
    data: bytes,
    offset: int,
    section_end: int,
    context: str,
    audit: IndexAuditData,
    result: AuditResult,
    expected_level: Optional[int] = None,
) -> Tuple[Optional[NavNode], int]:
    """
    Current V3 Navigation Node:

        <IiiiiII>

    28 bytes.

    v3_jump is the exact byte size of the complete child subtree.

    Therefore:

        subtree_start = offset + 28
        subtree_end   = subtree_start + v3_jump
    """

    if offset + NAV_NODE_SIZE > section_end:
        result.fail(
            "{}: Navigation Node out of bounds "
            "(offset={}, size={}, section_end={})".format(
                context,
                offset,
                NAV_NODE_SIZE,
                section_end,
            )
        )
        return None, offset

    if not require_range(
        data,
        offset,
        NAV_NODE_SIZE,
        context,
        result,
    ):
        return None, offset

    v3_jump = u32(data, offset)

    xmin = i32(data, offset + 4)
    ymin = i32(data, offset + 8)
    xmax = i32(data, offset + 12)
    ymax = i32(data, offset + 16)

    level = u32(data, offset + 20)
    child_count = u32(data, offset + 24)

    if xmin > xmax:
        result.fail(
            "{}: invalid BBox xmin={} > xmax={}".format(
                context,
                xmin,
                xmax,
            )
        )

    if ymin > ymax:
        result.fail(
            "{}: invalid BBox ymin={} > ymax={}".format(
                context,
                ymin,
                ymax,
            )
        )

    if expected_level is not None and level != expected_level:
        result.fail(
            "{}: level={}, expected={}".format(
                context,
                level,
                expected_level,
            )
        )

    subtree_start = offset + NAV_NODE_SIZE

    subtree_end = checked_add(
        subtree_start,
        v3_jump,
        "{}: v3_jump".format(context),
        result,
    )

    if subtree_end is None:
        return None, offset

    if subtree_end > section_end:
        result.fail(
            "{}: v3_jump exceeds LOD boundary "
            "(subtree_end={}, section_end={})".format(
                context,
                subtree_end,
                section_end,
            )
        )
        return None, offset

    node = NavNode(
        offset=offset,
        v3_jump=v3_jump,
        xmin=xmin,
        ymin=ymin,
        xmax=xmax,
        ymax=ymax,
        level=level,
        child_count=child_count,
    )

    audit.nav_nodes.append(node)

    current = subtree_start

    if level > 0:
        child_is_nav = True
        child_level = level - 1
    else:
        child_is_nav = False
        child_level = 0

    for child_index in range(child_count):
        if child_is_nav:
            child, new_current = parse_nav_node_v3(
                data=data,
                offset=current,
                section_end=subtree_end,
                context=(
                    "{} child NAV[{}]".format(
                        context,
                        child_index,
                    )
                ),
                audit=audit,
                result=result,
                expected_level=child_level,
            )
        else:
            child, new_current = parse_data_node(
                data=data,
                offset=current,
                section_end=subtree_end,
                context=(
                    "{} child DATA[{}]".format(
                        context,
                        child_index,
                    )
                ),
                audit=audit,
                result=result,
            )

        if child is None:
            return None, offset

        node.children.append(child)
        current = new_current

    # Critical V3 check:
    #
    # v3_jump must equal the physical size of all children.
    actual_children_size = current - subtree_start

    if actual_children_size != v3_jump:
        result.fail(
            "{}: v3_jump mismatch: declared={}, "
            "actual_children_size={}".format(
                context,
                v3_jump,
                actual_children_size,
            )
        )

    if child_count == 0 and v3_jump != 0:
        result.fail(
            "{}: child_count=0 but v3_jump={}".format(
                context,
                v3_jump,
            )
        )

    if child_count > 0 and v3_jump == 0:
        result.fail(
            "{}: child_count={} but v3_jump=0".format(
                context,
                child_count,
            )
        )

    return node, subtree_end


# ============================================================================
# MLP
# ============================================================================

def parse_mlp(
    data: bytes,
    result: AuditResult,
) -> List[GeometryRecord]:
    """
    Parse all MLP geometry records.

    Local header:

        sequence    >I
        body length <I

    Body:

        bbox        16 bytes
        num_parts   uint32
        num_points  uint32
        parts[]     uint32[]
        points[]    int32[2][]
    """

    header = parse_pgo_header(
        data,
        FILE_TYPE_MLP,
        result,
    )

    if header is None:
        return []

    records: List[GeometryRecord] = []

    cursor = PGO_HEADER_SIZE
    sequence_expected = 1

    while cursor < len(data):
        record_offset = cursor

        if not require_range(
            data,
            cursor,
            MLP_LOCAL_HEADER_SIZE,
            "MLP Local Header",
            result,
        ):
            break

        sequence = be_u32(data, cursor)
        content_length = u32(data, cursor + 4)

        if sequence != sequence_expected:
            result.fail(
                "MLP record at offset={}: sequence={}, expected={}".format(
                    record_offset,
                    sequence,
                    sequence_expected,
                )
            )

        body_offset = cursor + MLP_LOCAL_HEADER_SIZE

        record_end = checked_add(
            body_offset,
            content_length,
            "MLP record end",
            result,
        )

        if record_end is None:
            break

        if record_end > len(data):
            result.fail(
                "MLP record at offset={}: Content Length={} "
                "exceeds file: record_end={}, file_size={}".format(
                    record_offset,
                    content_length,
                    record_end,
                    len(data),
                )
            )
            break

        if content_length < MLP_BODY_FIXED_SIZE:
            result.fail(
                "MLP record #{}: Content Length={} < "
                "minimum body size={}".format(
                    sequence,
                    content_length,
                    MLP_BODY_FIXED_SIZE,
                )
            )
            break

        xmin = i32(data, body_offset)
        ymin = i32(data, body_offset + 4)
        xmax = i32(data, body_offset + 8)
        ymax = i32(data, body_offset + 12)

        num_parts = u32(data, body_offset + 16)
        num_points = u32(data, body_offset + 20)

        if xmin > xmax:
            result.fail(
                "MLP record #{}: invalid BBox xmin={} > xmax={}".format(
                    sequence,
                    xmin,
                    xmax,
                )
            )

        if ymin > ymax:
            result.fail(
                "MLP record #{}: invalid BBox ymin={} > ymax={}".format(
                    sequence,
                    ymin,
                    ymax,
                )
            )

        parts_bytes = checked_mul(
            num_parts,
            4,
            "MLP record #{}: parts size".format(sequence),
            result,
        )

        points_bytes = checked_mul(
            num_points,
            8,
            "MLP record #{}: points size".format(sequence),
            result,
        )

        if parts_bytes is None or points_bytes is None:
            break

        expected_body_size = (
            MLP_BODY_FIXED_SIZE
            + parts_bytes
            + points_bytes
        )

        if expected_body_size != content_length:
            result.fail(
                "MLP record #{}: Content Length mismatch: "
                "declared={}, calculated={}".format(
                    sequence,
                    content_length,
                    expected_body_size,
                )
            )
            break

        parts_offset = body_offset + MLP_BODY_FIXED_SIZE
        points_offset = parts_offset + parts_bytes

        if points_offset + points_bytes != record_end:
            result.fail(
                "MLP record #{}: calculated geometry end "
                "does not equal record end".format(
                    sequence
                )
            )
            break

        parts: List[int] = []

        for index in range(num_parts):
            parts.append(
                u32(
                    data,
                    parts_offset + index * 4,
                )
            )

        validate_parts(
            parts=parts,
            num_parts=num_parts,
            num_points=num_points,
            sequence=sequence,
            result=result,
        )

        points: List[Tuple[int, int]] = []

        for index in range(num_points):
            point_offset = points_offset + index * 8

            x = i32(data, point_offset)
            y = i32(data, point_offset + 4)

            points.append((x, y))

        records.append(
            GeometryRecord(
                record_offset=record_offset,
                sequence=sequence,
                content_length=content_length,
                body_offset=body_offset,
                xmin=xmin,
                ymin=ymin,
                xmax=xmax,
                ymax=ymax,
                num_parts=num_parts,
                num_points=num_points,
                parts=parts,
                points=points,
                record_size=MLP_LOCAL_HEADER_SIZE + content_length,
            )
        )

        cursor = record_end
        sequence_expected += 1

    if cursor != len(data):
        result.fail(
            "MLP parser ended at offset={}, file_size={}".format(
                cursor,
                len(data),
            )
        )

    return records


def validate_parts(
    parts: List[int],
    num_parts: int,
    num_points: int,
    sequence: int,
    result: AuditResult,
) -> None:
    """
    Current V4 parts semantics:

        parts[0] == 0

        every part index < num_points

        part indexes are strictly increasing
    """

    if num_parts == 0:
        return

    if num_points == 0:
        result.fail(
            "MLP record #{}: num_parts={}, num_points=0".format(
                sequence,
                num_parts,
            )
        )
        return

    if parts[0] != 0:
        result.fail(
            "MLP record #{}: parts[0]={}, expected 0".format(
                sequence,
                parts[0],
            )
        )

    for index in range(num_parts):
        if parts[index] >= num_points:
            result.fail(
                "MLP record #{}: parts[{}]={} >= num_points={}".format(
                    sequence,
                    index,
                    parts[index],
                    num_points,
                )
            )

        if index > 0 and parts[index] <= parts[index - 1]:
            result.fail(
                "MLP record #{}: parts[] not strictly increasing "
                "at index {}: {} -> {}".format(
                    sequence,
                    index,
                    parts[index - 1],
                    parts[index],
                )
            )


# ============================================================================
# Geometry semantic checks
# ============================================================================

def audit_geometry_semantics(
    idx: IndexAuditData,
    mlp_records: List[GeometryRecord],
    result: AuditResult,
    is_poi: bool,
) -> None:
    """
    Checks that follow directly from the current V4 format contract.
    """

    if is_poi:
        for node in idx.data_nodes:
            if node.xmin != node.xmax:
                result.fail(
                    "POI Data Node at offset={}: "
                    "xmin={} != xmax={}".format(
                        node.offset,
                        node.xmin,
                        node.xmax,
                    )
                )

            if node.ymin != node.ymax:
                result.fail(
                    "POI Data Node at offset={}: "
                    "ymin={} != ymax={}".format(
                        node.offset,
                        node.ymin,
                        node.ymax,
                    )
                )

        return

    for record in mlp_records:
        if record.num_points == 0:
            result.fail(
                "MLP record #{}: num_points=0".format(
                    record.sequence
                )
            )

        if record.num_parts == 0:
            result.fail(
                "MLP record #{}: num_parts=0 for geometry".format(
                    record.sequence
                )
            )

        for point_index, (x, y) in enumerate(record.points):
            if x < record.xmin or x > record.xmax:
                result.fail(
                    "MLP record #{}: point[{}].x={} "
                    "outside BBox".format(
                        record.sequence,
                        point_index,
                        x,
                    )
                )
                break

            if y < record.ymin or y > record.ymax:
                result.fail(
                    "MLP record #{}: point[{}].y={} "
                    "outside BBox".format(
                        record.sequence,
                        point_index,
                        y,
                    )
                )
                break


def audit_polygon_closure(
    mlp_records: List[GeometryRecord],
    result: AuditResult,
) -> None:
    """
    Polygon layers must have closed rings.

    Current V4 specification:
        outer rings = CW
        holes        = CCW

    Winding direction is calculated only for diagnostics.
    No winding rejection is performed here because the binary
    representation does not encode an explicit outer/inner role.
    """

    for record in mlp_records:
        for part_index, start in enumerate(record.parts):
            if part_index + 1 < record.num_parts:
                end = record.parts[part_index + 1]
            else:
                end = record.num_points

            if end <= start:
                continue

            first = record.points[start]
            last = record.points[end - 1]

            if first != last:
                result.fail(
                    "MLP record #{}: polygon part {} "
                    "is not closed: first={}, last={}".format(
                        record.sequence,
                        part_index,
                        first,
                        last,
                    )
                )

            _ = polygon_signed_area2(
                record.points[start:end]
            )


def polygon_signed_area2(
    points: List[Tuple[int, int]],
) -> int:
    """
    Twice the signed polygon area.

    Integer-only implementation.
    """

    if len(points) < 3:
        return 0

    area = 0

    for index in range(len(points) - 1):
        x1, y1 = points[index]
        x2, y2 = points[index + 1]

        area += x1 * y2
        area -= x2 * y1

    return area


# ============================================================================
# DBF
# ============================================================================

def parse_db(
    data: bytes,
    result: AuditResult,
    is_poi: bool,
) -> Optional[DBInfo]:
    """
    Parse PGO/DBF produced by the current compiler.
    """

    header = parse_pgo_header(
        data,
        FILE_TYPE_DB,
        result,
    )

    if header is None:
        return None

    if len(data) < PGO_HEADER_SIZE + DBF_HEADER_SIZE:
        result.fail(
            "DBF: file is too short for PGO + {}-byte DBF header".format(
                DBF_HEADER_SIZE
            )
        )
        return None

    dbf = data[PGO_HEADER_SIZE:]

    if dbf[0] != DBF_MAGIC:
        result.fail(
            "DBF: invalid magic 0x{:02X}, expected 0x03".format(
                dbf[0]
            )
        )

    if dbf[1:4] != b"\x00\x00\x00":
        result.fail(
            "DBF: bytes 0x01..0x03 are {}, expected 00 00 00".format(
                dbf[1:4].hex(" ")
            )
        )

    record_count = u32(dbf, 4)
    header_size = u16(dbf, 8)
    record_size = u16(dbf, 10)

    if header_size != DBF_HEADER_SIZE:
        result.fail(
            "DBF: header size={}, expected {}".format(
                header_size,
                DBF_HEADER_SIZE,
            )
        )

    if record_size != DBF_RECORD_SIZE:
        result.fail(
            "DBF: record size={}, expected {}".format(
                record_size,
                DBF_RECORD_SIZE,
            )
        )

    descriptor_start = 32

    fields: List[Tuple[str, int]] = []

    for index, (expected_name, expected_length) in enumerate(
        EXPECTED_DB_FIELDS
    ):
        offset = (
            descriptor_start
            + index * DBF_DESCRIPTOR_SIZE
        )

        if offset + DBF_DESCRIPTOR_SIZE > len(dbf):
            result.fail(
                "DBF: descriptor {} out of bounds".format(
                    index + 1
                )
            )
            return None

        descriptor = dbf[
            offset:
            offset + DBF_DESCRIPTOR_SIZE
        ]

        raw_name = descriptor[0:11]

        field_name = raw_name.split(
            b"\x00",
            1,
        )[0].decode(
            "ascii",
            errors="replace",
        )

        field_type = descriptor[11]
        field_length = descriptor[16]

        if field_name != expected_name:
            result.fail(
                "DBF descriptor {}: name={!r}, expected={!r}".format(
                    index + 1,
                    field_name,
                    expected_name,
                )
            )

        if field_type != ord("C"):
            result.fail(
                "DBF descriptor {} {}: type=0x{:02X}, "
                "expected ASCII 'C'".format(
                    index + 1,
                    expected_name,
                    field_type,
                )
            )

        if field_length != expected_length:
            result.fail(
                "DBF descriptor {} {}: length={}, expected {}".format(
                    index + 1,
                    expected_name,
                    field_length,
                    expected_length,
                )
            )

        if descriptor[12:16] != b"\x00" * 4:
            result.fail(
                "DBF descriptor {} {}: reserved bytes "
                "0x0C..0x0F are not zero".format(
                    index + 1,
                    expected_name,
                )
            )

        if descriptor[17:32] != b"\x00" * 15:
            result.fail(
                "DBF descriptor {} {}: reserved bytes "
                "0x11..0x1F are not zero".format(
                    index + 1,
                    expected_name,
                )
            )

        fields.append(
            (
                field_name,
                field_length,
            )
        )

    terminator_offset = (
        descriptor_start
        + DBF_FIELD_COUNT * DBF_DESCRIPTOR_SIZE
    )

    if terminator_offset >= len(dbf):
        result.fail(
            "DBF: header terminator is out of bounds"
        )
        return None

    if dbf[terminator_offset] != DBF_HEADER_TERMINATOR:
        result.fail(
            "DBF: header terminator=0x{:02X}, expected 0x0D".format(
                dbf[terminator_offset]
            )
        )

    if header_size != terminator_offset + 1:
        result.fail(
            "DBF: header structure ends at {}, "
            "but header_size={}".format(
                terminator_offset + 1,
                header_size,
            )
        )

    records_start = header_size

    expected_dbf_size = (
        records_start
        + record_count * record_size
    )

    if expected_dbf_size != len(dbf):
        result.fail(
            "DBF: file size mismatch: header_size={} + "
            "records={} * record_size={} = {}, "
            "actual DBF payload={}".format(
                records_start,
                record_count,
                record_size,
                expected_dbf_size,
                len(dbf),
            )
        )

    records: Dict[int, DBRecord] = {}

    for record_index in range(1, record_count + 1):
        record_offset = (
            records_start
            + (record_index - 1) * record_size
        )

        if not require_range(
            dbf,
            record_offset,
            record_size,
            "DBF record {}".format(record_index),
            result,
        ):
            break

        record = dbf[
            record_offset:
            record_offset + record_size
        ]

        active = (
            record[0] == DBF_RECORD_ACTIVE
        )

        osm_id = record[1:13]
        code = record[13:17]
        name = record[17:117]

        records[record_index] = DBRecord(
            index=record_index,
            offset=record_offset,
            active=active,
            osm_id=osm_id,
            code=code,
            name=name,
        )

    if not is_poi:
        if record_count < 1:
            result.fail(
                "DBF standard layer: record_count must be at least 1"
            )
        else:
            dummy = records.get(1)

            if dummy is not None:
                dummy_bytes = dbf[
                    dummy.offset:
                    dummy.offset + DBF_RECORD_SIZE
                ]

                if any(
                    value != 0
                    for value in dummy_bytes
                ):
                    result.fail(
                        "DBF standard layer: "
                        "record 1 is not an all-zero dummy record"
                    )

    return DBInfo(
        record_count=record_count,
        header_size=header_size,
        record_size=record_size,
        fields=fields,
        records=records,
    )


# ============================================================================
# Cross-file IDX <-> MLP
# ============================================================================

def audit_idx_mlp(
    idx: IndexAuditData,
    mlp_records: List[GeometryRecord],
    result: AuditResult,
    is_poi: bool,
) -> None:
    """
    Verify Data Node v1 references.

    Current writer:

        feature.v1 = len(bin_records) + 8

    Firmware interpretation:

        absolute_body_offset = 32 + v1

    Therefore v1 identifies the Geometry Body.
    """

    by_v1: Dict[int, GeometryRecord] = {}

    for record in mlp_records:
        relative_body_offset = (
            record.body_offset - PGO_HEADER_SIZE
        )

        if relative_body_offset in by_v1:
            result.fail(
                "MLP: duplicate v1/body offset {}".format(
                    relative_body_offset
                )
            )

        by_v1[relative_body_offset] = record

    if is_poi:
        for node in idx.data_nodes:
            if node.v1 != 0:
                result.fail(
                    "POI Data Node at offset={}: v1={}, expected 0".format(
                        node.offset,
                        node.v1,
                    )
                )

        if mlp_records:
            result.warn(
                "POI layer has an MLP file containing geometry records"
            )

        return

    for node in idx.data_nodes:
        if node.v1 == 0:
            result.fail(
                "Data Node at offset={}: v1=0 for geometry layer".format(
                    node.offset
                )
            )
            continue

        record = by_v1.get(node.v1)

        if record is None:
            result.fail(
                "Data Node at offset={}: v1={} does not reference "
                "an MLP Geometry Body".format(
                    node.offset,
                    node.v1,
                )
            )
            continue

        expected_absolute = (
            PGO_HEADER_SIZE + node.v1
        )

        if expected_absolute != record.body_offset:
            result.fail(
                "Data Node at offset={}: v1={} resolves to absolute {}, "
                "but MLP body is at {}".format(
                    node.offset,
                    node.v1,
                    expected_absolute,
                    record.body_offset,
                )
            )

        if (
            node.xmin != record.xmin
            or node.ymin != record.ymin
            or node.xmax != record.xmax
            or node.ymax != record.ymax
        ):
            result.fail(
                "Data Node at offset={}: BBox {} does not match "
                "MLP record #{} BBox {}".format(
                    node.offset,
                    (
                        node.xmin,
                        node.ymin,
                        node.xmax,
                        node.ymax,
                    ),
                    record.sequence,
                    (
                        record.xmin,
                        record.ymin,
                        record.xmax,
                        record.ymax,
                    ),
                )
            )


# ============================================================================
# LOD structure (V4)
# ============================================================================

MAX_REPORTED_PER_CHECK = 10


def audit_lod_membership(
    idx: IndexAuditData,
    result: AuditResult,
) -> None:
    """
    LOD n contains the objects whose features.csv LOD is >= n
    (map-format-v4.md, section 4.2). Therefore every LOD is a subset
    of the previous one.

    Objects are compared as a multiset of (Type, v2): a variant of the
    geometry may have another v1, but Type and the DB record are the
    same in all LODs of one object.
    """

    lod_count = len(idx.sqts)

    per_lod: List[Counter] = [Counter() for _ in range(lod_count)]

    for node in idx.data_nodes:
        if 0 <= node.lod < lod_count:
            per_lod[node.lod][(node.feature_type, node.v2)] += 1

    reported = 0

    for lod_index in range(1, lod_count):
        for key, count in per_lod[lod_index].items():
            previous = per_lod[lod_index - 1][key]

            if count <= previous:
                continue

            reported += 1

            if reported > MAX_REPORTED_PER_CHECK:
                result.fail(
                    "LOD membership: further violations not shown"
                )
                return

            result.fail(
                "LOD {}: object Type={} v2={} occurs {} times, "
                "but LOD {} contains it {} times "
                "(LOD n must be a subset of LOD n-1)".format(
                    lod_index,
                    key[0],
                    key[1],
                    count,
                    lod_index - 1,
                    previous,
                )
            )


def is_subsequence(
    short: List[Tuple[int, int]],
    full: List[Tuple[int, int]],
) -> bool:
    iterator = iter(full)

    return all(point in iterator for point in short)


def audit_lod_geometry_variants(
    idx: IndexAuditData,
    mlp_records: List[GeometryRecord],
    result: AuditResult,
) -> None:
    """
    Geometry variants (map-format-v4.md, section 3.5):

      1. LOD 0 and LOD 1 reference full geometry: every v1 of LOD 1
         also occurs in LOD 0.
      2. A node of LOD 2 or LOD 3 either shares v1 with LOD 0
         (nothing was removed) or references a simplified variant:
         a record that has the same Type and the same first point as
         a full record, whose points are a subsequence of the full
         points in the original order, with no more points or parts
         and with a BBox inside the full BBox. A single-part line
         keeps its first and last point.
      3. A variant that equals the full geometry should have shared
         its v1 (warning).
    """

    by_v1: Dict[int, GeometryRecord] = {}

    for record in mlp_records:
        by_v1[record.body_offset - PGO_HEADER_SIZE] = record

    v1_by_lod: List[Dict[int, int]] = [dict() for _ in range(LOD_COUNT)]

    for node in idx.data_nodes:
        if 0 <= node.lod < LOD_COUNT and node.v1 != 0:
            v1_by_lod[node.lod][node.v1] = node.feature_type

    full = v1_by_lod[0]

    reported = 0

    def report(message: str) -> bool:
        nonlocal reported

        reported += 1

        if reported > MAX_REPORTED_PER_CHECK:
            if reported == MAX_REPORTED_PER_CHECK + 1:
                result.fail(
                    "LOD geometry variants: further violations not shown"
                )
            return False

        result.fail(message)
        return True

    for v1 in v1_by_lod[1]:
        if v1 not in full:
            report(
                "LOD 1: v1={} is not a full geometry of LOD 0 "
                "(LOD 0 and LOD 1 must share geometry)".format(v1)
            )

    candidates: Dict[Tuple[int, Tuple[int, int]], List[GeometryRecord]] = {}

    for v1, feature_type in full.items():
        record = by_v1.get(v1)

        if record is None or not record.points:
            continue

        candidates.setdefault(
            (feature_type, record.points[0]),
            [],
        ).append(record)

    for lod_index in (2, 3):
        for v1, feature_type in v1_by_lod[lod_index].items():
            if v1 in full:
                continue

            variant = by_v1.get(v1)

            if variant is None or not variant.points:
                continue

            originals = candidates.get(
                (feature_type, variant.points[0]),
                [],
            )

            matched = False
            identical = False

            for original in originals:
                if (
                    variant.num_points > original.num_points
                    or variant.num_parts > original.num_parts
                ):
                    continue

                if (
                    variant.xmin < original.xmin
                    or variant.ymin < original.ymin
                    or variant.xmax > original.xmax
                    or variant.ymax > original.ymax
                ):
                    continue

                if (
                    variant.num_parts == 1
                    and original.num_parts == 1
                    and variant.points[-1] != original.points[-1]
                ):
                    continue

                if not is_subsequence(
                    variant.points,
                    original.points,
                ):
                    continue

                matched = True

                if variant.points == original.points:
                    identical = True

                break

            if not matched:
                report(
                    "LOD {}: v1={} (MLP record #{}) is not a valid "
                    "simplified variant of any full geometry "
                    "(Type={})".format(
                        lod_index,
                        v1,
                        variant.sequence,
                        feature_type,
                    )
                )
            elif identical:
                result.warn(
                    "LOD {}: v1={} (MLP record #{}) equals the full "
                    "geometry; v1 should be shared".format(
                        lod_index,
                        v1,
                        variant.sequence,
                    )
                )


# ============================================================================
# Cross-file IDX <-> DB
# ============================================================================

def audit_idx_db(
    idx: IndexAuditData,
    db_info: Optional[DBInfo],
    result: AuditResult,
    is_poi: bool,
) -> None:
    """
    Verify Data Node v2 against DB record numbering.
    """

    if db_info is None:
        for node in idx.data_nodes:
            if node.v2 != 0:
                result.fail(
                    "Data Node at offset={}: v2={} but .db is absent".format(
                        node.offset,
                        node.v2,
                    )
                )
        return

    for node in idx.data_nodes:
        v2 = node.v2

        if is_poi:
            # V4 (map-format-v4.md, section 10.5): every POI has a
            # physical record, v2 >= 1. The V3 text allowed v2 = 0.
            if v2 == 0 and idx.version == 3:
                continue

            if v2 == 0:
                result.fail(
                    "POI Data Node at offset={}: v2=0 while .db exists "
                    "(every POI must have a DB record)".format(
                        node.offset
                    )
                )
                continue

            if v2 > db_info.record_count:
                result.fail(
                    "POI Data Node at offset={}: v2={} exceeds "
                    "DB record_count={}".format(
                        node.offset,
                        v2,
                        db_info.record_count,
                    )
                )
                continue

            if v2 not in db_info.records:
                result.fail(
                    "POI Data Node at offset={}: "
                    "v2={} has no DB record".format(
                        node.offset,
                        v2,
                    )
                )
            continue

        if v2 == 0:
            result.fail(
                "Standard Data Node at offset={}: v2=0 while .db exists".format(
                    node.offset
                )
            )
            continue

        if v2 > db_info.record_count:
            result.fail(
                "Standard Data Node at offset={}: v2={} exceeds "
                "DB record_count={}".format(
                    node.offset,
                    v2,
                    db_info.record_count,
                )
            )
            continue

        if v2 == 1:
            continue

        if v2 not in db_info.records:
            result.fail(
                "Standard Data Node at offset={}: "
                "v2={} has no DB record".format(
                    node.offset,
                    v2,
                )
            )


# ============================================================================
# map.name
# ============================================================================

def validate_map_name(
    path: Path,
    result: AuditResult,
) -> Optional[dict]:
    """
    Validate the map.name JSON generated by MapCompiler.create_map_name().

    Current writer output:

        {
            "centerLat": <number>,
            "centerLon": <number>,
            "mapName": <string>
        }

    JSON is parsed strictly enough to reject malformed data, while
    allowing additional future fields.
    """

    if not path.exists():
        result.fail(
            "map.name: missing file"
        )
        return None

    if not path.is_file():
        result.fail(
            "map.name: path exists but is not a regular file"
        )
        return None

    try:
        raw = path.read_bytes()
    except OSError as exc:
        result.fail(
            "map.name: cannot read file: {}".format(exc)
        )
        return None

    if not raw:
        result.fail(
            "map.name: empty file"
        )
        return None

    try:
        text = raw.decode("utf-8")
    except UnicodeDecodeError as exc:
        result.fail(
            "map.name: invalid UTF-8: {}".format(exc)
        )
        return None

    try:
        value = json.loads(text)
    except json.JSONDecodeError as exc:
        result.fail(
            "map.name: invalid JSON: {}".format(exc)
        )
        return None

    if not isinstance(value, dict):
        result.fail(
            "map.name: JSON root must be an object"
        )
        return None

    for field_name in MAP_NAME_REQUIRED_FIELDS:
        if field_name not in value:
            result.fail(
                "map.name: missing required field {!r}".format(
                    field_name
                )
            )

    if "centerLat" in value:
        center_lat = value["centerLat"]

        if isinstance(center_lat, bool) or not isinstance(
            center_lat,
            (int, float),
        ):
            result.fail(
                "map.name: centerLat must be a JSON number"
            )

    if "centerLon" in value:
        center_lon = value["centerLon"]

        if isinstance(center_lon, bool) or not isinstance(
            center_lon,
            (int, float),
        ):
            result.fail(
                "map.name: centerLon must be a JSON number"
            )

    if "mapName" in value:
        map_name = value["mapName"]

        if not isinstance(map_name, str):
            result.fail(
                "map.name: mapName must be a JSON string"
            )
        elif not map_name:
            result.fail(
                "map.name: mapName must not be empty"
            )

    return value


# ============================================================================
# Fixture discovery
# ============================================================================

def discover_fixture_directories(
    root: Path,
) -> List[Path]:
    """
    Discover fixture directories.

    A directory is considered a fixture directory if it contains at least
    one of:

        *.idx
        *.mlp
        *.db
        map.name

    The scan is recursive.
    """

    directories = set()

    for suffix in (
        ".idx",
        ".mlp",
        ".db",
    ):
        for path in root.rglob("*{}".format(suffix)):
            if path.is_file():
                directories.add(path.parent)

    for path in root.rglob("map.name"):
        if path.is_file():
            directories.add(path.parent)

    return sorted(directories)


def layer_name_from_idx(
    idx_path: Path,
) -> str:
    return idx_path.stem


# ============================================================================
# Verbose reporting
# ============================================================================

def print_verbose_layer_info(
    directory: Path,
    stats: LayerStats,
    idx_audit: Optional[IndexAuditData],
    mlp_records: List[GeometryRecord],
    db_info: Optional[DBInfo],
    map_name: Optional[dict],
) -> None:
    print(
        "      INFO  files:"
        " idx={} B"
        " mlp={} B"
        " db={} B".format(
            stats.idx_size,
            stats.mlp_size,
            stats.db_size,
        )
    )

    if idx_audit is not None:
        print(
            "      INFO  IDX:"
            " SQT={}"
            " NAV={}"
            " DATA={}".format(
                len(idx_audit.sqts),
                len(idx_audit.nav_nodes),
                len(idx_audit.data_nodes),
            )
        )

        for lod_index, sqt in enumerate(idx_audit.sqts):
            print(
                "      INFO  LOD {}:"
                " depth={}"
                " roots={}"
                " blocks={}"
                " size={} B".format(
                    lod_index,
                    sqt.depth,
                    sqt.root_count,
                    sqt.block_count,
                    sqt.section_size,
                )
            )

    print(
        "      INFO  MLP:"
        " records={}".format(
            len(mlp_records)
        )
    )

    if db_info is not None:
        print(
            "      INFO  DB:"
            " records={}".format(
                db_info.record_count
            )
        )
    else:
        print(
            "      INFO  DB: absent"
        )

    if map_name is not None:
        print(
            "      INFO  map.name:"
            " mapName={!r}"
            " centerLat={}"
            " centerLon={}".format(
                map_name.get("mapName"),
                map_name.get("centerLat"),
                map_name.get("centerLon"),
            )
        )


# ============================================================================
# Per-fixture audit
# ============================================================================

def audit_fixture(
    directory: Path,
    verbose: bool,
) -> AuditResult:
    """
    Audit one fixture directory.

    Expected common layout:

        layer.idx
        layer.mlp
        layer.db
        map.name

    A fixture may contain several layers.

    POI layer:

        pois.idx
        pois.db

    POI does not require pois.mlp.

    Standard layers:

        layer.idx
        layer.mlp

    A .db file is optional when a standard layer contains no names.
    """

    result = AuditResult()
    stats = LayerStats()

    idx_files = [
        path
        for path in sorted(directory.glob("*.idx"))
        if path.name != "search.idx"
    ]

    if not idx_files:
        result.warn(
            "No IDX files in directory"
        )

    for idx_path in idx_files:
        audit_single_layer(
            directory=directory,
            idx_path=idx_path,
            result=result,
            stats=stats,
            verbose=verbose,
        )

    # ------------------------------------------------------------------
    # search.idx (optional)
    # ------------------------------------------------------------------

    search_path = directory / "search.idx"

    if search_path.exists():
        audit_search_idx_header(
            search_path,
            result,
            verbose,
        )

    # ------------------------------------------------------------------
    # map.name
    # ------------------------------------------------------------------

    map_name_path = directory / "map.name"

    map_name = validate_map_name(
        map_name_path,
        result,
    )

    if verbose and map_name is None:
        print(
            "      INFO  map.name: invalid or missing"
        )

    if verbose and map_name is not None:
        print(
            "      INFO  map.name: checked"
        )
        print(
            "      INFO  map.name:"
            " mapName={!r}"
            " centerLat={}"
            " centerLon={}".format(
                map_name.get("mapName"),
                map_name.get("centerLat"),
                map_name.get("centerLon"),
            )
        )

    return result


def audit_search_idx_header(
    path: Path,
    result: AuditResult,
    verbose: bool,
) -> None:
    """
    search.idx (map-format-v4.md, section 21).

    Only the PGO container is checked here: type 4, payload size and
    zero LOD/reserved fields. The SRCH header, the sections and the
    dictionary are not audited yet.
    """

    try:
        data = path.read_bytes()
    except OSError as exc:
        result.fail(
            "{}: cannot read file: {}".format(
                path.name,
                exc,
            )
        )
        return

    parse_pgo_header(
        data,
        FILE_TYPE_SEARCH,
        result,
    )

    result.warn(
        "{}: only the PGO header is checked; "
        "SRCH header and sections are not audited yet".format(
            path.name
        )
    )

    if verbose:
        print(
            "      INFO  {}: {} B, PGO header checked".format(
                path.name,
                len(data),
            )
        )


def audit_single_layer(
    directory: Path,
    idx_path: Path,
    result: AuditResult,
    stats: LayerStats,
    verbose: bool,
) -> None:
    layer = layer_name_from_idx(idx_path)
    is_poi = layer.lower() == "pois"

    if verbose:
        print(
            "      INFO  layer {!r}".format(
                layer
            )
        )

    # ------------------------------------------------------------------
    # IDX
    # ------------------------------------------------------------------

    try:
        idx_data = idx_path.read_bytes()
    except OSError as exc:
        result.fail(
            "{}: cannot read file: {}".format(
                idx_path.name,
                exc,
            )
        )
        return

    stats.idx_size += len(idx_data)

    idx_audit = parse_idx(
        idx_data,
        result,
    )

    if idx_audit is None:
        return

    audit_lod_membership(
        idx=idx_audit,
        result=result,
    )

    if verbose:
        print(
            "      INFO  {}: PGO/IDX/SQT/NAV/DATA checked".format(
                idx_path.name
            )
        )

    # ------------------------------------------------------------------
    # MLP
    # ------------------------------------------------------------------

    mlp_path = directory / "{}.mlp".format(layer)

    mlp_records: List[GeometryRecord] = []

    if mlp_path.exists():
        try:
            mlp_data = mlp_path.read_bytes()
        except OSError as exc:
            result.fail(
                "{}: cannot read file: {}".format(
                    mlp_path.name,
                    exc,
                )
            )
            mlp_data = b""

        stats.mlp_size += len(mlp_data)

        if mlp_data:
            mlp_records = parse_mlp(
                mlp_data,
                result,
            )
        else:
            result.fail(
                "{}: empty file".format(
                    mlp_path.name
                )
            )

        audit_idx_mlp(
            idx=idx_audit,
            mlp_records=mlp_records,
            result=result,
            is_poi=is_poi,
        )

        audit_geometry_semantics(
            idx=idx_audit,
            mlp_records=mlp_records,
            result=result,
            is_poi=is_poi,
        )

        if not is_poi and idx_audit.version == 4:
            audit_lod_geometry_variants(
                idx=idx_audit,
                mlp_records=mlp_records,
                result=result,
            )

        if (
            not is_poi
            and layer.lower() in {
                "landuse",
                "water",
            }
        ):
            audit_polygon_closure(
                mlp_records,
                result,
            )

        if verbose:
            print(
                "      INFO  {}: {} geometry records checked".format(
                    mlp_path.name,
                    len(mlp_records),
                )
            )

    else:
        if is_poi:
            audit_idx_mlp(
                idx=idx_audit,
                mlp_records=[],
                result=result,
                is_poi=True,
            )

            if verbose:
                print(
                    "      INFO  {}: not present (valid for POI)".format(
                        mlp_path.name
                    )
                )
        else:
            result.fail(
                "{}: missing for non-POI layer".format(
                    mlp_path.name
                )
            )

    # ------------------------------------------------------------------
    # DB
    # ------------------------------------------------------------------

    db_path = directory / "{}.db".format(layer)

    db_info: Optional[DBInfo] = None

    if db_path.exists():
        try:
            db_data = db_path.read_bytes()
        except OSError as exc:
            result.fail(
                "{}: cannot read file: {}".format(
                    db_path.name,
                    exc,
                )
            )
            db_data = b""

        stats.db_size += len(db_data)

        if db_data:
            db_info = parse_db(
                data=db_data,
                result=result,
                is_poi=is_poi,
            )
        else:
            result.fail(
                "{}: empty file".format(
                    db_path.name
                )
            )

        if verbose:
            if db_info is not None:
                print(
                    "      INFO  {}: DBF checked, {} records".format(
                        db_path.name,
                        db_info.record_count,
                    )
                )
            else:
                print(
                    "      INFO  {}: DBF invalid".format(
                        db_path.name
                    )
                )

    else:
        if verbose:
            print(
                "      INFO  {}: absent".format(
                    db_path.name
                )
            )

    audit_idx_db(
        idx=idx_audit,
        db_info=db_info,
        result=result,
        is_poi=is_poi,
    )

    if verbose:
        print(
            "      INFO  {}: IDX <-> DB references checked".format(
                layer
            )
        )


# ============================================================================
# Human-readable summary
# ============================================================================

def print_layer_summary(
    directory: Path,
    result: AuditResult,
) -> None:
    status = "PASS" if result.passed else "FAIL"

    print(
        "[{}] {}".format(
            status,
            directory,
        )
    )

    for warning in result.warnings:
        print(
            "      WARN  {}".format(
                warning
            )
        )

    for error in result.errors:
        print(
            "      FAIL  {}".format(
                error
            )
        )

    if result.passed and not result.warnings:
        print(
            "      PASS  All binary conformance checks"
        )


# ============================================================================
# Main
# ============================================================================

def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Audit all PurrGO V4 binary map fixtures recursively."
        )
    )

    parser.add_argument(
        "root",
        nargs="?",
        default="tests/data/maps",
        help=(
            "Root directory containing map fixtures "
            "(default: tests/data/maps)"
        ),
    )

    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        help=(
            "Print detailed information about files and "
            "objects found and checked in every fixture"
        ),
    )

    args = parser.parse_args()

    root = Path(args.root)

    if not root.exists():
        print(
            "ERROR: directory does not exist: {}".format(root),
            file=sys.stderr,
        )
        return 2

    if not root.is_dir():
        print(
            "ERROR: not a directory: {}".format(root),
            file=sys.stderr,
        )
        return 2

    directories = discover_fixture_directories(
        root
    )

    if not directories:
        print(
            "ERROR: no map fixture files found under {}".format(
                root
            ),
            file=sys.stderr,
        )
        return 2

    total = 0
    passed = 0
    failed = 0

    for directory in directories:
        total += 1

        result = audit_fixture(
            directory=directory,
            verbose=args.verbose,
        )

        print_layer_summary(
            directory=directory,
            result=result,
        )

        if result.passed:
            passed += 1
        else:
            failed += 1

    print()
    print("=" * 72)
    print(
        "SUMMARY: {}/{} fixture directories passed".format(
            passed,
            total,
        )
    )

    if failed:
        print(
            "         {} fixture directories failed".format(
                failed
            )
        )
    else:
        print(
            "         all discovered fixture directories passed"
        )

    print("=" * 72)

    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())