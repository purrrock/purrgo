#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
PurrGO Map Format V3 — Binary Fixture Auditor

Audits all compiled map fixtures below a directory.

The parser intentionally follows the current PurrGO V3 binary writer
and the current firmware parser semantics:

    PGO
      ├── IDX
      │    ├── LOD 0
      │    ├── LOD 1
      │    └── LOD 2
      │          └── SQT
      │               ├── Navigation Nodes
      │               │     └── Navigation/Data children
      │               └── Data Nodes
      │
      ├── MLP
      │    └── Geometry Records
      │
      └── DB
           └── DBF

Important current V3 semantics:

    Data Node:
        <iiiiBII>
        size = 25 bytes

    Navigation Node:
        <IiiiiII>
        size = 28 bytes

    SQT Header:
        16 bytes

    MLP local header:
        >I + <I
        size = 8 bytes

    MLP v1:
        offset relative to the beginning of the MLP payload
        and points to Geometry Body, not the local 8-byte header.

        absolute_body_offset = 32 + v1

    Navigation v3_jump:
        exact byte size of the complete child subtree.
        It does NOT include the current 28-byte Navigation Node.

    DB v2:
        standard layer:
            0 = no DB reference
            1 = dummy record
            >=2 = physical named record

        POI:
            0 = unnamed
            >=1 = physical record

