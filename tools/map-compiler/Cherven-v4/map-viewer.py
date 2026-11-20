#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
PurrGO MAP FORMAT V3 / V4 — PC MAP VIEWER / VALIDATOR

Strict validator and viewer for PurrGO MAP FORMAT V3 and V4
(docs/map-format-v4.md). The format of a layer is detected from the
SQT signature of its .idx file (SQT\x01 = V3, SQT\x02 = V4).

V4 usage:

    python map-viewer.py roads.idx roads.mlp [map.name] [roads.db]
        [--lod N] [--size-km X] [--screenshot out.png]
    python map-viewer.py pois.idx - map.name          (layer without .mlp)
    python map-viewer.py search.idx [pois.db]          (validation only)

In the window keys 0..3 switch the displayed LOD of a V4 layer. All four LOD
sections are validated structurally; geometry records that fall into the
camera are validated for every LOD.

Supported files:

    .idx  File Type = 1
    .mlp  File Type = 2
    .db   File Type = 3

Global PGO header:

    0x00  char[3]   "PGO"
    0x03  uint8     File Type
    0x04  uint32    Payload Size
    0x08  uint32    LOD 0 Offset
    0x0C  uint32    LOD 1 Offset
    0x10  uint32    LOD 2 Offset
    0x14  uint32    Reserved / Future Extension
    0x18  uint32    Reserved / Future Extension
    0x1C  uint32    Reserved / Future Extension

All multi-byte global-header fields are Little-Endian.

The header is exactly 32 bytes.

For .idx files the LOD offsets are absolute file offsets and
must point to the beginning of the corresponding SQT section.

For .mlp and .db files all three LOD offsets must currently be zero.

