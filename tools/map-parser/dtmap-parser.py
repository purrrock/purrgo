#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
PurrGO MAP FORMAT V3 — PC MAP VIEWER / VALIDATOR

This tool reads the binary map format produced by the PurrGO map compiler.

V3 rules implemented here:

    Global header:
        32 bytes
        Magic = "PGO"
        No format-version field

    Data Node:
        25 bytes
        <iiiiBII>
        BBox[4] + uint8 code + uint32 v1 + uint32 v2

    Navigation Node:
        28 bytes
        <IiiiiII>
        v3_jump + BBox[4] + level + child_count

    v3_jump:
        Exact byte size of the complete child subtree.
        No DT G1 / hardware prefetch compensation.

    Coordinates:
        signed int32
        degrees * 10^7

    MLP:
        PGO header
        8-byte local record header
        geometry body

The parser is intentionally strict. Corrupt or structurally
inconsistent map data is reported instead of being silently accepted.
"""

import hashlib
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

DATA_NODE_SIZE = 25
NAV_NODE_SIZE = 28

SQT_HEADER_SIZE = 16

MLP_RECORD_HEADER_SIZE = 8
MLP_BODY_HEADER_SIZE = 24

COORD_SCALE = 10_000_000.0

MAX_NUM_POINTS = 50_000
MAX_NUM_PARTS = 10_000

SQT_MAGIC = b"SQT\x01"


# ============================================================
# Viewer configuration
# ============================================================

WIDTH = 600
HEIGHT = 600


# ============================================================
# Diagnostic statistics
# ============================================================

stats = {
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


# ============================================================
# Error handling
# ============================================================

class MapFormatError(Exception):
    """Raised when a binary map violates the PurrGO V3 format."""


def fail(message):
    """
    Raise a format error.

    Keeping validation failures as exceptions makes malformed files
    impossible to silently pass through the parser.
    """
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


# ============================================================
# Camera
# ============================================================

def load_camera_bbox(map_name_path, size_km=5.0):
    """
    Load map center from map.name and calculate a camera BBox.

    map.name stores ordinary geographic degrees.

    The binary map uses integer coordinates scaled by 10^7.
    The PC renderer keeps the camera in degrees.
    """

    if not os.path.exists(map_name_path):
        print(
            f"[WARN] Файл {map_name_path} не найден. "
            f"Используются координаты по умолчанию."
        )

        center_lat = 55.7558
        center_lon = 37.6173

    else:
        with open(map_name_path, "r", encoding="utf-8") as file:
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
        111.139 * math.cos(math.radians(center_lat))
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

    X = longitude
    Y = latitude.

    Screen Y is inverted because graphical coordinates start
    at the top of the window.
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
    """
    AABB intersection test.

    Coordinates are geographic degrees.
    """

    cam_min_x, cam_min_y, cam_max_x, cam_max_y = camera_bbox

    if xmax < cam_min_x or xmin > cam_max_x:
        return False

    if ymax < cam_min_y or ymin > cam_max_y:
        return False

    return True


# ============================================================
# PGO global header
# ============================================================

def read_pgo_header(file, path):
    """
    Read and validate the 32-byte PGO header.

    V3:

        0x00  3 bytes   Magic = "PGO"
        0x03  1 byte    File magic extension
        0x04  4 bytes   Payload size, LE
        0x08  4 bytes   RAM load type, LE
        0x0C  4 bytes   LOD2 section size, BE
        0x10 16 bytes   MD5(payload)

    There is NO format-version field.
    """

    header = read_exact(
        file,
        PGO_HEADER_SIZE,
        "PGO header"
    )

    magic = header[0:3]

    if magic != b"PGO":
        fail(
            f"{path}: invalid PGO magic: {magic!r}"
        )

    magic_extension = header[3]

    payload_size = struct.unpack(
        "<I",
        header[4:8]
    )[0]

    ram_load_type = struct.unpack(
        "<I",
        header[8:12]
    )[0]

    lod2_size = struct.unpack(
        ">I",
        header[12:16]
    )[0]

    md5_expected = header[16:32]

    actual_file_size = os.fstat(
        file.fileno()
    ).st_size

    actual_payload_size = (
        actual_file_size
        - PGO_HEADER_SIZE
    )

    if payload_size != actual_payload_size:
        fail(
            f"{path}: payload size mismatch: "
            f"header={payload_size}, "
            f"actual={actual_payload_size}"
        )

    print(
        f"[INFO] PGO header: "
        f"extension=0x{magic_extension:02X}, "
        f"payload={payload_size}, "
        f"RAM=0x{ram_load_type:08X}, "
        f"LOD2={lod2_size}"
    )

    # Verify the checksum written by the current compiler.
    current_position = file.tell()

    file.seek(PGO_HEADER_SIZE)

    payload = file.read(payload_size)

    if len(payload) != payload_size:
        fail(
            f"{path}: cannot read complete payload for MD5"
        )

    md5_actual = hashlib.md5(payload).digest()

    if md5_actual != md5_expected:
        fail(
            f"{path}: MD5 mismatch: "
            f"header={md5_expected.hex()}, "
            f"actual={md5_actual.hex()}"
        )

    file.seek(current_position)

    print(
        f"[INFO] PGO MD5: {md5_actual.hex()} OK"
    )

    return {
        "magic_extension": magic_extension,
        "payload_size": payload_size,
        "ram_load_type": ram_load_type,
        "lod2_size": lod2_size,
        "md5": md5_actual,
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

    v1 is a payload-relative offset to the GEOMETRY BODY.

    The compiler writes:

        feature.v1 = len(bin_records) + 8

    Therefore:

        payload offset v1
            |
            +-- points to geometry body
            |
            -8 --> local record header

    MLP record:

        +0x00 uint32 BE   sequence number
        +0x04 uint32 LE   body length
        +0x08             geometry body

    Geometry body:

        +0x00 int32[4]    BBox
        +0x10 uint32      num_parts
        +0x14 uint32      num_points
        +0x18 uint32[]    parts
        ...               points
    """

    if mlp_file is None:
        return

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
            f"Invalid MLP content length {content_length}"
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

    # Validate geometry BBox against the actual points.
    actual_min_x = raw_points[0]
    actual_min_y = raw_points[1]
    actual_max_x = raw_points[0]
    actual_max_y = raw_points[1]

    for i in range(num_points):
        x = raw_points[i * 2]
        y = raw_points[i * 2 + 1]

        actual_min_x = min(actual_min_x, x)
        actual_min_y = min(actual_min_y, y)
        actual_max_x = max(actual_max_x, x)
        actual_max_y = max(actual_max_y, y)

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
    # Render every part independently.
    #
    # parts[] contains START INDICES.
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

    # Native POI:
    #
    # BBox is a single point.
    #
    # v1 is unused by the compiler for POIs.

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

    # Ordinary line/polygon geometry.

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

    IMPORTANT:

        v3_jump is the exact physical byte size of the
        complete child subtree.

        Therefore, after the 28-byte Navigation Node has
        already been consumed:

            seek(v3_jump, SEEK_CUR)

        skips the complete subtree.

        There is NO -8 compensation.
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

    # The compiler defines v3_jump as the exact size of
    # the child subtree.

    expected_min_jump = (
        child_count
        * DATA_NODE_SIZE
        if level == 0
        else 0
    )

    if level == 0:
        expected_jump = (
            child_count
            * DATA_NODE_SIZE
        )

        if v3_jump != expected_jump:
            fail(
                f"Invalid v3_jump for level-0 Nav Node: "
                f"jump={v3_jump}, "
                f"expected={expected_jump}, "
                f"children={child_count}"
            )

    elif child_count == 0:
        if v3_jump != 0:
            fail(
                f"Invalid empty Nav Node: "
                f"level={level}, "
                f"children=0, "
                f"v3_jump={v3_jump}"
            )

    xmin_f = xmin / COORD_SCALE
    ymin_f = ymin / COORD_SCALE
    xmax_f = xmax / COORD_SCALE
    ymax_f = ymax / COORD_SCALE

    # --------------------------------------------------------
    # BBox culling
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

        new_position = idx_file.tell()

        stats["bytes_skipped"] += (
            new_position
            - current_position
        )

        return

    # --------------------------------------------------------
    # Visible subtree
    # --------------------------------------------------------

    child_is_nav = level > 0

    for _ in range(child_count):

        parse_node(
            idx_file,
            mlp_file,
            child_is_nav,
            camera_bbox,
            screen_surface,
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
):
    """
    Read and parse exactly one node.

    V3 has different physical sizes:

        Data Node = 25 bytes
        Nav Node  = 28 bytes

    The caller must therefore know the node type before
    reading the node.
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
):
    """
    Read one 16-byte SQT section header.

    Layout:

        0x00  4 bytes  SQT magic
        0x04  4 bytes  topology marker
        0x08  4 bytes  mode/depth
        0x0C  4 bytes  root count

    Current compiler writes:

        magic          = SQT\\x01
        topology       = 1
        mode           = tree depth
        root_count     = number of root nodes
    """

    header = idx_file.read(SQT_HEADER_SIZE)

    if len(header) == 0:
        return False

    if len(header) != SQT_HEADER_SIZE:
        fail(
            f"SQT section #{section_index}: "
            f"truncated header"
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
        f"topology=0x{topology_marker:08X}, "
        f"depth={mode}, "
        f"roots={root_count}"
    )

    stats["sqt_sections"] += 1

    if root_count == 0:
        return True

    # mode is the tree depth.
    #
    # mode == 1:
    #     root Nav Nodes have level 0
    #
    # mode == 2:
    #     root Nav Nodes have level 1
    #
    # etc.
    #
    # mode == 0:
    #     root objects are Data Nodes.

    if mode == 0:
        root_is_nav = False
    else:
        root_is_nav = True

    for _ in range(root_count):

        parse_node(
            idx_file,
            mlp_file,
            root_is_nav,
            camera_bbox,
            screen_surface,
        )

    return True


# ============================================================
# Full map renderer
# ============================================================

def render_map(
    idx_path,
    mlp_path,
    map_name_path,
):
    """
    Validate and render a PurrGO V3 map layer.
    """

    print()
    print("============================================")
    print(" PurrGO MAP FORMAT V3 — PC MAP VIEWER")
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

    try:

        with (
            open(idx_path, "rb") as idx_file,
            open(mlp_path, "rb") as mlp_file,
        ):

            print(
                f"[INFO] Opening IDX: {idx_path}"
            )

            read_pgo_header(
                idx_file,
                idx_path,
            )

            print(
                f"[INFO] Opening MLP: {mlp_path}"
            )

            read_pgo_header(
                mlp_file,
                mlp_path,
            )

            section_index = 0

            while True:

                current_position = idx_file.tell()

                # Payload starts after the 32-byte header.
                #
                # EOF means that all SQT sections have been
                # consumed.

                file_size = os.fstat(
                    idx_file.fileno()
                ).st_size

                if current_position >= file_size:
                    break

                parse_sqt_section(
                    idx_file,
                    mlp_file,
                    camera_bbox,
                    screen,
                    section_index,
                )

                section_index += 1

        print()
        print("============================================")
        print(" V3 MAP VALIDATION / RENDER STATISTICS")
        print("============================================")

        print(
            f" SQT sections:        {stats['sqt_sections']}"
        )

        print(
            f" Nav visited:         {stats['nav_visited']}"
        )

        print(
            f" Nav culled:          {stats['nav_culled']}"
        )

        print(
            f" Data visited:        {stats['data_visited']}"
        )

        print(
            f" Data visible:        {stats['data_drawn']}"
        )

        print(
            f" Geometry records:    {stats['geometry_drawn']}"
        )

        print(
            f" Lines drawn:         {stats['lines_drawn']}"
        )

        print(
            f" Polygons drawn:      {stats['polygons_drawn']}"
        )

        print(
            f" POIs drawn:          {stats['pois_drawn']}"
        )

        print(
            f" Bytes skipped:       {stats['bytes_skipped']}"
        )

        print("============================================")

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

    except (OSError, struct.error) as error:

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

    if len(sys.argv) not in (3, 4):

        print(
            "Usage:"
        )

        print(
            "  python dtmap-parser.py "
            "<layer.idx> <layer.mlp> [map.name]"
        )

        sys.exit(2)

    IDX_FILE = sys.argv[1]
    MLP_FILE = sys.argv[2]

    if len(sys.argv) == 4:
        MAP_NAME = sys.argv[3]
    else:
        MAP_NAME = "map.name"

    success = render_map(
        IDX_FILE,
        MLP_FILE,
        MAP_NAME,
    )

    sys.exit(
        0 if success else 1
    )