The script uses only Python standard library.
"""

from __future__ import annotations

import argparse
import struct
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Tuple


# ============================================================================
# Constants — current PurrGO V3 binary format
# ============================================================================

PGO_HEADER_SIZE = 32
SQT_HEADER_SIZE = 16

NAV_NODE_SIZE = 28
DATA_NODE_SIZE = 25

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

SQT_MAGIC = b"SQT\x01"
PGO_MAGIC = b"PGO"

DBF_MAGIC = 0x03
DBF_HEADER_TERMINATOR = 0x0D
DBF_RECORD_ACTIVE = 0x20

EXPECTED_DB_FIELDS = (
    ("osm_id", 12),
    ("code", 4),
    ("name", 100),
)


# ============================================================================
# Result / diagnostic structures
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
    lod_offsets: Tuple[int, int, int]
    extension_fields: Tuple[int, int, int]


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


@dataclass
class NavNode:
    offset: int
    v3_jump: int
    xmin: int
    ymin: int
    xmax: int
    ymax: int
    level: int
    child_count: int
    children: List[object] = field(default_factory=list)


@dataclass
class SQTInfo:
    offset: int
    mode: int
    root_count: int
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
    data_nodes: List[DataNode] = field(default_factory=list)
    nav_nodes: List[NavNode] = field(default_factory=list)
    sqts: List[SQTInfo] = field(default_factory=list)


# ============================================================================
# Binary helpers
# ============================================================================

def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def i32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<i", data, offset)[0]


def be_u32(data: bytes, offset: int) -> int:
    return struct.unpack_from(">I", data, offset)[0]


def require_range(
    data: bytes,
    offset: int,
    size: int,
    what: str,
    result: AuditResult,
) -> bool:
    """
    Check that [offset, offset + size) lies completely inside data.

    All variable-sized reads go through this check.
    """

    if offset < 0:
        result.fail(f"{what}: negative offset {offset}")
        return False

    if size < 0:
        result.fail(f"{what}: negative size {size}")
        return False

    if offset > len(data):
        result.fail(
            f"{what}: offset={offset} beyond file_size={len(data)}"
        )
        return False

    if size > len(data) - offset:
        result.fail(
            f"{what}: out of bounds "
            f"(offset={offset}, size={size}, file_size={len(data)})"
        )
        return False

    return True


def checked_add(
    a: int,
    b: int,
    what: str,
    result: AuditResult,
) -> Optional[int]:
    """
    Checked unsigned-32-bit addition.

    Binary offsets and sizes are uint32 in the format.
    """

    if a < 0 or b < 0:
        result.fail(f"{what}: negative operand")
        return None

    value = a + b

    if value > MAX_UINT32:
        result.fail(
            f"{what}: uint32 overflow "
            f"({a} + {b})"
        )
        return None

    return value


def checked_mul(
    a: int,
    b: int,
    what: str,
    result: AuditResult,
) -> Optional[int]:
    """
    Checked multiplication for variable-sized binary arrays.
    """

    if a < 0 or b < 0:
        result.fail(f"{what}: negative operand")
        return None

    value = a * b

    if value > MAX_UINT32:
        result.fail(
            f"{what}: uint32 overflow "
            f"({a} * {b})"
        )
        return None

    return value


# ============================================================================
# PGO header
# ============================================================================

def parse_pgo_header(
    data: bytes,
    expected_type: int,
    path: Path,
    result: AuditResult,
) -> Optional[PGOHeader]:
    """
    Parse and validate the 32-byte PGO header.
    """

    if len(data) < PGO_HEADER_SIZE:
        result.fail(
            f"PGO header: file is shorter than "
            f"{PGO_HEADER_SIZE} bytes"
        )
        return None

    if data[0:3] != PGO_MAGIC:
        result.fail(
            f"PGO header: invalid magic {data[0:3]!r}, "
            f"expected b'PGO'"
        )
        return None

    file_type = data[3]

    if file_type not in (
        FILE_TYPE_IDX,
        FILE_TYPE_MLP,
        FILE_TYPE_DB,
    ):
        result.fail(
            f"PGO header: invalid file type {file_type}"
        )
        return None

    if file_type != expected_type:
        result.fail(
            f"PGO header: file type={file_type}, "
            f"expected={expected_type}"
        )
        return None

    payload_size = u32(data, 4)

    expected_payload_size = len(data) - PGO_HEADER_SIZE

    if payload_size != expected_payload_size:
        result.fail(
            f"PGO header: Payload Size={payload_size}, "
            f"actual file payload={expected_payload_size}"
        )

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

    # The current writer writes all three extension fields as zero.
    for index, value in enumerate(extension_fields, start=1):
        if value != 0:
            result.fail(
                f"PGO header: Future Extension Field {index} "
                f"is {value}, expected 0"
            )

    payload_end = PGO_HEADER_SIZE + payload_size

    if payload_end != len(data):
        result.fail(
            f"PGO header: calculated payload end={payload_end}, "
            f"file_size={len(data)}"
        )

    if file_type == FILE_TYPE_IDX:
        validate_idx_lod_offsets(
            data,
            lod_offsets,
            result,
        )
    else:
        # Current writer explicitly writes 0,0,0 for MLP and DB.
        if lod_offsets != (0, 0, 0):
            result.fail(
                f"PGO header: non-IDX file has LOD offsets "
                f"{lod_offsets}, expected (0, 0, 0)"
            )

    return PGOHeader(
        file_type=file_type,
        payload_size=payload_size,
        lod_offsets=lod_offsets,
        extension_fields=extension_fields,
    )


def validate_idx_lod_offsets(
    data: bytes,
    lod_offsets: Tuple[int, int, int],
    result: AuditResult,
) -> None:
    """
    Current compiler writes three sequential LOD sections.

    Every offset points to the beginning of a 16-byte SQT header.
    """

    payload_start = PGO_HEADER_SIZE
    payload_end = len(data)

    for index, offset in enumerate(lod_offsets):
        if offset < payload_start:
            result.fail(
                f"IDX LOD {index}: offset={offset} "
                f"is before payload start={payload_start}"
            )
            continue

        if offset >= payload_end:
            result.fail(
                f"IDX LOD {index}: offset={offset} "
                f"is outside payload/file"
            )
            continue

        if offset + SQT_HEADER_SIZE > payload_end:
            result.fail(
                f"IDX LOD {index}: SQT header at offset={offset} "
                f"does not fit in file"
            )

    if lod_offsets[0] >= lod_offsets[1]:
        result.fail(
            f"IDX LOD offsets are not strictly increasing: "
            f"{lod_offsets}"
        )

    if lod_offsets[1] >= lod_offsets[2]:
        result.fail(
            f"IDX LOD offsets are not strictly increasing: "
            f"{lod_offsets}"
        )


# ============================================================================
# IDX / SQT
# ============================================================================

def parse_idx(
    data: bytes,
    path: Path,
    result: AuditResult,
) -> Optional[IndexAuditData]:
    """
    Parse the complete IDX file.

    The three LOD offsets define section boundaries:

        LOD0 = [lod0, lod1)
        LOD1 = [lod1, lod2)
        LOD2 = [lod2, EOF)
    """

    header = parse_pgo_header(
        data,
        FILE_TYPE_IDX,
        path,
        result,
    )

    if header is None:
        return None

    audit = IndexAuditData()

    lod_offsets = header.lod_offsets

    lod_ends = (
        lod_offsets[1],
        lod_offsets[2],
        len(data),
    )

    for lod_index in range(3):
        lod_start = lod_offsets[lod_index]
        lod_end = lod_ends[lod_index]

        parse_sqt(
            data=data,
            offset=lod_start,
            section_end=lod_end,
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
    Parse one SQT block.

    Layout:

        SQT header (16)
        root node #0
        root node #1
        ...

    Current writer semantics:

        mode == 0
            roots are Data Nodes

        mode > 0
            roots are Navigation Nodes

    For a non-empty tree, root Navigation Node level is:

        mode - 1

    This follows MapCompiler._build_rtree().
    """

    if section_end < offset:
        result.fail(
            f"LOD {lod_index}: section end before start"
        )
        return None

    if not require_range(
        data,
        offset,
        SQT_HEADER_SIZE,
        f"LOD {lod_index} SQT header",
        result,
    ):
        return None

    if offset + SQT_HEADER_SIZE > section_end:
        result.fail(
            f"LOD {lod_index}: SQT header crosses LOD boundary"
        )
        return None

    header = data[offset:offset + SQT_HEADER_SIZE]

    if header[0:4] != SQT_MAGIC:
        result.fail(
            f"LOD {lod_index}: invalid SQT magic "
            f"{header[0:4]!r}"
        )
        return None

    topology = u32(header, 4)
    mode = u32(header, 8)
    root_count = u32(header, 12)

    if topology != 1:
        result.fail(
            f"LOD {lod_index}: unsupported SQT topology={topology}, "
            f"expected 1"
        )

    sqt = SQTInfo(
        offset=offset,
        mode=mode,
        root_count=root_count,
    )

    audit.sqts.append(sqt)

    current = offset + SQT_HEADER_SIZE

    for root_index in range(root_count):
        if mode == 0:
            node, current = parse_data_node(
                data=data,
                offset=current,
                section_end=section_end,
                lod_index=lod_index,
                context=f"LOD {lod_index} root DATA[{root_index}]",
                audit=audit,
                result=result,
            )
        else:
            node, current = parse_nav_node(
                data=data,
                offset=current,
                section_end=section_end,
                lod_index=lod_index,
                context=f"LOD {lod_index} root NAV[{root_index}]",
                audit=audit,
                result=result,
                expected_level=mode - 1,
            )

        if node is not None:
            sqt.roots.append(node)

    # The current writer serializes exactly one LOD section and the next
    # LOD offset is its physical end.
    if current != section_end:
        result.fail(
            f"LOD {lod_index}: parsed section ends at {current}, "
            f"but LOD boundary is {section_end}; "
            f"unparsed bytes={section_end - current}"
        )

    return sqt


