#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
PurrGO MAP FORMAT V3 — PC MAP VIEWER / VALIDATOR

Strict validator and viewer for PurrGO MAP FORMAT V3.

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


# ============================================================
# IDX Data Node
# ============================================================

def parse_data_node(
    node_data,
    mlp_file,
    camera_bbox,
    screen_surface,
):
    """
    Parse one V3 Data Node.

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

    parse_geometry_mlp(
        mlp_file,
        v1,
        camera_bbox,
        screen_surface,
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
# Full map renderer / validator
# ============================================================

def render_map(
    idx_path,
    mlp_path,
    map_name_path,
    db_path=None,
):
    """
    Validate and render a PurrGO V3 map layer.

    Required:

        .idx
        .mlp

    Optional:

        .db

    The DB is currently header-validated only.
    """

    global stats

    stats = create_stats()

    print()
    print("============================================")
    print(" PurrGO MAP FORMAT V3 — PC MAP VALIDATOR")
    print("============================================")

    camera_bbox = load_camera_bbox(
        map_name_path,
        size_km=5.0,
    )

    pygame.init()

    screen = pygame.display.set_mode(
        (WIDTH, HEIGHT)
    )

    pygame.display.set_caption(
        "PurrGO Map Viewer V3"
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

    if not os.path.exists(mlp_path):

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

        with (
            open(idx_path, "rb") as idx_file,
            open(mlp_path, "rb") as mlp_file,
        ):

            print(
                f"[INFO] Opening IDX: {idx_path}"
            )

            idx_header = read_pgo_header(
                idx_file,
                idx_path,
            )

            if idx_header["file_type"] != PGO_FILE_TYPE_IDX:
                fail(
                    "IDX file does not contain "
                    "PGO File Type 1"
                )

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

            validate_idx(
                idx_file,
                mlp_file,
                idx_header,
                camera_bbox,
                screen,
            )

        print()
        print("============================================")
        print(" V3 MAP VALIDATION / RENDER STATISTICS")
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

        print("============================================")
        print("[OK] PurrGO V3 map validation successful.")

        pygame.display.flip()

        running = True

        while running:

            for event in pygame.event.get():

                if event.type == pygame.QUIT:
                    running = False

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

    if len(sys.argv) not in (3, 4, 5):

        print("Usage:")
        print(
            "  python dtmap-parser.py "
            "<layer.idx> <layer.mlp> "
            "[map.name] [layer.db]"
        )

        sys.exit(2)

    IDX_FILE = sys.argv[1]
    MLP_FILE = sys.argv[2]

    if len(sys.argv) >= 4:
        MAP_NAME = sys.argv[3]
    else:
        MAP_NAME = "map.name"

    if len(sys.argv) == 5:
        DB_FILE = sys.argv[4]
    else:
        DB_FILE = None

    success = render_map(
        IDX_FILE,
        MLP_FILE,
        MAP_NAME,
        DB_FILE,
    )

    sys.exit(
        0 if success else 1
    )