This parser is intentionally strict. Structural inconsistencies
are reported as format errors instead of being silently accepted.
"""

import argparse
import contextlib
import json
import math
import os
import struct
import sys

import pygame


# ============================================================
# Format constants
# ============================================================

PGO_HEADER_SIZE = 32

PGO_FILE_TYPE_IDX = 1
PGO_FILE_TYPE_MLP = 2
PGO_FILE_TYPE_DB = 3

DATA_NODE_SIZE = 25
NAV_NODE_SIZE = 28

SQT_HEADER_SIZE = 16
SQT_MAGIC = b"SQT\x01"

# --- V4 (docs/map-format-v4.md) ---
SQT_MAGIC_V4 = b"SQT\x02"
V4_BLOCK_SIZE = 512
V4_NAV_FANOUT = 18
V4_LEAF_FANOUT = 20
V4_LOD_COUNT = 4
V4_FIRST_SECTION = 0x200

PGO_FILE_TYPE_SEARCH = 4

MLP_RECORD_HEADER_SIZE = 8
MLP_BODY_HEADER_SIZE = 24

COORD_SCALE = 10_000_000.0

MAX_NUM_POINTS = 50_000
MAX_NUM_PARTS = 10_000

UINT32_MAX = 0xFFFFFFFF


# ============================================================
# Viewer configuration
# ============================================================

WIDTH = 600
HEIGHT = 600


# ============================================================
# Diagnostic statistics
# ============================================================

def create_stats():
    """Create a fresh statistics dictionary for one validation run."""

    return {
        "sqt_sections": 0,

        "nav_visited": 0,
        "nav_culled": 0,

        "data_visited": 0,
        "data_drawn": 0,

        "geometry_drawn": 0,
        "lines_drawn": 0,
        "polygons_drawn": 0,

        "pois_drawn": 0,

        "bytes_skipped": 0,

        "geometry_checked": 0,
    }


stats = create_stats()


# ============================================================
# Error handling
# ============================================================

class MapFormatError(Exception):
    """Raised when a binary map violates the PurrGO V3 format."""


def fail(message):
    """Raise a strict map-format validation error."""

    raise MapFormatError(message)


def read_exact(file, size, description):
    """
    Read exactly `size` bytes.

    A short read is always a format error.
    """

    data = file.read(size)

    if len(data) != size:
        fail(
            f"Unexpected EOF while reading {description}: "
            f"expected {size} bytes, got {len(data)}"
        )

    return data


def file_size(file):
    """Return the physical size of an already opened file."""

    return os.fstat(file.fileno()).st_size


def validate_uint32(value, description):
    """Validate a value which must fit into uint32."""

    if value < 0 or value > UINT32_MAX:
        fail(
            f"{description} is outside uint32 range: {value}"
        )


# ============================================================
# Camera
# ============================================================

def load_camera_bbox(map_name_path, size_km=5.0):
    """
    Load map center from map.name and calculate a camera BBox.

    map.name stores ordinary geographic degrees.

    Binary map coordinates use integer degrees * 10^7.
    """

    if not os.path.exists(map_name_path):

        print(
            f"[WARN] Файл {map_name_path} не найден. "
            f"Используются координаты по умолчанию."
        )

        center_lat = 55.7558
        center_lon = 37.6173

    else:

        with open(
            map_name_path,
            "r",
            encoding="utf-8",
        ) as file:

            data = json.load(file)

        center_lat = float(data["centerLat"])
        center_lon = float(data["centerLon"])

        print(
            f"[INFO] Прочитан {map_name_path}: "
            f"centerLat={center_lat}, "
            f"centerLon={center_lon}"
        )

    half_km = size_km / 2.0

    d_lat = half_km / 111.139

    d_lon = half_km / (
        111.139 * math.cos(
            math.radians(center_lat)
        )
    )

    min_x = center_lon - d_lon
    max_x = center_lon + d_lon

    min_y = center_lat - d_lat
    max_y = center_lat + d_lat

    print(
        f"[INFO] BBox камеры ({size_km:g}x{size_km:g} км):"
    )

    print(
        f"       Lon (X): "
        f"min={min_x:.7f}, max={max_x:.7f}"
    )

    print(
        f"       Lat (Y): "
        f"min={min_y:.7f}, max={max_y:.7f}"
    )

    return min_x, min_y, max_x, max_y


def world_to_screen(x, y, camera_bbox):
    """
    Convert geographic coordinates in degrees to screen pixels.

    X = longitude.
    Y = latitude.

    Screen Y is inverted.
    """

    cam_min_x, cam_min_y, cam_max_x, cam_max_y = camera_bbox

    width = cam_max_x - cam_min_x
    height = cam_max_y - cam_min_y

    if width <= 0 or height <= 0:
        fail("Invalid camera BBox")

    screen_x = int(
        (x - cam_min_x)
        / width
        * WIDTH
    )

    screen_y = HEIGHT - int(
        (y - cam_min_y)
        / height
        * HEIGHT
    )

    return screen_x, screen_y


def is_in_screen(
    xmin,
    ymin,
    xmax,
    ymax,
    camera_bbox,
):
    """Test geographic AABB intersection with camera BBox."""

    cam_min_x, cam_min_y, cam_max_x, cam_max_y = camera_bbox

    if xmax < cam_min_x or xmin > cam_max_x:
        return False

    if ymax < cam_min_y or ymin > cam_max_y:
        return False

    return True


# ============================================================
# PGO global header
# ============================================================

def expected_file_type(path):
    """
    Determine expected PGO file type from the file extension.

    Only .idx, .mlp and .db are supported.
    """

    extension = os.path.splitext(path)[1].lower()

    if extension == ".idx":
        return PGO_FILE_TYPE_IDX

    if extension == ".mlp":
        return PGO_FILE_TYPE_MLP

    if extension == ".db":
        return PGO_FILE_TYPE_DB

    fail(
        f"{path}: unsupported map file extension "
        f"{extension!r}"
    )


def file_type_name(file_type):
    """Return human-readable PGO file type name."""

    names = {
        PGO_FILE_TYPE_IDX: ".idx",
        PGO_FILE_TYPE_MLP: ".mlp",
        PGO_FILE_TYPE_DB: ".db",
    }

    return names.get(
        file_type,
        f"unknown({file_type})"
    )


def read_pgo_header(file, path):
    """
    Read and strictly validate the V3 32-byte PGO header.

    Layout:

        0x00  3 bytes   Magic = "PGO"
        0x03  1 byte    File Type
        0x04  4 bytes   Payload Size, LE
        0x08  4 bytes   LOD 0 Offset, LE
        0x0C  4 bytes   LOD 1 Offset, LE
        0x10  4 bytes   LOD 2 Offset, LE
        0x14  4 bytes   Reserved, LE
        0x18  4 bytes   Reserved, LE
        0x1C  4 bytes   Reserved, LE

    There is no version field and no MD5 checksum.
    """

    header = read_exact(
        file,
        PGO_HEADER_SIZE,
        "PGO V3 header"
    )

    magic = header[0:3]

    if magic != b"PGO":
        fail(
            f"{path}: invalid PGO magic: {magic!r}"
        )

    file_type = header[3]

    if file_type not in (
        PGO_FILE_TYPE_IDX,
        PGO_FILE_TYPE_MLP,
        PGO_FILE_TYPE_DB,
    ):
        fail(
            f"{path}: unsupported PGO file type: "
            f"{file_type}"
        )

    expected_type = expected_file_type(path)

    if file_type != expected_type:
        fail(
            f"{path}: PGO file type mismatch: "
            f"header={file_type} "
            f"({file_type_name(file_type)}), "
            f"extension expects "
            f"{expected_type} "
            f"({file_type_name(expected_type)})"
        )

    (
        payload_size,
        lod0_offset,
        lod1_offset,
        lod2_offset,
        reserved1,
        reserved2,
        reserved3,
    ) = struct.unpack(
        "<IIIIIII",
        header[4:32]
    )

    actual_size = file_size(file)

    if actual_size < PGO_HEADER_SIZE:
        fail(
            f"{path}: file is smaller than "
            f"{PGO_HEADER_SIZE}-byte PGO header"
        )

    actual_payload_size = (
        actual_size
        - PGO_HEADER_SIZE
    )

    if payload_size != actual_payload_size:
        fail(
            f"{path}: payload size mismatch: "
            f"header={payload_size}, "
            f"actual={actual_payload_size}"
        )

    # The current compiler must write all reserved fields as zero.

    if (
        reserved1 != 0
        or reserved2 != 0
        or reserved3 != 0
    ):
        fail(
            f"{path}: reserved PGO header fields "
            f"must be zero: "
            f"{reserved1:#010x}, "
            f"{reserved2:#010x}, "
            f"{reserved3:#010x}"
        )

    offsets = (
        lod0_offset,
        lod1_offset,
        lod2_offset,
    )

    if file_type == PGO_FILE_TYPE_IDX:

        for lod_index, offset in enumerate(offsets):

            if offset < PGO_HEADER_SIZE:
                fail(
                    f"{path}: LOD {lod_index} offset "
                    f"{offset} is before payload"
                )

            if offset >= actual_size:
                fail(
                    f"{path}: LOD {lod_index} offset "
                    f"{offset} is outside file "
                    f"(size={actual_size})"
                )

        if not (
            lod0_offset
            < lod1_offset
            < lod2_offset
        ):
            fail(
                f"{path}: LOD offsets are not strictly "
                f"increasing: "
                f"{lod0_offset}, "
                f"{lod1_offset}, "
                f"{lod2_offset}"
            )

        # Every LOD offset must point to the SQT signature.

        for lod_index, offset in enumerate(offsets):

            current_position = file.tell()

            file.seek(offset)

            magic = read_exact(
                file,
                4,
                f"LOD {lod_index} SQT magic"
            )

            if magic != SQT_MAGIC:
                fail(
                    f"{path}: LOD {lod_index} offset "
                    f"{offset} does not point to SQT: "
                    f"{magic!r}"
                )

            file.seek(current_position)

    else:

        if any(offset != 0 for offset in offsets):
            fail(
                f"{path}: LOD offsets for "
                f"{file_type_name(file_type)} must be zero: "
                f"{offsets}"
            )

    print(
        f"[INFO] PGO V3 header: "
        f"type={file_type} "
        f"({file_type_name(file_type)}), "
        f"payload={payload_size}"
    )

    if file_type == PGO_FILE_TYPE_IDX:

        print(
            f"[INFO] LOD offsets: "
            f"LOD0={lod0_offset}, "
            f"LOD1={lod1_offset}, "
            f"LOD2={lod2_offset}"
        )

    else:

        print(
            "[INFO] LOD offsets: "
            "0, 0, 0 (reserved)"
        )

    return {
        "file_type": file_type,
        "payload_size": payload_size,
        "lod_offsets": offsets,
        "reserved": (
            reserved1,
            reserved2,
            reserved3,
        ),
        "file_size": actual_size,
    }


# ============================================================
# MLP geometry
# ============================================================

def parse_geometry_mlp(
    mlp_file,
    v1_offset,
    camera_bbox,
    screen_surface,
):
    """
    Read one MLP geometry record.

    Data Node v1 is a payload-relative offset to the geometry
    body.

    The compiler writes:

        v1 = payload_offset_of_geometry_body

    The local 8-byte record header immediately precedes the body.

    Therefore:

        absolute body offset =
            PGO_HEADER_SIZE + v1

        absolute record-header offset =
            PGO_HEADER_SIZE + v1 - 8
    """

    if mlp_file is None:
        return

    mlp_size = file_size(mlp_file)

    body_offset = (
        PGO_HEADER_SIZE
        + v1_offset
    )

    record_header_offset = (
        body_offset
        - MLP_RECORD_HEADER_SIZE
    )

    if record_header_offset < PGO_HEADER_SIZE:
        fail(
            f"Invalid MLP v1 offset: {v1_offset}"
        )

    if (
        body_offset < PGO_HEADER_SIZE
        or body_offset >= mlp_size
    ):
        fail(
            f"MLP v1 offset {v1_offset} "
            f"points outside MLP file"
        )

    mlp_file.seek(record_header_offset)

    local_header = read_exact(
        mlp_file,
        MLP_RECORD_HEADER_SIZE,
        "MLP record header"
    )

    sequence_number = struct.unpack(
        ">I",
        local_header[0:4]
    )[0]

    content_length = struct.unpack(
        "<I",
        local_header[4:8]
    )[0]

    if sequence_number == 0:
        fail(
            f"Invalid MLP sequence number 0 "
            f"at offset {record_header_offset}"
        )

    if content_length < MLP_BODY_HEADER_SIZE:
        fail(
            f"Invalid MLP content length "
            f"{content_length}"
        )

    body_end = (
        body_offset
        + content_length
    )

    if body_end > mlp_size:
        fail(
            f"MLP record #{sequence_number}: "
            f"record extends beyond file: "
            f"end={body_end}, size={mlp_size}"
        )

    body_data = read_exact(
        mlp_file,
        content_length,
        f"MLP body #{sequence_number}"
    )

    (
        minx,
        miny,
        maxx,
        maxy,
        num_parts,
        num_points,
    ) = struct.unpack(
        "<iiiiII",
        body_data[:MLP_BODY_HEADER_SIZE]
    )

    if minx > maxx or miny > maxy:
        fail(
            f"MLP record #{sequence_number}: "
            f"invalid BBox"
        )

    if num_parts == 0:
        fail(
            f"MLP record #{sequence_number}: "
            f"num_parts == 0"
        )

    if num_parts > MAX_NUM_PARTS:
        fail(
            f"MLP record #{sequence_number}: "
            f"num_parts={num_parts} exceeds "
            f"limit {MAX_NUM_PARTS}"
        )

    if num_points == 0:
        fail(
            f"MLP record #{sequence_number}: "
            f"num_points == 0"
        )

    if num_points > MAX_NUM_POINTS:
        fail(
            f"MLP record #{sequence_number}: "
            f"num_points={num_points} exceeds "
            f"limit {MAX_NUM_POINTS}"
        )

    expected_size = (
        MLP_BODY_HEADER_SIZE
        + num_parts * 4
        + num_points * 8
    )

    if expected_size != content_length:
        fail(
            f"MLP record #{sequence_number}: "
            f"content length mismatch: "
            f"declared={content_length}, "
            f"expected={expected_size}"
        )

    parts_offset = MLP_BODY_HEADER_SIZE

    points_offset = (
        parts_offset
        + num_parts * 4
    )

    parts = struct.unpack_from(
        f"<{num_parts}I",
        body_data,
        parts_offset
    )

    previous_part = -1

    for part_index in parts:

        if part_index >= num_points:
            fail(
                f"MLP record #{sequence_number}: "
                f"part index {part_index} "
                f"is outside num_points={num_points}"
            )

        if part_index <= previous_part:
            fail(
                f"MLP record #{sequence_number}: "
                f"parts[] is not strictly increasing"
            )

        previous_part = part_index

    raw_points = struct.unpack_from(
        f"<{num_points * 2}i",
        body_data,
        points_offset
    )

    # --------------------------------------------------------
    # Validate BBox against actual points.
    # --------------------------------------------------------

    actual_min_x = raw_points[0]
    actual_min_y = raw_points[1]
    actual_max_x = raw_points[0]
    actual_max_y = raw_points[1]

    for i in range(num_points):

        x = raw_points[i * 2]
        y = raw_points[i * 2 + 1]

        actual_min_x = min(
            actual_min_x,
            x
        )

        actual_min_y = min(
            actual_min_y,
            y
        )

        actual_max_x = max(
            actual_max_x,
            x
        )

        actual_max_y = max(
            actual_max_y,
            y
        )

    if (
        minx != actual_min_x
        or miny != actual_min_y
        or maxx != actual_max_x
        or maxy != actual_max_y
    ):
        print(
            f"[WARN] MLP record #{sequence_number}: "
            f"BBox does not match point data"
        )

    screen_points = []

    for i in range(num_points):

        lon = (
            raw_points[i * 2]
            / COORD_SCALE
        )

        lat = (
            raw_points[i * 2 + 1]
            / COORD_SCALE
        )

        screen_points.append(
            world_to_screen(
                lon,
                lat,
                camera_bbox
            )
        )

    if screen_surface is None:
        # Validation only (V4 non-displayed LODs).
        stats["geometry_checked"] += 1
        return (minx, miny, maxx, maxy)

    stats["geometry_drawn"] += 1

    # --------------------------------------------------------
    # Render each part independently.
    #
    # parts[] contains start indices.
    # --------------------------------------------------------

    for part_index in range(num_parts):

        start = parts[part_index]

        if part_index + 1 < num_parts:
            end = parts[part_index + 1]
        else:
            end = num_points

        if end <= start:
            fail(
                f"MLP record #{sequence_number}: "
                f"empty or invalid part range"
            )

        contour = screen_points[start:end]

        if len(contour) < 2:
            continue

        if (
            len(contour) >= 3
            and contour[0] == contour[-1]
        ):

            pygame.draw.polygon(
                screen_surface,
                (80, 140, 80),
                contour,
                1
            )

            stats["polygons_drawn"] += 1

        else:

            pygame.draw.lines(
                screen_surface,
                (220, 220, 220),
                False,
                contour,
                2
            )

            stats["lines_drawn"] += 1

    return (minx, miny, maxx, maxy)


# ============================================================
# IDX Data Node
# ============================================================

def parse_data_node(
    node_data,
    mlp_file,
    camera_bbox,
    screen_surface,
    strict_bbox=False,
):
    """
    Parse one V3/V4 Data Node.

    screen_surface=None validates without drawing; strict_bbox=True (V4)
    requires the node BBox to be equal to the BBox of its geometry record.

    Exact size: 25 bytes.

        <iiiiBII>

        0x00  int32   xmin
        0x04  int32   ymin
        0x08  int32   xmax
        0x0C  int32   ymax
        0x10  uint8   feature code
        0x11  uint32  v1
        0x15  uint32  v2
    """

    if len(node_data) != DATA_NODE_SIZE:
        fail(
            f"Invalid Data Node size: "
            f"{len(node_data)}"
        )

    (
        xmin,
        ymin,
        xmax,
        ymax,
        code,
        v1,
        v2,
    ) = struct.unpack(
        "<iiiiBII",
        node_data
    )

    if xmin > xmax or ymin > ymax:
        fail(
            f"Data Node code={code}: "
            f"invalid BBox"
        )

    stats["data_visited"] += 1

    xmin_f = xmin / COORD_SCALE
    ymin_f = ymin / COORD_SCALE
    xmax_f = xmax / COORD_SCALE
    ymax_f = ymax / COORD_SCALE

    if not is_in_screen(
        xmin_f,
        ymin_f,
        xmax_f,
        ymax_f,
        camera_bbox,
    ):
        return

    stats["data_drawn"] += 1

    # --------------------------------------------------------
    # Native POI
    #
    # A native POI is represented by a point BBox.
    # v1 is unused.
    # --------------------------------------------------------

    if xmin == xmax and ymin == ymax:

        if screen_surface is None:
            return

        stats["pois_drawn"] += 1

        sx, sy = world_to_screen(
            xmin_f,
            ymin_f,
            camera_bbox
        )

        pygame.draw.circle(
            screen_surface,
            (255, 80, 80),
            (sx, sy),
            4
        )

        return

    # --------------------------------------------------------
    # Ordinary geometry feature.
    # --------------------------------------------------------

    if v1 == 0:
        fail(
            f"Data Node code={code}: "
            f"non-POI feature has v1=0"
        )

    record_bbox = parse_geometry_mlp(
        mlp_file,
        v1,
        camera_bbox,
        screen_surface,
    )

    if (
        strict_bbox
        and record_bbox is not None
        and record_bbox != (xmin, ymin, xmax, ymax)
    ):
        fail(
            f"Data Node code={code}: BBox "
            f"{(xmin, ymin, xmax, ymax)} differs from the BBox "
            f"of MLP record v1={v1}: {record_bbox}"
        )


# ============================================================
# IDX Navigation Node
# ============================================================

def parse_nav_node(
    idx_file,
    mlp_file,
    node_data,
    camera_bbox,
    screen_surface,
    section_end,
):
    """
    Parse one V3 Navigation Node.

    Exact size: 28 bytes.

        <IiiiiII>

        0x00  uint32  v3_jump
        0x04  int32   xmin
        0x08  int32   ymin
        0x0C  int32   xmax
        0x10  int32   ymax
        0x14  uint32  level
        0x18  uint32  child_count

    v3_jump is the exact byte size of the complete child
    subtree.

    It is measured from the position immediately after this
    28-byte Navigation Node.
    """

    if len(node_data) != NAV_NODE_SIZE:
        fail(
            f"Invalid Navigation Node size: "
            f"{len(node_data)}"
        )

    (
        v3_jump,
        xmin,
        ymin,
        xmax,
        ymax,
        level,
        child_count,
    ) = struct.unpack(
        "<IiiiiII",
        node_data
    )

    stats["nav_visited"] += 1

    subtree_start = idx_file.tell()
    subtree_end = (
        subtree_start
        + v3_jump
    )

    if subtree_end > section_end:
        fail(
            f"Navigation Node subtree exceeds "
            f"LOD section: "
            f"start={subtree_start}, "
            f"jump={v3_jump}, "
            f"section_end={section_end}"
        )

    if xmin > xmax or ymin > ymax:
        fail(
            "Navigation Node contains invalid BBox"
        )

    # --------------------------------------------------------
    # Structural validation.
    #
    # Level 0 children are Data Nodes.
    # Their physical size is fixed at 25 bytes.
    # --------------------------------------------------------

    if level == 0:

        expected_jump = (
            child_count
            * DATA_NODE_SIZE
        )

        if v3_jump != expected_jump:
            fail(
                f"Invalid v3_jump for level-0 "
                f"Navigation Node: "
                f"jump={v3_jump}, "
                f"expected={expected_jump}, "
                f"children={child_count}"
            )

    elif child_count == 0:

        if v3_jump != 0:
            fail(
                f"Invalid empty Navigation Node: "
                f"level={level}, "
                f"children=0, "
                f"v3_jump={v3_jump}"
            )

    xmin_f = xmin / COORD_SCALE
    ymin_f = ymin / COORD_SCALE
    xmax_f = xmax / COORD_SCALE
    ymax_f = ymax / COORD_SCALE

    # --------------------------------------------------------
    # BBox culling.
    #
    # We can skip the complete child subtree because v3_jump
    # is its exact physical byte size.
    # --------------------------------------------------------

    if not is_in_screen(
        xmin_f,
        ymin_f,
        xmax_f,
        ymax_f,
        camera_bbox,
    ):

        stats["nav_culled"] += 1

        if v3_jump == 0:
            return

        current_position = idx_file.tell()

        idx_file.seek(
            v3_jump,
            os.SEEK_CUR
        )

        if idx_file.tell() != subtree_end:
            fail(
                "Internal parser error while "
                "skipping Navigation Node subtree"
            )

        stats["bytes_skipped"] += (
            idx_file.tell()
            - current_position
        )

        return

    # --------------------------------------------------------
    # Visible subtree.
    # --------------------------------------------------------

    child_is_nav = level > 0

    for _ in range(child_count):

        if idx_file.tell() >= section_end:
            fail(
                "Navigation Node children exceed "
                "LOD section boundary"
            )

        parse_node(
            idx_file,
            mlp_file,
            child_is_nav,
            camera_bbox,
            screen_surface,
            section_end,
        )

    # The parsed children must occupy exactly v3_jump bytes.

    if idx_file.tell() != subtree_end:
        fail(
            f"Navigation Node subtree size mismatch: "
            f"expected end={subtree_end}, "
            f"actual={idx_file.tell()}"
        )


# ============================================================
# Generic IDX node parser
# ============================================================

def parse_node(
    idx_file,
    mlp_file,
    is_nav_node,
    camera_bbox,
    screen_surface,
    section_end,
):
    """
    Read and parse exactly one IDX node.

    Data Node = 25 bytes.
    Navigation Node = 28 bytes.

    The caller determines the node type from the SQT tree depth.
    """

    if is_nav_node:

        node_data = read_exact(
            idx_file,
            NAV_NODE_SIZE,
            "Navigation Node"
        )

        parse_nav_node(
            idx_file,
            mlp_file,
            node_data,
            camera_bbox,
            screen_surface,
            section_end,
        )

    else:

        node_data = read_exact(
            idx_file,
            DATA_NODE_SIZE,
            "Data Node"
        )

        parse_data_node(
            node_data,
            mlp_file,
            camera_bbox,
            screen_surface,
        )


# ============================================================
# SQT section
# ============================================================

def parse_sqt_section(
    idx_file,
    mlp_file,
    camera_bbox,
    screen_surface,
    section_index,
    section_end,
):
    """
    Parse one SQT section.

    Layout:

        0x00  4 bytes  SQT magic
        0x04  4 bytes  topology marker
        0x08  4 bytes  tree depth
        0x0C  4 bytes  root count

    The SQT section is bounded by the next LOD offset or
    by the end of the IDX file.
    """

    section_start = idx_file.tell()

    if section_start + SQT_HEADER_SIZE > section_end:
        fail(
            f"SQT section #{section_index}: "
            f"section is smaller than "
            f"{SQT_HEADER_SIZE}-byte header"
        )

    header = read_exact(
        idx_file,
        SQT_HEADER_SIZE,
        f"SQT section #{section_index} header"
    )

    (
        magic,
        topology_marker,
        mode,
        root_count,
    ) = struct.unpack(
        "<4sIII",
        header
    )

    if magic != SQT_MAGIC:
        fail(
            f"SQT section #{section_index}: "
            f"invalid magic {magic!r}"
        )

    print(
        f"[INFO] SQT #{section_index}: "
        f"offset={section_start}, "
        f"topology=0x{topology_marker:08X}, "
        f"depth={mode}, "
        f"roots={root_count}"
    )

    stats["sqt_sections"] += 1

    if root_count == 0:

        if idx_file.tell() != section_end:
            fail(
                f"SQT section #{section_index}: "
                f"empty section has trailing data: "
                f"current={idx_file.tell()}, "
                f"end={section_end}"
            )

        return

    if mode == 0:
        root_is_nav = False
    else:
        root_is_nav = True

    for _ in range(root_count):

        if idx_file.tell() >= section_end:
            fail(
                f"SQT section #{section_index}: "
                f"root nodes exceed section boundary"
            )

        parse_node(
            idx_file,
            mlp_file,
            root_is_nav,
            camera_bbox,
            screen_surface,
            section_end,
        )

    # The SQT must consume exactly its declared section.

    if idx_file.tell() != section_end:
        fail(
            f"SQT section #{section_index}: "
            f"trailing or unparsed bytes: "
            f"current={idx_file.tell()}, "
            f"section_end={section_end}"
        )


# ============================================================
# IDX validation
# ============================================================

def validate_idx(
    idx_file,
    mlp_file,
    header,
    camera_bbox,
    screen_surface,
):
    """
    Validate all three IDX LOD sections.

    The global header contains absolute offsets to the beginning
    of each SQT section.

    LOD 0 ends at LOD 1 offset.
    LOD 1 ends at LOD 2 offset.
    LOD 2 ends at physical EOF.
    """

    lod_offsets = header["lod_offsets"]
    actual_size = header["file_size"]

    for lod_index in range(3):

        section_start = lod_offsets[lod_index]

        if lod_index < 2:
            section_end = lod_offsets[lod_index + 1]
        else:
            section_end = actual_size

        if section_end <= section_start:
            fail(
                f"LOD {lod_index}: invalid section range: "
                f"{section_start}..{section_end}"
            )

        idx_file.seek(section_start)

        parse_sqt_section(
            idx_file,
            mlp_file,
            camera_bbox,
            screen_surface,
            lod_index,
            section_end,
        )

        if idx_file.tell() != section_end:
            fail(
                f"LOD {lod_index}: parser ended at "
                f"{idx_file.tell()}, expected "
                f"{section_end}"
            )

    print(
        "[INFO] All three IDX LOD sections "
        "validated successfully."
    )


# ============================================================
# MLP validation
# ============================================================

def validate_mlp_header(
    mlp_file,
    header,
):
    """
    Validate the MLP global header.

    Structural MLP records are validated on demand through IDX
    Data Node v1 references.

    This function also confirms that the payload is non-empty
    only if the file actually contains data.
    """

    if header["file_type"] != PGO_FILE_TYPE_MLP:
        fail(
            "Internal error: validate_mlp_header() "
            "called for non-MLP file"
        )

    print(
        f"[INFO] MLP payload size: "
        f"{header['payload_size']} bytes"
    )


# ============================================================
# Generic DB validation
# ============================================================

def validate_db_header(
    db_file,
    header,
):
    """
    Validate the DB global header.

    The DB payload format is not parsed here because this viewer
    currently does not have a DB record consumer.

    The global V3 header is still validated strictly.
    """

    if header["file_type"] != PGO_FILE_TYPE_DB:
        fail(
            "Internal error: validate_db_header() "
            "called for non-DB file"
        )

    print(
        f"[INFO] DB payload size: "
        f"{header['payload_size']} bytes"
    )



# ============================================================
# V4: PGO header, block SQT sections (docs/map-format-v4.md)
# ============================================================

def detect_idx_v4(idx_file):
    """True if the .idx file is format V4 (SQT\\x02 at the LOD 0 offset)."""

    position = idx_file.tell()

    try:
        idx_file.seek(0)
        head = idx_file.read(PGO_HEADER_SIZE)

        if len(head) < PGO_HEADER_SIZE or head[0:3] != b"PGO":
            return False

        lod0 = struct.unpack_from("<I", head, 8)[0]

        idx_file.seek(lod0)
        return idx_file.read(4) == SQT_MAGIC_V4

    finally:
        idx_file.seek(position)


def read_pgo_header_v4(file, path):
    """Read and strictly validate the V4 .idx PGO header (four LOD offsets)."""

    header = read_exact(file, PGO_HEADER_SIZE, "PGO V4 header")

    if header[0:3] != b"PGO":
        fail(f"{path}: invalid PGO magic: {header[0:3]!r}")

    file_type = header[3]

    if file_type != PGO_FILE_TYPE_IDX:
        fail(f"{path}: PGO file type {file_type}, expected 1 (IDX)")

    (
        payload_size,
        lod0,
        lod1,
        lod2,
        lod3,
        reserved1,
        reserved2,
    ) = struct.unpack("<IIIIIII", header[4:32])

    actual_size = file_size(file)

    if payload_size != actual_size - PGO_HEADER_SIZE:
        fail(
            f"{path}: payload size mismatch: header={payload_size}, "
            f"actual={actual_size - PGO_HEADER_SIZE}"
        )

    if reserved1 != 0 or reserved2 != 0:
        fail(f"{path}: reserved PGO header fields must be zero")

    offsets = (lod0, lod1, lod2, lod3)

    if lod0 != V4_FIRST_SECTION:
        fail(f"{path}: LOD 0 offset {lod0}, expected {V4_FIRST_SECTION}")

    if not lod0 < lod1 < lod2 < lod3:
        fail(f"{path}: LOD offsets are not strictly increasing: {offsets}")

    if actual_size % V4_BLOCK_SIZE != 0:
        fail(f"{path}: file size {actual_size} is not a multiple of 512")

    for lod_index, offset in enumerate(offsets):

        if offset % V4_BLOCK_SIZE != 0 or offset >= actual_size:
            fail(f"{path}: invalid LOD {lod_index} offset {offset}")

        file.seek(offset)

        if read_exact(file, 4, f"LOD {lod_index} SQT magic") != SQT_MAGIC_V4:
            fail(f"{path}: LOD {lod_index} offset {offset} does not point to SQT\\x02")

    file.seek(PGO_HEADER_SIZE)
    padding = read_exact(
        file,
        V4_FIRST_SECTION - PGO_HEADER_SIZE,
        "padding before LOD 0",
    )

    if any(padding):
        fail(f"{path}: bytes 0x20..0x1FF must be zero")

    print(
        f"[INFO] PGO V4 header: type=1 (IDX), payload={payload_size}"
    )
    print(
        "[INFO] LOD offsets: "
        + ", ".join(f"LOD{i}={o}" for i, o in enumerate(offsets))
    )

    return {
        "file_type": file_type,
        "payload_size": payload_size,
        "lod_offsets": offsets,
        "reserved": (reserved1, reserved2),
        "file_size": actual_size,
    }


def _section_range_v4(header, lod_index):
    offsets = header["lod_offsets"]
    start = offsets[lod_index]

    if lod_index + 1 < V4_LOD_COUNT:
        end = offsets[lod_index + 1]
    else:
        end = header["file_size"]

    return start, end


def _bbox_inside(inner, outer):
    return (
        outer[0] <= inner[0]
        and outer[1] <= inner[1]
        and inner[2] <= outer[2]
        and inner[3] <= outer[3]
    )


def validate_sqt_v4(
    idx_file,
    mlp_file,
    header,
    lod_index,
    camera_bbox,
):
    """
    Validate one V4 LOD section.

    Blocks are stored level by level (breadth first), so the section is
    read sequentially. Every block is checked; geometry records of data
    nodes that fall into the camera are read and checked as well.
    """

    start, end = _section_range_v4(header, lod_index)
    name = f"LOD {lod_index}"

    idx_file.seek(start)
    block = read_exact(idx_file, V4_BLOCK_SIZE, f"{name} SQT header")

    (
        magic,
        topology,
        depth,
        root_count,
        block_size,
        section_size,
        nav_fanout,
        leaf_fanout,
    ) = struct.unpack_from("<4sIIIIIHH", block)

    if magic != SQT_MAGIC_V4:
        fail(f"{name}: invalid SQT magic {magic!r}")

    if topology != 2 or block_size != V4_BLOCK_SIZE:
        fail(f"{name}: topology={topology}, block_size={block_size}")

    if (nav_fanout, leaf_fanout) != (V4_NAV_FANOUT, V4_LEAF_FANOUT):
        fail(f"{name}: fan-out {nav_fanout}/{leaf_fanout}, expected 18/20")

    if section_size != end - start:
        fail(
            f"{name}: section_size={section_size}, "
            f"expected {end - start}"
        )

    if any(block[28:]):
        fail(f"{name}: SQT header reserved bytes must be zero")

    stats["sqt_sections"] += 1

    print(
        f"[INFO] SQT #{lod_index}: offset={start}, depth={depth}, "
        f"roots={root_count}, size={section_size}"
    )

    if depth == 0:

        if root_count != 0 or section_size != V4_BLOCK_SIZE:
            fail(f"{name}: empty section must have no roots and no blocks")

        return

    if not 1 <= root_count <= V4_NAV_FANOUT:
        fail(f"{name}: root_count={root_count} outside 1..18")

    # expected[i] = (level, entries, parent_bbox); level -1 = data block
    expected = [(depth - 1, root_count, None)]
    index = 0

    while index < len(expected):

        level, count, parent_bbox = expected[index]
        offset = start + V4_BLOCK_SIZE * (1 + index)

        if offset + V4_BLOCK_SIZE > end:
            fail(f"{name}: block #{index} is outside the section")

        block = read_exact(idx_file, V4_BLOCK_SIZE, f"{name} block #{index}")

        if level == -1:

            used = count * DATA_NODE_SIZE

            for k in range(count):
                node = block[k * DATA_NODE_SIZE:(k + 1) * DATA_NODE_SIZE]
                bbox = struct.unpack_from("<iiii", node)

                if parent_bbox is not None and not _bbox_inside(bbox, parent_bbox):
                    fail(f"{name}: data node in block #{index} is outside its parent BBox")

                parse_data_node(
                    node,
                    mlp_file,
                    camera_bbox,
                    None,
                    strict_bbox=True,
                )

        else:

            used = count * NAV_NODE_SIZE

            for k in range(count):
                (
                    child,
                    xmin,
                    ymin,
                    xmax,
                    ymax,
                    node_level,
                    child_count,
                ) = struct.unpack_from("<IiiiiII", block, k * NAV_NODE_SIZE)

                bbox = (xmin, ymin, xmax, ymax)

                if xmin > xmax or ymin > ymax:
                    fail(f"{name}: navigation node with invalid BBox")

                if parent_bbox is not None and not _bbox_inside(bbox, parent_bbox):
                    fail(f"{name}: navigation node in block #{index} is outside its parent BBox")

                if node_level != level:
                    fail(
                        f"{name}: block #{index} node level {node_level}, "
                        f"expected {level}"
                    )

                expected_child = start + V4_BLOCK_SIZE * (1 + len(expected))

                if child != expected_child:
                    fail(
                        f"{name}: block #{index} child_block={child}, "
                        f"expected {expected_child}"
                    )

                limit = V4_LEAF_FANOUT if level == 0 else V4_NAV_FANOUT

                if not 1 <= child_count <= limit:
                    fail(f"{name}: child count {child_count} outside 1..{limit}")

                expected.append((level - 1, child_count, bbox))
                stats["nav_visited"] += 1

        if any(block[used:]):
            fail(f"{name}: block #{index} has non-zero bytes after its entries")

        index += 1

    if V4_BLOCK_SIZE * (1 + len(expected)) != section_size:
        fail(
            f"{name}: {len(expected)} blocks do not fill "
            f"section_size={section_size}"
        )


def validate_idx_v4(idx_file, mlp_file, header, camera_bbox):
    """Validate all four V4 LOD sections."""

    for lod_index in range(V4_LOD_COUNT):
        validate_sqt_v4(idx_file, mlp_file, header, lod_index, camera_bbox)

    print("[INFO] All four V4 IDX LOD sections validated successfully.")


def draw_sqt_v4(idx_file, mlp_file, header, lod_index, camera_bbox, surface):
    """Draw one V4 LOD; navigation nodes outside the camera are skipped."""

    start, end = _section_range_v4(header, lod_index)

    idx_file.seek(start)
    sqt = read_exact(idx_file, V4_BLOCK_SIZE, "SQT header")
    _, _, depth, root_count = struct.unpack_from("<4sIII", sqt)

    if depth == 0:
        return

    def visit(offset, level, count):

        idx_file.seek(offset)
        block = read_exact(idx_file, V4_BLOCK_SIZE, "block")

        if level == -1:

            for k in range(count):
                parse_data_node(
                    block[k * DATA_NODE_SIZE:(k + 1) * DATA_NODE_SIZE],
                    mlp_file,
                    camera_bbox,
                    surface,
                )

            return

        for k in range(count):
            (
                child,
                xmin,
                ymin,
                xmax,
                ymax,
                node_level,
                child_count,
            ) = struct.unpack_from("<IiiiiII", block, k * NAV_NODE_SIZE)

            stats["nav_visited"] += 1

            if not is_in_screen(
                xmin / COORD_SCALE,
                ymin / COORD_SCALE,
                xmax / COORD_SCALE,
                ymax / COORD_SCALE,
                camera_bbox,
            ):
                stats["nav_culled"] += 1
                continue

            visit(child, node_level - 1, child_count)

    visit(start + V4_BLOCK_SIZE, depth - 1, root_count)


# ============================================================
# V4: search.idx validation (section 21)
# ============================================================

def _is_key_byte(b):
    """Byte of a normalized (folded) search word."""

    return (
        0x30 <= b <= 0x39
        or 0x61 <= b <= 0x7A
        or 0xA0 <= b <= 0xBF
        or 0xD0 <= b <= 0xDF
        or 0xE0 <= b <= 0xEF
    )


def _morton(lon, lat):
    def spread(v):
        v &= 0xFFFFFFFF
        v = (v | (v << 16)) & 0x0000FFFF0000FFFF
        v = (v | (v << 8)) & 0x00FF00FF00FF00FF
        v = (v | (v << 4)) & 0x0F0F0F0F0F0F0F0F
        v = (v | (v << 2)) & 0x3333333333333333
        v = (v | (v << 1)) & 0x5555555555555555
        return v

    return spread(lon + 1_800_000_000) | (spread(lat + 900_000_000) << 1)


def _align512(n):
    return (n + 511) // 512 * 512


def validate_search_idx(path, db_path=None):
    """Strict validation of search.idx (file type 4). Returns True/False."""

    print()
    print("============================================")
    print(" PurrGO V4 — search.idx VALIDATOR")
    print("============================================")

    try:
        with open(path, "rb") as f:
            data = f.read()

        if len(data) < 0x60 or data[0:3] != b"PGO" or data[3] != PGO_FILE_TYPE_SEARCH:
            fail(f"{path}: not a PGO file of type 4 (search)")

        payload = struct.unpack_from("<I", data, 4)[0]

        if payload != len(data) - PGO_HEADER_SIZE:
            fail(f"{path}: payload size mismatch")

        if any(data[8:32]):
            fail(f"{path}: LOD offsets and reserved fields must be zero")

        if len(data) % 512 != 0:
            fail(f"{path}: size {len(data)} is not a multiple of 512")

        (
            magic, version, record_size, flags, db_records,
            name_off, name_cnt, cat_off, cat_cnt,
            dict_off, dict_size, range_off, range_cnt,
        ) = struct.unpack_from("<4sHHIIIIIIIIII", data, 0x20)

        if magic != b"SRCH" or version != 1 or record_size != 32 or flags != 0:
            fail("SRCH header: bad magic/version/record size/flags")

        if any(data[0x20 + 48:0x60]):
            fail("SRCH header: reserved bytes must be zero")

        if dict_off != 0x60 or range_off != 0 or range_cnt != 0:
            fail("SRCH header: dict_offset must be 0x60, range section absent")

        if dict_off + dict_size > len(data):
            fail("dictionary is outside the file")

        word_count, entry_size, dict_reserved = struct.unpack_from("<HHI", data, dict_off)

        if entry_size != 48 or dict_reserved != 0 or dict_size != 8 + 48 * word_count:
            fail("dictionary header is inconsistent")

        previous = None

        for i in range(word_count):
            e = dict_off + 8 + i * 48
            key = data[e:e + 16]
            mask = int.from_bytes(data[e + 16:e + 48], "little")
            length = len(key.rstrip(b"\0"))

            if length == 0 or not all(_is_key_byte(b) for b in key[:length]):
                fail(f"dictionary word #{i} is not a normalized key")

            if previous is not None and key <= previous:
                fail(f"dictionary word #{i} is not sorted/unique")

            if mask & 1 or mask == 0:
                fail(f"dictionary word #{i}: bad subtype mask")

            previous = key

        if name_off != _align512(dict_off + dict_size):
            fail(f"name_offset {name_off} is not the aligned end of the dictionary")

        if cat_off != name_off + _align512(32 * name_cnt):
            fail("cat_offset does not follow the NAME section")

        if len(data) != cat_off + _align512(32 * cat_cnt):
            fail("file size does not match the section sizes")

        def entries(offset, count, what):
            out = []
            for i in range(count):
                lon, lat, db_idx, code, subtype, fl, klen, key = struct.unpack_from(
                    "<iiIBBBB16s", data, offset + i * 32
                )

                if not -1_800_000_000 <= lon <= 1_800_000_000 or not -900_000_000 <= lat <= 900_000_000:
                    fail(f"{what} #{i}: coordinates out of range")

                if not 1 <= db_idx <= db_records:
                    fail(f"{what} #{i}: db_idx {db_idx} outside 1..{db_records}")

                if klen > 16 or key[klen:] != bytes(16 - klen) or fl & ~7:
                    fail(f"{what} #{i}: bad key_len/key padding/flags")

                if not all(_is_key_byte(b) for b in key[:klen]):
                    fail(f"{what} #{i}: key is not normalized")

                out.append((lon, lat, db_idx, code, subtype, fl, klen, key))

            for pad in (data[offset + 32 * count:offset + _align512(32 * count)],):
                if any(pad):
                    fail(f"{what}: non-zero padding after the last entry")

            return out

        names = entries(name_off, name_cnt, "NAME")
        cats = entries(cat_off, cat_cnt, "CAT")

        order = [(e[7], e[2], e[5] & 1) for e in names]

        if order != sorted(order):
            fail("NAME section is not sorted by (key, db_idx, primary first)")

        primary = {}
        secondary = {}

        for e in names:
            table = secondary if e[5] & 1 else primary
            table[e[2]] = table.get(e[2], 0) + 1

            if e[6] == 0:
                fail("NAME entry with empty key")

        if any(v != 1 for v in primary.values()):
            fail("a POI has more than one primary NAME entry")

        if any(v > 3 or k not in primary for k, v in secondary.items()):
            fail("secondary NAME entries without primary or more than 3")

        cat_order = [(e[4], _morton(e[0], e[1]), e[2]) for e in cats]

        if cat_order != sorted(cat_order):
            fail("CAT section is not sorted by (subtype, morton, db_idx)")

        for e in cats:
            if e[4] == 0 or e[5] & 1:
                fail("CAT entry with subtype 0 or SECONDARY flag")

            if (e[5] & 2) != 0 and e[6] != 0:
                fail("UNNAMED CAT entry with a key")

        seen = {}

        for e in names + cats:
            ref = (e[0], e[1], e[3], e[4])
            if seen.setdefault(e[2], ref) != ref:
                fail(f"entries of db_idx {e[2]} disagree on position/code/subtype")

        if db_path is not None:
            with open(db_path, "rb") as f:
                db = f.read(0x28)

            if len(db) < 0x28 or db[0:3] != b"PGO" or db[3] != PGO_FILE_TYPE_DB:
                fail(f"{db_path}: not a PGO .db file")

            if struct.unpack_from("<I", db, 0x24)[0] != db_records:
                fail("db_records differs from the record count of the .db file")

        print(f"[INFO] dictionary words: {word_count}")
        print(f"[INFO] NAME entries:     {name_cnt} "
              f"({len(primary)} POIs, {sum(secondary.values())} secondary)")
        print(f"[INFO] CAT entries:      {cat_cnt}")
        print(f"[INFO] db_records:       {db_records}")
        print("[OK] search.idx validation successful.")
        return True

    except MapFormatError as error:
        print(f"[FORMAT ERROR] {error}")
        return False

    except (OSError, struct.error) as error:
        print(f"[ERROR] Parser failure: {error}")
        return False


# ============================================================
# Full map renderer / validator
# ============================================================

def render_map(
    idx_path,
    mlp_path,
    map_name_path,
    db_path=None,
    lod=0,
    size_km=5.0,
    screenshot=None,
):
    """
    Validate and render a PurrGO V3 or V4 map layer.

    V4: all four LODs are validated; `lod` selects the displayed one
    (keys 0..3 switch it). `screenshot` saves a PNG and exits.

    Required:

        .idx
        .mlp  (mlp_path None: layer without geometry, e.g. pois.idx)

    Optional:

        .db

    The DB is currently header-validated only.
    """

    global stats

    stats = create_stats()

    print()
    print("============================================")
    print(" PurrGO MAP FORMAT V3 / V4 — PC MAP VALIDATOR")
    print("============================================")

    camera_bbox = load_camera_bbox(
        map_name_path,
        size_km=size_km,
    )

    pygame.init()

    screen = pygame.display.set_mode(
        (WIDTH, HEIGHT)
    )

    pygame.display.set_caption(
        "PurrGO Map Viewer"
    )

    screen.fill(
        (20, 20, 20)
    )

    if not os.path.exists(idx_path):

        print(
            f"[ERROR] IDX file not found: {idx_path}"
        )

        pygame.quit()
        return False

    if mlp_path is not None and not os.path.exists(mlp_path):

        print(
            f"[ERROR] MLP file not found: {mlp_path}"
        )

        pygame.quit()
        return False

    if db_path is not None and not os.path.exists(db_path):

        print(
            f"[ERROR] DB file not found: {db_path}"
        )

        pygame.quit()
        return False

    try:

        mlp_context = (
            open(mlp_path, "rb")
            if mlp_path is not None
            else contextlib.nullcontext(None)
        )

        with (
            open(idx_path, "rb") as idx_file,
            mlp_context as mlp_file,
        ):

            print(
                f"[INFO] Opening IDX: {idx_path}"
            )

            is_v4 = detect_idx_v4(idx_file)

            if is_v4:
                idx_header = read_pgo_header_v4(
                    idx_file,
                    idx_path,
                )
            else:
                idx_header = read_pgo_header(
                    idx_file,
                    idx_path,
                )

            if idx_header["file_type"] != PGO_FILE_TYPE_IDX:
                fail(
                    "IDX file does not contain "
                    "PGO File Type 1"
                )

            if mlp_file is not None:

                print(
                    f"[INFO] Opening MLP: {mlp_path}"
                )

                mlp_header = read_pgo_header(
                    mlp_file,
                    mlp_path,
                )

                validate_mlp_header(
                    mlp_file,
                    mlp_header,
                )

            # ------------------------------------------------
            # Optional DB.
            # ------------------------------------------------

            if db_path is not None:

                print(
                    f"[INFO] Opening DB: {db_path}"
                )

                with open(
                    db_path,
                    "rb",
                ) as db_file:

                    db_header = read_pgo_header(
                        db_file,
                        db_path,
                    )

                    validate_db_header(
                        db_file,
                        db_header,
                    )

            # ------------------------------------------------
            # Validate all IDX LOD sections.
            # ------------------------------------------------

            if is_v4:
                validate_idx_v4(
                    idx_file,
                    mlp_file,
                    idx_header,
                    camera_bbox,
                )
            else:
                validate_idx(
                    idx_file,
                    mlp_file,
                    idx_header,
                    camera_bbox,
                    screen,
                )

        version = 4 if is_v4 else 3

        print()
        print("============================================")
        print(f" V{version} MAP VALIDATION / RENDER STATISTICS")
        print("============================================")

        print(
            f" SQT sections:        "
            f"{stats['sqt_sections']}"
        )

        print(
            f" Nav visited:         "
            f"{stats['nav_visited']}"
        )

        print(
            f" Nav culled:          "
            f"{stats['nav_culled']}"
        )

        print(
            f" Data visited:        "
            f"{stats['data_visited']}"
        )

        print(
            f" Data visible:        "
            f"{stats['data_drawn']}"
        )

        print(
            f" Geometry records:    "
            f"{stats['geometry_drawn']}"
        )

        print(
            f" Lines drawn:         "
            f"{stats['lines_drawn']}"
        )

        print(
            f" Polygons drawn:      "
            f"{stats['polygons_drawn']}"
        )

        print(
            f" POIs drawn:          "
            f"{stats['pois_drawn']}"
        )

        print(
            f" Bytes skipped:       "
            f"{stats['bytes_skipped']}"
        )

        if is_v4:
            print(
                f" Geometry validated:  "
                f"{stats['geometry_checked']}"
            )

        print("============================================")
        print(f"[OK] PurrGO V{version} map validation successful.")

        def show_v4_lod(lod_index):
            """Draw one V4 LOD (files are reopened: validation closed them)."""

            global stats

            stats = create_stats()
            screen.fill((20, 20, 20))

            with (
                open(idx_path, "rb") as idx_file,
                (
                    open(mlp_path, "rb")
                    if mlp_path is not None
                    else contextlib.nullcontext(None)
                ) as mlp_file,
            ):
                draw_sqt_v4(
                    idx_file,
                    mlp_file,
                    idx_header,
                    lod_index,
                    camera_bbox,
                    screen,
                )

            pygame.display.set_caption(
                f"PurrGO Map Viewer V4 - LOD {lod_index} "
                f"(keys 0..3)"
            )

            print(
                f"[INFO] LOD {lod_index}: "
                f"nav visited={stats['nav_visited']}, "
                f"culled={stats['nav_culled']}, "
                f"data visible={stats['data_drawn']}, "
                f"lines={stats['lines_drawn']}, "
                f"polygons={stats['polygons_drawn']}, "
                f"pois={stats['pois_drawn']}"
            )

        if is_v4:
            show_v4_lod(lod)

        pygame.display.flip()

        if screenshot is not None:
            pygame.image.save(screen, screenshot)
            print(f"[INFO] Screenshot saved: {screenshot}")
            pygame.quit()
            return True

        running = True

        lod_keys = {
            pygame.K_0: 0,
            pygame.K_1: 1,
            pygame.K_2: 2,
            pygame.K_3: 3,
        }

        while running:

            for event in pygame.event.get():

                if event.type == pygame.QUIT:
                    running = False

                elif (
                    event.type == pygame.KEYDOWN
                    and is_v4
                    and event.key in lod_keys
                ):
                    show_v4_lod(lod_keys[event.key])
                    pygame.display.flip()

        pygame.quit()

        return True

    except MapFormatError as error:

        print()
        print(
            f"[FORMAT ERROR] {error}"
        )

        pygame.quit()

        return False

    except (
        OSError,
        struct.error,
        ValueError,
        json.JSONDecodeError,
    ) as error:

        print()
        print(
            f"[ERROR] Parser failure: {error}"
        )

        pygame.quit()

        return False


# ============================================================
# Main
# ============================================================

if __name__ == "__main__":

    cli = argparse.ArgumentParser(
        description="PurrGO map viewer / validator (formats V3 and V4)"
    )
    cli.add_argument("idx", help="layer .idx file, or search.idx (V4)")
    cli.add_argument("mlp", nargs="?", default=None,
                     help='layer .mlp file, "-" for a layer without geometry')
    cli.add_argument("map_name", nargs="?", default="map.name")
    cli.add_argument("db", nargs="?", default=None,
                     help="layer .db (header check); for search.idx: pois.db")
    cli.add_argument("--lod", type=int, choices=range(V4_LOD_COUNT), default=0,
                     help="V4: displayed LOD (default 0)")
    cli.add_argument("--size-km", type=float, default=5.0,
                     help="camera size in km (default 5)")
    cli.add_argument("--screenshot", default=None,
                     help="save a PNG and exit")
    options = cli.parse_args()

    if os.path.basename(options.idx).lower() == "search.idx":

        # search.idx [pois.db]
        ok = validate_search_idx(
            options.idx,
            options.mlp if options.mlp not in (None, "-") else None,
        )

        sys.exit(0 if ok else 1)

    if options.mlp is None:
        cli.error("mlp is required (use \"-\" for a layer without .mlp)")

    success = render_map(
        options.idx,
        None if options.mlp == "-" else options.mlp,
        options.map_name,
        options.db or None,
        lod=options.lod,
        size_km=options.size_km,
        screenshot=options.screenshot,
    )

    sys.exit(
        0 if success else 1
    )