def parse_data_node(
    data: bytes,
    offset: int,
    section_end: int,
    lod_index: int,
    context: str,
    audit: IndexAuditData,
    result: AuditResult,
) -> Tuple[Optional[DataNode], int]:
    """
    Parse a 25-byte Data Node.

    Binary format:

        <iiiiBII>

        0x00 xmin
        0x04 ymin
        0x08 xmax
        0x0C ymax
        0x10 Type
        0x11 v1
        0x15 v2
    """

    if offset + DATA_NODE_SIZE > section_end:
        result.fail(
            f"{context}: Data Node out of bounds "
            f"(offset={offset}, size={DATA_NODE_SIZE}, "
            f"section_end={section_end})"
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

    xmin = i32(data, offset + 0)
    ymin = i32(data, offset + 4)
    xmax = i32(data, offset + 8)
    ymax = i32(data, offset + 12)

    feature_type = data[offset + 16]

    v1 = u32(data, offset + 17)
    v2 = u32(data, offset + 21)

    if xmin > xmax:
        result.fail(
            f"{context}: invalid BBox xmin={xmin} > xmax={xmax}"
        )

    if ymin > ymax:
        result.fail(
            f"{context}: invalid BBox ymin={ymin} > ymax={ymax}"
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
    section_end: int,
    lod_index: int,
    context: str,
    audit: IndexAuditData,
    result: AuditResult,
    expected_level: Optional[int] = None,
) -> Tuple[Optional[NavNode], int]:
    """
    Parse a 28-byte Navigation Node recursively.

    Binary format:

        <IiiiiII>

        0x00 v3_jump
        0x04 xmin
        0x08 ymin
        0x0C xmax
        0x10 ymax
        0x14 level
        0x18 child_count

    v3_jump is the exact byte size of all immediate/recursive children.

    Therefore:

        subtree_start = offset + 28
        subtree_end   = subtree_start + v3_jump

    When parsing the children, the final cursor must equal subtree_end.
    """

    if offset + NAV_NODE_SIZE > section_end:
        result.fail(
            f"{context}: Navigation Node out of bounds "
            f"(offset={offset}, size={NAV_NODE_SIZE}, "
            f"section_end={section_end})"
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

    v3_jump = u32(data, offset + 0)

    xmin = i32(data, offset + 4)
    ymin = i32(data, offset + 8)
    xmax = i32(data, offset + 12)
    ymax = i32(data, offset + 16)

    level = u32(data, offset + 20)
    child_count = u32(data, offset + 24)

    if xmin > xmax:
        result.fail(
            f"{context}: invalid BBox xmin={xmin} > xmax={xmax}"
        )

    if ymin > ymax:
        result.fail(
            f"{context}: invalid BBox ymin={ymin} > ymax={ymax}"
        )

    if expected_level is not None and level != expected_level:
        result.fail(
            f"{context}: level={level}, "
            f"expected={expected_level}"
        )

    subtree_start = offset + NAV_NODE_SIZE

    subtree_end = checked_add(
        subtree_start,
        v3_jump,
        f"{context}: v3_jump",
        result,
    )

    if subtree_end is None:
        return None, offset

    if subtree_end > section_end:
        result.fail(
            f"{context}: v3_jump exceeds LOD boundary "
            f"(subtree_end={subtree_end}, "
            f"section_end={section_end})"
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

    # Current V3 semantics:
    #
    #   level == 0
    #       children are Data Nodes
    #
    #   level > 0
    #       children are Navigation Nodes with level - 1
    #
    child_is_nav = level > 0

    for child_index in range(child_count):
        if child_is_nav:
            child, new_current = parse_nav_node(
                data=data,
                offset=current,
                section_end=subtree_end,
                lod_index=lod_index,
                context=(
                    f"{context} child NAV[{child_index}]"
                ),
                audit=audit,
                result=result,
                expected_level=level - 1,
            )
        else:
            child, new_current = parse_data_node(
                data=data,
                offset=current,
                section_end=subtree_end,
                lod_index=lod_index,
                context=(
                    f"{context} child DATA[{child_index}]"
                ),
                audit=audit,
                result=result,
            )

        if child is None:
            return None, offset

        node.children.append(child)
        current = new_current

    # This is the most important v3_jump conformance check.
    #
    # The serialized subtree must contain exactly v3_jump bytes.
    if current != subtree_end:
        result.fail(
            f"{context}: v3_jump mismatch: "
            f"declared={v3_jump}, "
            f"actual_children_size={current - subtree_start}"
        )

    # v3_jump == 0 is only structurally valid for a node with zero children.
    if child_count == 0 and v3_jump != 0:
        result.fail(
            f"{context}: child_count=0 but v3_jump={v3_jump}"
        )

    if child_count > 0 and v3_jump == 0:
        result.fail(
            f"{context}: child_count={child_count} "
            f"but v3_jump=0"
        )

    return node, subtree_end


# ============================================================================
# MLP
# ============================================================================

def parse_mlp(
    data: bytes,
    path: Path,
    result: AuditResult,
) -> List[GeometryRecord]:
    """
    Parse every MLP geometry record.

    Current writer:

        Local Header:
            sequence: BE uint32
            body size: LE uint32

        Body:
            BBox: 16 bytes
            num_parts: uint32
            num_points: uint32
            parts[]
            points[]

    v1 references the body relative to the PGO payload start.
    """

    header = parse_pgo_header(
        data,
        FILE_TYPE_MLP,
        path,
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
                f"MLP record at offset={record_offset}: "
                f"sequence={sequence}, "
                f"expected={sequence_expected}"
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
                f"MLP record at offset={record_offset}: "
                f"Content Length={content_length} exceeds file "
                f"(body_offset={body_offset}, "
                f"record_end={record_end}, "
                f"file_size={len(data)})"
            )
            break

        if content_length < MLP_BODY_FIXED_SIZE:
            result.fail(
                f"MLP record at offset={record_offset}: "
                f"Content Length={content_length} < "
                f"minimum body size={MLP_BODY_FIXED_SIZE}"
            )
            break

        # Fixed body header.
        bbox_offset = body_offset

        xmin = i32(data, bbox_offset + 0)
        ymin = i32(data, bbox_offset + 4)
        xmax = i32(data, bbox_offset + 8)
        ymax = i32(data, bbox_offset + 12)

        # Writer uses <II here. Current C parser interprets the same
        # 32-bit representation as signed counts. Valid compiler output
        # is non-negative, so unsigned parsing is the natural binary audit.
        num_parts = u32(data, bbox_offset + 16)
        num_points = u32(data, bbox_offset + 20)

        if xmin > xmax:
            result.fail(
                f"MLP record #{sequence}: "
                f"invalid BBox xmin={xmin} > xmax={xmax}"
            )

        if ymin > ymax:
            result.fail(
                f"MLP record #{sequence}: "
                f"invalid BBox ymin={ymin} > ymax={ymax}"
            )

        parts_bytes = checked_mul(
            num_parts,
            4,
            f"MLP record #{sequence}: parts size",
            result,
        )

        points_bytes = checked_mul(
            num_points,
            8,
            f"MLP record #{sequence}: points size",
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
                f"MLP record #{sequence}: Content Length mismatch: "
                f"declared={content_length}, "
                f"calculated={expected_body_size}"
            )
            # We still stop parsing this record because the array layout
            # cannot be trusted after this point.
            break

        parts_offset = body_offset + MLP_BODY_FIXED_SIZE
        points_offset = parts_offset + parts_bytes

        if points_offset + points_bytes != record_end:
            result.fail(
                f"MLP record #{sequence}: "
                f"calculated geometry end does not equal record end"
            )
            break

        parts: List[int] = []

        for i in range(num_parts):
            part_value = u32(
                data,
                parts_offset + i * 4,
            )
            parts.append(part_value)

        validate_parts(
            parts,
            num_parts,
            num_points,
            sequence,
            result,
        )

        points: List[Tuple[int, int]] = []

        # Do not allocate an artificial temporary array.
        # Iterate directly over the binary data.
        for i in range(num_points):
            point_offset = points_offset + i * 8

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
            f"MLP parser ended at offset={cursor}, "
            f"file_size={len(data)}"
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
    Validate current PurrGO parts[] semantics.

        parts[0] == 0

        parts[i] < num_points

        parts[i] > parts[i-1]
    """

    if num_parts == 0:
        return

    if num_points == 0:
        result.fail(
            f"MLP record #{sequence}: "
            f"num_parts={num_parts}, num_points=0"
        )
        return

    if parts[0] != 0:
        result.fail(
            f"MLP record #{sequence}: "
            f"parts[0]={parts[0]}, expected 0"
        )

    for i in range(1, num_parts):
        if parts[i] >= num_points:
            result.fail(
                f"MLP record #{sequence}: "
                f"parts[{i}]={parts[i]} >= num_points={num_points}"
            )

        if parts[i] <= parts[i - 1]:
            result.fail(
                f"MLP record #{sequence}: "
                f"parts[] not strictly increasing at index {i}: "
                f"{parts[i - 1]} -> {parts[i]}"
            )


# ============================================================================
# DBF
# ============================================================================

def parse_db(
    data: bytes,
    path: Path,
    result: AuditResult,
    is_poi: bool,
) -> Optional[DBInfo]:
    """
    Parse the PGO/DBF container produced by MapCompiler.compile_db().
    """

    header = parse_pgo_header(
        data,
        FILE_TYPE_DB,
        path,
        result,
    )

    if header is None:
        return None

    if len(data) < PGO_HEADER_SIZE + DBF_HEADER_SIZE:
        result.fail(
            f"DBF: file is too short for "
            f"PGO + {DBF_HEADER_SIZE}-byte DBF header"
        )
        return None

    dbf = data[PGO_HEADER_SIZE:]

    if dbf[0] != DBF_MAGIC:
        result.fail(
            f"DBF: invalid magic 0x{dbf[0]:02X}, "
            f"expected 0x03"
        )

    # bytes 1..3 are the current writer's 00 00 00.
    if dbf[1:4] != b"\x00\x00\x00":
        result.fail(
            f"DBF: bytes 0x01..0x03 are "
            f"{dbf[1:4].hex(' ')}, expected 00 00 00"
        )

    record_count = u32(dbf, 4)
    header_size = struct.unpack_from("<H", dbf, 8)[0]
    record_size = struct.unpack_from("<H", dbf, 10)[0]

    if header_size != DBF_HEADER_SIZE:
        result.fail(
            f"DBF: header size={header_size}, "
            f"expected={DBF_HEADER_SIZE}"
        )

    if record_size != DBF_RECORD_SIZE:
        result.fail(
            f"DBF: record size={record_size}, "
            f"expected={DBF_RECORD_SIZE}"
        )

    # Current writer creates:
    #
    #   32-byte DBF fixed header
    #   3 * 32-byte descriptors
    #   0x0D
    #
    # = 129 bytes.
    descriptor_start = 32

    fields: List[Tuple[str, int]] = []

    for index, (expected_name, expected_length) in enumerate(
        EXPECTED_DB_FIELDS
    ):
        offset = descriptor_start + index * DBF_DESCRIPTOR_SIZE

        if offset + DBF_DESCRIPTOR_SIZE > len(dbf):
            result.fail(
                f"DBF: descriptor {index + 1} "
                f"out of bounds"
            )
            return None

        descriptor = dbf[
            offset:
            offset + DBF_DESCRIPTOR_SIZE
        ]

        raw_name = descriptor[0:11]
        field_name = raw_name.split(b"\x00", 1)[0].decode(
            "ascii",
            errors="replace",
        )

        field_type = descriptor[11]
        field_length = descriptor[16]

        if field_name != expected_name:
            result.fail(
                f"DBF descriptor {index + 1}: "
                f"name={field_name!r}, "
                f"expected={expected_name!r}"
            )

        if field_type != ord("C"):
            result.fail(
                f"DBF descriptor {index + 1} "
                f"{expected_name}: type=0x{field_type:02X}, "
                f"expected ASCII 'C'"
            )

        if field_length != expected_length:
            result.fail(
                f"DBF descriptor {index + 1} "
                f"{expected_name}: length={field_length}, "
                f"expected={expected_length}"
            )

        # The writer sets all reserved descriptor bytes to zero:
        #
        #   0x0C..0x0F = 4 bytes
        #   0x11..0x1F = 15 bytes
        #
        if descriptor[12:16] != b"\x00" * 4:
            result.fail(
                f"DBF descriptor {index + 1} "
                f"{expected_name}: reserved bytes 0x0C..0x0F "
                f"are not zero"
            )

        if descriptor[17:32] != b"\x00" * 15:
            result.fail(
                f"DBF descriptor {index + 1} "
                f"{expected_name}: reserved bytes 0x11..0x1F "
                f"are not zero"
            )

        fields.append(
            (field_name, field_length)
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
            f"DBF: header terminator=0x"
            f"{dbf[terminator_offset]:02X}, "
            f"expected 0x0D"
        )

    if header_size != terminator_offset + 1:
        result.fail(
            f"DBF: header structure ends at "
            f"{terminator_offset + 1}, "
            f"but header_size={header_size}"
        )

    records_start = header_size

    expected_dbf_size = (
        records_start
        + record_count * record_size
    )

    if expected_dbf_size != len(dbf):
        result.fail(
            f"DBF: file size mismatch: "
            f"header_size={records_start} + "
            f"records={record_count} * "
            f"record_size={record_size} = "
            f"{expected_dbf_size}, "
            f"actual DBF payload={len(dbf)}"
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
            f"DBF record {record_index}",
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

        # Current writer emits:
        #
        #   deletion marker
        #   osm_id[12]
        #   code[4]
        #   name[100]
        #
        # = 117 bytes exactly.
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

    # Standard layers:
    #
    #   record 1 = dummy
    #
    # POI:
    #
    #   there is no dummy record.
    #
    if not is_poi:
        if record_count < 1:
            result.fail(
                "DBF standard layer: "
                "record_count must be at least 1 "
                "because record 1 is the dummy record"
            )
        else:
            dummy = records.get(1)

            if dummy is not None:
                if any(dummy_byte != 0 for dummy_byte in
                       dbf[dummy.offset:dummy.offset + DBF_RECORD_SIZE]):
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
# Cross-file IDX ↔ MLP
# ============================================================================

def audit_idx_mlp(
    idx: IndexAuditData,
    mlp_records: List[GeometryRecord],
    mlp_data: bytes,
    result: AuditResult,
    is_poi: bool,
) -> None:
    """
    Verify Data Node v1 references.

    Current writer:

        feature.v1 = len(bin_records) + 8

    Current firmware:

        absolute_body_offset = 32 + v1

    Therefore v1 must identify a Geometry Body, not a local header.
    """

    by_v1: Dict[int, GeometryRecord] = {}

    for record in mlp_records:
        # Body offset relative to PGO payload.
        relative_body_offset = (
            record.body_offset - PGO_HEADER_SIZE
        )

        if relative_body_offset in by_v1:
            result.fail(
                f"MLP: duplicate v1/body offset "
                f"{relative_body_offset}"
            )

        by_v1[relative_body_offset] = record

    if is_poi:
        # Native POIs have no MLP geometry.
        for node in idx.data_nodes:
            if node.v1 != 0:
                result.fail(
                    f"POI Data Node at offset={node.offset}: "
                    f"v1={node.v1}, expected 0"
                )

        if mlp_records:
            result.warn(
                "POI layer has an MLP file containing geometry "
                "records; native POI format does not require MLP"
            )

        return

    for node in idx.data_nodes:
        if node.v1 == 0:
            result.fail(
                f"Data Node at offset={node.offset}: "
                f"v1=0 for geometry layer"
            )
            continue

        record = by_v1.get(node.v1)

        if record is None:
            result.fail(
                f"Data Node at offset={node.offset}: "
                f"v1={node.v1} does not reference an MLP "
                f"Geometry Body"
            )
            continue

        expected_absolute = (
            PGO_HEADER_SIZE + node.v1
        )

        if expected_absolute != record.body_offset:
            result.fail(
                f"Data Node at offset={node.offset}: "
                f"v1={node.v1} resolves to absolute "
                f"{expected_absolute}, but MLP body is at "
                f"{record.body_offset}"
            )

        # Data Node and Geometry Body must describe the same BBox.
        if (
            node.xmin != record.xmin
            or node.ymin != record.ymin
            or node.xmax != record.xmax
            or node.ymax != record.ymax
        ):
            result.fail(
                f"Data Node at offset={node.offset}: "
                f"BBox {node.xmin,node.ymin,node.xmax,node.ymax} "
                f"does not match MLP record #{record.sequence} "
                f"BBox {record.xmin,record.ymin,record.xmax,record.ymax}"
            )


# ============================================================================
# Cross-file IDX ↔ DB
# ============================================================================

def audit_idx_db(
    idx: IndexAuditData,
    db_info: Optional[DBInfo],
    result: AuditResult,
    is_poi: bool,
) -> None:
    """
    Verify Data Node v2 against DBF record numbering.

    Current compiler:

        Standard layer:
            v2=0 if .db omitted
            v2=1 dummy
            v2>=2 named physical records

        POI:
            v2=0 unnamed
            v2>=1 physical record
    """

    if db_info is None:
        # No DB exists.
        #
        # The only valid possibility for a standard geometry layer
        # is v2=0 for every feature.
        #
        # POI without DB may also use v2=0.
        for node in idx.data_nodes:
            if node.v2 != 0:
                result.fail(
                    f"Data Node at offset={node.offset}: "
                    f"v2={node.v2}, but .db is absent"
                )
        return

    for node in idx.data_nodes:
        v2 = node.v2

        if is_poi:
            if v2 == 0:
                continue

            if v2 > db_info.record_count:
                result.fail(
                    f"POI Data Node at offset={node.offset}: "
                    f"v2={v2} exceeds DB record_count="
                    f"{db_info.record_count}"
                )
            continue

        # Standard layer.
        if v2 == 0:
            result.fail(
                f"Standard Data Node at offset={node.offset}: "
                f"v2=0 while .db exists; "
                f"current writer uses v2=1 for unnamed features"
            )
            continue

        if v2 > db_info.record_count:
            result.fail(
                f"Standard Data Node at offset={node.offset}: "
                f"v2={v2} exceeds DB record_count="
                f"{db_info.record_count}"
            )
            continue

        if v2 == 1:
            # Dummy record is valid.
            continue

        # v2 >= 2 refers to a physical named record.
        record = db_info.records.get(v2)

        if record is None:
            result.fail(
                f"Standard Data Node at offset={node.offset}: "
                f"v2={v2} has no DB record"
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
    Additional semantic checks that are directly supported by the
    current binary model.

    These do not attempt to infer OSM semantics from feature codes.
    """

    if is_poi:
        for node in idx.data_nodes:
            if node.xmin != node.xmax:
                result.fail(
                    f"POI Data Node at offset={node.offset}: "
                    f"xmin={node.xmin} != xmax={node.xmax}"
                )

            if node.ymin != node.ymax:
                result.fail(
                    f"POI Data Node at offset={node.offset}: "
                    f"ymin={node.ymin} != ymax={node.ymax}"
                )

        return

    for record in mlp_records:
        if record.num_points == 0:
            result.fail(
                f"MLP record #{record.sequence}: "
                f"num_points=0"
            )

        if record.num_parts == 0:
            result.fail(
                f"MLP record #{record.sequence}: "
                f"num_parts=0 for geometry"
            )

        # Check that the stored BBox actually encloses all points.
        #
        # This is a useful writer conformance check and does not depend
        # on rendering.
        for point_index, (x, y) in enumerate(record.points):
            if x < record.xmin or x > record.xmax:
                result.fail(
                    f"MLP record #{record.sequence}: "
                    f"point[{point_index}].x={x} "
                    f"outside BBox"
                )
                break

            if y < record.ymin or y > record.ymax:
                result.fail(
                    f"MLP record #{record.sequence}: "
                    f"point[{point_index}].y={y} "
                    f"outside BBox"
                )
                break

        # Check ring closure for every part.
        for part_index, start in enumerate(record.parts):
            if part_index + 1 < record.num_parts:
                end = record.parts[part_index + 1]
            else:
                end = record.num_points

            if end <= start:
                result.fail(
                    f"MLP record #{record.sequence}: "
                    f"part {part_index} has empty/negative range "
                    f"[{start}, {end})"
                )
                continue

            first = record.points[start]
            last = record.points[end - 1]

            # We do not require closure for all geometry types here.
            #
            # Layer semantics are not encoded in MLP itself.
            #
            # Therefore this is intentionally not an error.
            #
            # Polygon-specific closure is checked only when the fixture
            # is known to be a polygon layer by filename.
            _ = first
            _ = last


def audit_polygon_closure(
    mlp_records: List[GeometryRecord],
    result: AuditResult,
) -> None:
    """
    Polygon-layer-specific topology check.

    Layer identity is derived only from the filename/directory convention
    used by the fixture set, not guessed from the binary itself.
    """

    for record in mlp_records:
        for part_index, start in enumerate(record.parts):
            end = (
                record.parts[part_index + 1]
                if part_index + 1 < record.num_parts
                else record.num_points
            )

            if end <= start:
                continue

            first = record.points[start]
            last = record.points[end - 1]

            if first != last:
                result.fail(
                    f"MLP record #{record.sequence}: "
                    f"polygon part {part_index} is not closed "
                    f"(first={first}, last={last})"
                )

            # Signed doubled area:
            #
            # > 0 => one winding direction
            # < 0 => the opposite
            #
            # We only calculate it here for diagnostics. Which direction
            # is outer/inner depends on part role and is not encoded in
            # the binary itself.
            #
            # The current V3 specification says:
            #
            #   outer = CW
            #   holes = CCW
            #
            # But the binary has no explicit outer/inner role field.
            #
            # Therefore do not reject solely on winding here.
            _ = polygon_signed_area2(
                record.points[start:end]
            )


def polygon_signed_area2(
    points: List[Tuple[int, int]],
) -> int:
    """
    Twice the signed polygon area.

    Integer-only implementation.

    Positive/negative sign is useful for diagnostics without
    floating-point arithmetic.
    """

    if len(points) < 3:
        return 0

    area = 0

    for i in range(len(points) - 1):
        x1, y1 = points[i]
        x2, y2 = points[i + 1]

        area += x1 * y2
        area -= x2 * y1

    return area


# ============================================================================
# Fixture discovery
# ============================================================================

def discover_layers(
    root: Path,
) -> List[Path]:
    """
    Find every directory containing at least one .idx/.mlp/.db file.

    This intentionally scans recursively so the auditor can be run against:

        tests/data/maps

    without maintaining a hard-coded fixture list.
    """

    directories = set()

    for suffix in (".idx", ".mlp", ".db"):
        for path in root.rglob(f"*{suffix}"):
            if path.is_file():
                directories.add(path.parent)

    return sorted(directories)


def layer_name_from_idx(idx_path: Path) -> str:
    """
    Layer name is the IDX basename.

    Example:

        roads.idx -> roads
    """

    return idx_path.stem


# ============================================================================
# Per-layer audit
# ============================================================================

def audit_layer(
    directory: Path,
) -> AuditResult:
    """
    Audit all files belonging to one fixture directory.

    Expected normal layout:

        layer.idx
        layer.mlp
        layer.db

    For POI:

        pois.idx
        pois.db
        no pois.mlp required
    """

    result = AuditResult()

    idx_files = sorted(directory.glob("*.idx"))

    if not idx_files:
        result.warn(
            "No IDX file in directory"
        )
        return result

    # A fixture directory normally contains one layer. If there are
    # multiple IDX files, audit each one independently.
    for idx_path in idx_files:
        audit_single_layer(
            directory=directory,
            idx_path=idx_path,
            result=result,
        )

    return result


def audit_single_layer(
    directory: Path,
    idx_path: Path,
    result: AuditResult,
) -> None:
    layer = layer_name_from_idx(idx_path)

    is_poi = layer.lower() == "pois"

    try:
        idx_data = idx_path.read_bytes()
    except OSError as exc:
        result.fail(
            f"{idx_path.name}: cannot read file: {exc}"
        )
        return

    idx_audit = parse_idx(
        idx_data,
        idx_path,
        result,
    )

    if idx_audit is None:
        return

    # ------------------------------------------------------------------
    # MLP
    # ------------------------------------------------------------------

    mlp_path = directory / f"{layer}.mlp"

    mlp_records: List[GeometryRecord] = []

    if mlp_path.exists():
        try:
            mlp_data = mlp_path.read_bytes()
        except OSError as exc:
            result.fail(
                f"{mlp_path.name}: cannot read file: {exc}"
            )
            mlp_data = b""

        if mlp_data:
            mlp_records = parse_mlp(
                mlp_data,
                mlp_path,
                result,
            )
        else:
            # A zero-byte file is not a valid PGO container.
            result.fail(
                f"{mlp_path.name}: empty file"
            )

        audit_idx_mlp(
            idx=idx_audit,
            mlp_records=mlp_records,
            mlp_data=mlp_data,
            result=result,
            is_poi=is_poi,
        )

        audit_geometry_semantics(
            idx=idx_audit,
            mlp_records=mlp_records,
            result=result,
            is_poi=is_poi,
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

    else:
        # POI is explicitly allowed not to have MLP.
        if is_poi:
            audit_idx_mlp(
                idx=idx_audit,
                mlp_records=[],
                mlp_data=b"",
                result=result,
                is_poi=True,
            )
        else:
            result.fail(
                f"{layer}.mlp: missing for non-POI layer"
            )

    # ------------------------------------------------------------------
    # DB
    # ------------------------------------------------------------------

    db_path = directory / f"{layer}.db"

    db_info: Optional[DBInfo] = None

    if db_path.exists():
        try:
            db_data = db_path.read_bytes()
        except OSError as exc:
            result.fail(
                f"{db_path.name}: cannot read file: {exc}"
            )
            db_data = b""

        if db_data:
            db_info = parse_db(
                db_data,
                db_path,
                result,
                is_poi=is_poi,
            )
        else:
            result.fail(
                f"{db_path.name}: empty file"
            )
    else:
        db_info = None

    audit_idx_db(
        idx=idx_audit,
        db_info=db_info,
        result=result,
        is_poi=is_poi,
    )


# ============================================================================
# Human-readable report
# ============================================================================

def format_size(value: int) -> str:
    if value < 1024:
        return f"{value} B"

    if value < 1024 * 1024:
        return f"{value / 1024:.1f} KiB"

    return f"{value / (1024 * 1024):.1f} MiB"


def print_layer_summary(
    directory: Path,
    result: AuditResult,
) -> None:
    relative = directory

    status = "PASS" if result.passed else "FAIL"

    print(f"[{status}] {relative}")

    for warning in result.warnings:
        print(f"      WARN  {warning}")

    for error in result.errors:
        print(f"      FAIL  {error}")

    if result.passed and not result.warnings:
        print("      PASS  All binary conformance checks")


# ============================================================================
# Main
# ============================================================================

def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Audit all PurrGO V3 binary map fixtures "
            "recursively."
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

    args = parser.parse_args()

    root = Path(args.root)

    if not root.exists():
        print(
            f"ERROR: directory does not exist: {root}",
            file=sys.stderr,
        )
        return 2

    if not root.is_dir():
        print(
            f"ERROR: not a directory: {root}",
            file=sys.stderr,
        )
        return 2

    directories = discover_layers(root)

    if not directories:
        print(
            f"ERROR: no .idx/.mlp/.db files found under {root}",
            file=sys.stderr,
        )
        return 2

    total = 0
    passed = 0
    failed = 0

    for directory in directories:
        total += 1

        result = audit_layer(directory)

        print_layer_summary(
            directory,
            result,
        )

        if result.passed:
            passed += 1
        else:
            failed += 1

    print()
    print("=" * 72)
    print(
        f"SUMMARY: {passed}/{total} fixture directories passed"
    )

    if failed:
        print(
            f"         {failed} fixture directories failed"
        )
    else:
        print(
            "         all discovered fixture directories passed"
        )

    print("=" * 72)

    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())