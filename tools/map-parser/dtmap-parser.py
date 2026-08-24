import struct
import os
import json
import math
import pygame


# ============================================================
# PurrGO MAP FORMAT V2 — PC MAP VIEWER
# ============================================================

# Размер окна визуализатора.
WIDTH = 600
HEIGHT = 600

# Fixed-point coordinate scale used by PurrGO MAP FORMAT V2.
#
# Binary map coordinates:
#
#     integer_coordinate = geographic_coordinate * 10^7
#
# Example:
#
#     37.6173000° -> 376173000
#
# Coordinates are stored as signed int32 in IDX and MLP.
COORD_SCALE = 10_000_000.0


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
}


# ============================================================
# Camera
# ============================================================

def load_camera_bbox(map_name_path, size_km=5.0):
    """
    Load map center from map.name and calculate a camera BBox.

    map.name stores coordinates in ordinary degrees.

    The binary map itself uses the 10^7 fixed-point representation,
    but the PC renderer keeps camera coordinates in degrees and
    converts map coordinates only when comparing/projecting them.
    """

    if not os.path.exists(map_name_path):
        print(
            f"[ERROR] Файл {map_name_path} не найден! "
            f"Используются координаты по умолчанию."
        )

        center_lat = 55.7558
        center_lon = 37.6173

    else:
        with open(map_name_path, "r", encoding="utf-8") as f:
            data = json.load(f)

        center_lat = float(data["centerLat"])
        center_lon = float(data["centerLon"])

        print(
            f"[INFO] Прочитан {map_name_path}: "
            f"centerLat={center_lat}, "
            f"centerLon={center_lon}"
        )

    half_km = size_km / 2.0

    # Approximate conversion used only by the PC viewer.
    d_lat = half_km / 111.139

    d_lon = half_km / (
        111.139 * math.cos(math.radians(center_lat))
    )

    min_x = center_lon - d_lon
    max_x = center_lon + d_lon

    min_y = center_lat - d_lat
    max_y = center_lat + d_lat

    print(f"[INFO] BBox камеры ({size_km:g}x{size_km:g} км):")
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
    Y = latitude

    Screen Y is inverted because graphical coordinates start at
    the top of the window.
    """

    cam_min_x, cam_min_y, cam_max_x, cam_max_y = camera_bbox

    screen_x = int(
        (x - cam_min_x)
        / (cam_max_x - cam_min_x)
        * WIDTH
    )

    screen_y = HEIGHT - int(
        (y - cam_min_y)
        / (cam_max_y - cam_min_y)
        * HEIGHT
    )

    return screen_x, screen_y


def is_in_screen(
    xmin,
    ymin,
    xmax,
    ymax,
    camera_bbox
):
    """
    AABB intersection test.

    All arguments are geographic coordinates in degrees.
    """

    cam_min_x, cam_min_y, cam_max_x, cam_max_y = camera_bbox

    if xmax < cam_min_x or xmin > cam_max_x:
        return False

    if ymax < cam_min_y or ymin > cam_max_y:
        return False

    return True


# ============================================================
# YZL container
# ============================================================

def read_yzl_header(file):
    """
    Read and validate the 32-byte global YZL header.

    V2:

        0x00  3 bytes  Magic = "YZL"
        0x03  1 byte   Magic Extension
        0x04  4 bytes  Payload Size, LE
        0x08  4 bytes  RAM Load Type
        0x0C  4 bytes  LOD 2 Section Size, BE
        0x10 16 bytes  MD5
    """

    header_data = file.read(32)

    if len(header_data) != 32:
        print(
            "[ERROR] Недостаточно байт для YZL заголовка."
        )
        return False

    magic = header_data[0:3]

    if magic != b"YZL":
        print(
            f"[ERROR] Неверная сигнатура YZL: {magic!r}"
        )
        return False

    magic_extension = header_data[3]

    payload_size = struct.unpack(
        "<I",
        header_data[4:8]
    )[0]

    ram_load_type = struct.unpack(
        "<I",
        header_data[8:12]
    )[0]

    lod2_size = struct.unpack(
        ">I",
        header_data[12:16]
    )[0]

    print(
        "[INFO] YZL header:"
        f" extension=0x{magic_extension:02X},"
        f" payload={payload_size},"
        f" RAM=0x{ram_load_type:08X},"
        f" LOD2={lod2_size}"
    )

    return True


# ============================================================
# MLP geometry
# ============================================================

def parse_geometry_mlp(
    mlp_file,
    v1_offset,
    camera_bbox,
    screen_surface
):
    """
    Read one MLP geometry record.

    IMPORTANT:
    The current map compiler writes:

        feature.v1 = len(bin_records) + 8

    Therefore v1 points directly to the geometry BODY and skips
    the 8-byte local record header.

    Local MLP record format:

        +0x00 uint32 BE   sequence number
        +0x04 uint32 LE   content length
        +0x08             geometry body

    Geometry body:

        +0x00 int32[4]    bbox
        +0x10 int32       num_parts
        +0x14 int32       num_points
        +0x18 uint32[]    parts
        ...               points
    """

    if mlp_file is None:
        return

    # v1 is relative to the beginning of the YZL payload.
    body_offset = 32 + v1_offset

    # The local header begins 8 bytes before v1.
    record_header_offset = body_offset - 8

    if record_header_offset < 32:
        return

    mlp_file.seek(record_header_offset)

    local_header = mlp_file.read(8)

    if len(local_header) != 8:
        return

    sequence_number = struct.unpack(
        ">I",
        local_header[0:4]
    )[0]

    content_length = struct.unpack(
        "<I",
        local_header[4:8]
    )[0]

    # The compiler starts sequence numbering at 1.
    if sequence_number == 0:
        return

    if content_length < 24:
        return

    # v1 points to the body, so read exactly content_length bytes.
    body_data = mlp_file.read(content_length)

    if len(body_data) != content_length:
        return

    # Fixed geometry body header.
    minx, miny, maxx, maxy, num_parts, num_points = struct.unpack(
        "<iiiiii",
        body_data[:24]
    )

    if num_parts <= 0:
        return

    if num_points <= 0:
        return

    # Avoid allocating pathological amounts of memory if the file
    # is damaged.
    if num_points > 50000:
        return

    expected_size = (
        24
        + num_parts * 4
        + num_points * 8
    )

    if expected_size != content_length:
        print(
            "[WARN] Некорректная длина MLP record:"
            f" sequence={sequence_number},"
            f" declared={content_length},"
            f" expected={expected_size}"
        )
        return

    parts_offset = 24
    points_offset = (
        parts_offset
        + num_parts * 4
    )

    parts = struct.unpack_from(
        f"<{num_parts}I",
        body_data,
        parts_offset
    )

    raw_points = struct.unpack_from(
        f"<{num_points * 2}i",
        body_data,
        points_offset
    )

    # Validate parts indices.
    previous_part = -1

    for part_index in parts:
        if part_index >= num_points:
            return

        if part_index < previous_part:
            return

        previous_part = part_index

    # Convert all points from fixed-point int32 to degrees.
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
    # Draw every part/ring separately.
    #
    # parts[] contains START INDICES, not lengths.
    # --------------------------------------------------------

    for part_index in range(num_parts):

        start = parts[part_index]

        if part_index + 1 < num_parts:
            end = parts[part_index + 1]
        else:
            end = num_points

        if end <= start:
            continue

        contour = screen_points[start:end]

        if len(contour) < 2:
            continue

        # A closed ring is rendered as a polygon outline.
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
# IDX nodes
# ============================================================

def parse_node(
    idx_file,
    mlp_file,
    is_nav_node,
    current_level,
    camera_bbox,
    screen_surface
):
    """
    Parse one 28-byte IDX node.

    Data Node:

        <iiiiIII>

        int32  xmin
        int32  ymin
        int32  xmax
        int32  ymax
        uint32 Type
        uint32 v1
        uint32 v2

    Navigation Node:

        <IiiiiII>

        uint32 v3_jump
        int32  xmin
        int32  ymin
        int32  xmax
        int32  ymax
        uint32 level
        uint32 child_count
    """

    node_data = idx_file.read(28)

    if len(node_data) != 28:
        return

    # ========================================================
    # Data Node
    # ========================================================

    if not is_nav_node:

        stats["data_visited"] += 1

        (
            xmin,
            ymin,
            xmax,
            ymax,
            obj_type,
            v1,
            v2
        ) = struct.unpack(
            "<iiiiIII",
            node_data
        )

        xmin_f = xmin / COORD_SCALE
        ymin_f = ymin / COORD_SCALE
        xmax_f = xmax / COORD_SCALE
        ymax_f = ymax / COORD_SCALE

        if not is_in_screen(
            xmin_f,
            ymin_f,
            xmax_f,
            ymax_f,
            camera_bbox
        ):
            return

        stats["data_drawn"] += 1

        # ----------------------------------------------------
        # Native POI
        #
        # V2:
        #
        #     xmin == xmax
        #     ymin == ymax
        #
        # v1 is unused.
        # ----------------------------------------------------

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

        # ----------------------------------------------------
        # Ordinary geometry feature
        # ----------------------------------------------------

        if v1 != 0:

            parse_geometry_mlp(
                mlp_file,
                v1,
                camera_bbox,
                screen_surface
            )

        return

    # ========================================================
    # Navigation Node
    # ========================================================

    stats["nav_visited"] += 1

    (
        v3_jump,
        c_xmin,
        c_ymin,
        c_xmax,
        c_ymax,
        nav_level,
        obj_count
    ) = struct.unpack(
        "<IiiiiII",
        node_data
    )

    c_xmin_f = c_xmin / COORD_SCALE
    c_ymin_f = c_ymin / COORD_SCALE
    c_xmax_f = c_xmax / COORD_SCALE
    c_ymax_f = c_ymax / COORD_SCALE

    # --------------------------------------------------------
    # BBox culling
    # --------------------------------------------------------

    if not is_in_screen(
        c_xmin_f,
        c_ymin_f,
        c_xmax_f,
        c_ymax_f,
        camera_bbox
    ):

        stats["nav_culled"] += 1

        # V2 hardware prefetch compensation:
        #
        # v3_jump includes the 8-byte compensation.
        #
        # Keep exactly the interpretation used by the firmware
        # format.
        if v3_jump < 8:
            print(
                "[WARN] Некорректный v3_jump:"
                f" {v3_jump}"
            )
            return

        idx_file.seek(
            v3_jump - 8,
            os.SEEK_CUR
        )

        return

    # --------------------------------------------------------
    # Determine child node type from navigation depth.
    #
    # level > 0:
    #     children are Navigation Nodes
    #
    # level == 0:
    #     children are Data Nodes
    # --------------------------------------------------------

    child_is_nav = nav_level > 0

    child_level = (
        nav_level - 1
        if nav_level > 0
        else 0
    )

    for _ in range(obj_count):

        parse_node(
            idx_file,
            mlp_file,
            child_is_nav,
            child_level,
            camera_bbox,
            screen_surface
        )


# ============================================================
# Map renderer
# ============================================================

def render_map(
    idx_path,
    mlp_path,
    map_name_path
):
    """
    Render one PurrGO IDX layer.

    For ordinary geometry layers:

        IDX + MLP

    For native POI layer:

        IDX only
    """

    print(
        "=== ЗАПУСК РЕНДЕРА PURRGO MAP V2 ==="
    )

    camera_bbox = load_camera_bbox(
        map_name_path,
        size_km=5.0
    )

    pygame.init()

    screen = pygame.display.set_mode(
        (WIDTH, HEIGHT)
    )

    pygame.display.set_caption(
        "PurrGO Vector Renderer V2"
    )

    screen.fill((20, 20, 20))

    if not os.path.exists(idx_path):

        print(
            f"[ERROR] Не найден файл индекса: "
            f"{idx_path}"
        )

        pygame.quit()
        return

    has_mlp = (
        mlp_path is not None
        and os.path.exists(mlp_path)
    )

    if has_mlp:

        print(
            f"[INFO] Открытие geometry: "
            f"{mlp_path}"
        )

    else:

        print(
            "[INFO] MLP отсутствует. "
            "Работа в режиме native POI."
        )

    mlp_file = None

    try:

        with open(idx_path, "rb") as idx_file:

            if has_mlp:
                mlp_file = open(
                    mlp_path,
                    "rb"
                )

            print(
                f"[INFO] Открытие слоя: "
                f"{idx_path}"
            )

            if not read_yzl_header(idx_file):
                return

            # ------------------------------------------------
            # Read all LOD sections sequentially.
            #
            # Every IDX contains:
            #
            #     LOD 0
            #     LOD 1
            #     LOD 2
            #
            # Each starts with a 16-byte SQT header.
            # ------------------------------------------------

            sqt_index = 0

            while True:

                sqt_header = idx_file.read(16)

                if not sqt_header:
                    break

                if len(sqt_header) != 16:

                    print(
                        "[WARN] Неполный SQT header."
                    )

                    break

                (
                    magic,
                    topology_marker,
                    depth,
                    root_count
                ) = struct.unpack(
                    "<4sIII",
                    sqt_header
                )

                if magic != b"SQT\x01":

                    print(
                        "[WARN] Некорректный "
                        f"SQT header: {magic!r}"
                    )

                    break

                print(
                    f"[INFO] SQT section #{sqt_index}: "
                    f"depth={depth}, "
                    f"root_nodes={root_count}"
                )

                stats["sqt_sections"] += 1

                sqt_index += 1

                if root_count == 0:
                    continue

                # depth == 0:
                #
                # root nodes are Data Nodes.
                #
                # depth > 0:
                #
                # root nodes are Navigation Nodes.
                is_nav = depth > 0

                level = (
                    depth - 1
                    if depth > 0
                    else 0
                )

                for _ in range(root_count):

                    parse_node(
                        idx_file,
                        mlp_file,
                        is_nav,
                        level,
                        camera_bbox,
                        screen
                    )

    finally:

        if mlp_file is not None:
            mlp_file.close()

    # ========================================================
    # Diagnostics
    # ========================================================

    print()
    print(
        "=== СТАТИСТИКА РЕНДЕРИНГА ==="
    )

    print(
        f"SQT sections:          "
        f"{stats['sqt_sections']}"
    )

    print(
        f"Nav Nodes обработано:  "
        f"{stats['nav_visited']}"
    )

    print(
        f"Nav Nodes отброшено:   "
        f"{stats['nav_culled']}"
    )

    print(
        f"Data Nodes проверено:  "
        f"{stats['data_visited']}"
    )

    print(
        f"Data Nodes видимы:     "
        f"{stats['data_drawn']}"
    )

    print(
        f"Geometry features:     "
        f"{stats['geometry_drawn']}"
    )

    print(
        f"Отрисовано линий:      "
        f"{stats['lines_drawn']}"
    )

    print(
        f"Отрисовано полигонов:  "
        f"{stats['polygons_drawn']}"
    )

    print(
        f"Отрисовано POI точек:  "
        f"{stats['pois_drawn']}"
    )

    print(
        "============================="
    )

    pygame.display.flip()

    # ========================================================
    # Main Pygame event loop
    # ========================================================

    running = True

    while running:

        for event in pygame.event.get():

            if event.type == pygame.QUIT:
                running = False

    pygame.quit()


# ============================================================
# Entry point
# ============================================================

if __name__ == "__main__":

    # --------------------------------------------------------
    # Default test configuration.
    #
    # For roads / landuse / water:
    #
    #     IDX_FILE = "...idx"
    #     MLP_FILE = "...mlp"
    #
    # For POI:
    #
    #     MLP_FILE = None
    # --------------------------------------------------------

    IDX_FILE = "roads.idx"
    MLP_FILE = "roads.mlp"
    MAP_NAME = "map.name"

    render_map(
        IDX_FILE,
        MLP_FILE,
        MAP_NAME
    )