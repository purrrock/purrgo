import struct
import os
import json
import math
import pygame

# --- Настройки экрана ---
WIDTH, HEIGHT = 600, 600

# Коэффициент координат нового формата PurrGO (V2).
# Географические координаты хранятся как signed int32 (10⁻⁷ degrees):
# coordinate = degrees * COORD_SCALE
COORD_SCALE = 10_000_000.0

# Глобальные счетчики для диагностики
stats = {
    "nav_visited": 0,
    "nav_culled": 0,
    "data_visited": 0,
    "data_drawn": 0,
    "lines_drawn": 0,
    "polygons_drawn": 0,
    "pois_drawn": 0
}


def load_camera_bbox(map_name_path, size_km=5.0):
    """
    Загрузка центра из map.name и расчет BBox размером size_km x size_km.
    """
    if not os.path.exists(map_name_path):
        print(f"[ERROR] Файл {map_name_path} не найден! Проверьте путь.")
        center_lat, center_lon = 55.7558, 37.6173
    else:
        with open(map_name_path, "r", encoding="utf-8") as f:
            data = json.load(f)
            center_lat = float(data["centerLat"])
            center_lon = float(data["centerLon"])
            print(f"[INFO] Прочитан {map_name_path}: centerLat={center_lat}, centerLon={center_lon}")

    half_km = size_km / 2.0
    d_lat = half_km / 111.139
    d_lon = half_km / (111.139 * math.cos(math.radians(center_lat)))

    min_x = center_lon - d_lon
    max_x = center_lon + d_lon
    min_y = center_lat - d_lat
    max_y = center_lat + d_lat

    print(f"[INFO] BBox камеры ({size_km:g}x{size_km:g} км):")
    print(f"       Lon (X): min={min_x:.7f}, max={max_x:.7f}")
    print(f"       Lat (Y): min={min_y:.7f}, max={max_y:.7f}")

    return min_x, min_y, max_x, max_y


def world_to_screen(x, y, camera_bbox):
    """
    Проекция географических координат в пиксели окна.
    """
    cam_min_x, cam_min_y, cam_max_x, cam_max_y = camera_bbox
    
    screen_x = int((x - cam_min_x) / (cam_max_x - cam_min_x) * WIDTH)
    screen_y = HEIGHT - int((y - cam_min_y) / (cam_max_y - cam_min_y) * HEIGHT)

    return screen_x, screen_y


def is_in_screen(xmin, ymin, xmax, ymax, camera_bbox):
    """
    Проверка пересечения BBox узла с BBox камеры.
    """
    cam_min_x, cam_min_y, cam_max_x, cam_max_y = camera_bbox

    if xmax < cam_min_x or xmin > cam_max_x:
        return False
    if ymax < cam_min_y or ymin > cam_max_y:
        return False

    return True


def read_yzl_header(file):
    """
    Чтение и валидация 32-байтного глобального заголовка YZL.
    """
    header_data = file.read(32)
    if len(header_data) < 32:
        print("[ERROR] Недостаточно байт для YZL заголовка.")
        return False

    magic = header_data[0:3]
    if magic != b"YZL":
        print(f"[ERROR] Неверная сигнатура YZL: {magic}")
        return False

    payload_size = struct.unpack("<I", header_data[4:8])[0]
    print(f"[INFO] Заголовок YZL корректен. Payload Size: {payload_size} байт.")

    return True


def parse_geometry_mlp(mlp_file, v1_offset, camera_bbox, screen_surface):
    """
    Чтение геометрии из MLP.
    Координаты хранятся как int32 (degrees * 10^7).
    """
    if not mlp_file:
        return

    absolute_body_offset = 32 + v1_offset
    mlp_file.seek(absolute_body_offset)

    head_data = mlp_file.read(24)
    if len(head_data) < 24:
        return

    minx, miny, maxx, maxy, num_parts, num_points = struct.unpack("<iiiiii", head_data)

    if num_parts < 0 or num_points < 0 or num_points > 50000:
        return

    remaining_size = (num_parts * 4) + (num_points * 8)
    if remaining_size <= 0:
        return

    body_data = mlp_file.read(remaining_size)
    if len(body_data) < remaining_size:
        return

    parts = struct.unpack_from(f"<{num_parts}I", body_data, 0)
    points_offset = num_parts * 4
    raw_points = struct.unpack_from(f"<{num_points * 2}i", body_data, points_offset)

    screen_points = []
    for i in range(num_points):
        lon = raw_points[i * 2] / COORD_SCALE
        lat = raw_points[i * 2 + 1] / COORD_SCALE
        screen_points.append(world_to_screen(lon, lat, camera_bbox))

    if len(screen_points) >= 2:
        end_idx = parts[1] if num_parts > 1 else num_points
        contour = screen_points[0:end_idx]

        if len(contour) > 2 and contour[0] == contour[-1] and parts[0] == 0:
            pygame.draw.polygon(screen_surface, (80, 140, 80), contour, 1)
            stats["polygons_drawn"] += 1
        else:
            pygame.draw.lines(screen_surface, (220, 220, 220), False, screen_points, 2)
            stats["lines_drawn"] += 1


def parse_node(idx_file, mlp_file, is_nav_node, current_level, camera_bbox, screen_surface):
    """
    Разбор 28-байтного IDX узла с поддержкой POI (V2).
    """
    node_data = idx_file.read(28)
    if len(node_data) < 28:
        return

    if not is_nav_node:
        stats["data_visited"] += 1
        xmin, ymin, xmax, ymax, obj_type, v1, v2 = struct.unpack("<iiiiIII", node_data)

        # Конвертация BBox в градусы для отсечения
        if is_in_screen(xmin / COORD_SCALE, ymin / COORD_SCALE, 
                        xmax / COORD_SCALE, ymax / COORD_SCALE, camera_bbox):
            stats["data_drawn"] += 1

            # --- ИДЕНТИФИКАЦИЯ V2 POI ---
            if xmin == xmax and ymin == ymax:
                # В V2 нативные POI хранят свои координаты (int32) прямо в BBox,
                # указатель v1 игнорируется, а файл .mlp не требуется.
                stats["pois_drawn"] += 1
                sx, sy = world_to_screen(xmin / COORD_SCALE, ymin / COORD_SCALE, camera_bbox)
                pygame.draw.circle(screen_surface, (255, 80, 80), (sx, sy), 4)
            elif v1 > 0:
                parse_geometry_mlp(mlp_file, v1, camera_bbox, screen_surface)
        return

    # Обработка Nav Node
    stats["nav_visited"] += 1
    v3_jump, c_xmin, c_ymin, c_xmax, c_ymax, nav_level, obj_count = struct.unpack("<IiiiiII", node_data)

    if not is_in_screen(c_xmin / COORD_SCALE, c_ymin / COORD_SCALE, 
                        c_xmax / COORD_SCALE, c_ymax / COORD_SCALE, camera_bbox):
        stats["nav_culled"] += 1
        idx_file.seek(v3_jump - 8, os.SEEK_CUR)
        return

    child_is_nav = nav_level > 0
    child_level = nav_level - 1 if nav_level > 0 else 0

    for _ in range(obj_count):
        parse_node(idx_file, mlp_file, child_is_nav, child_level, camera_bbox, screen_surface)


def render_map(idx_path, mlp_path, map_name_path):
    print("=== ЗАПУСК РЕНДЕРА PURRGO MAP V2 ===")

    camera_bbox = load_camera_bbox(map_name_path, size_km=5.0)

    pygame.init()
    screen = pygame.display.set_mode((WIDTH, HEIGHT))
    pygame.display.set_caption("PurrGO Vector Renderer (600x600)")
    screen.fill((20, 20, 20))

    if not os.path.exists(idx_path):
        print(f"[ERROR] Не найден файл индекса: {idx_path}")
        return

    # В V2 POI-слои больше не имеют файла .mlp
    has_mlp = os.path.exists(mlp_path)
    if not has_mlp:
        print(f"[INFO] Файл геометрии {mlp_path} не найден. Работа в режиме POI-only.")

    with open(idx_path, "rb") as idx_file:
        mlp_file = open(mlp_path, "rb") if has_mlp else None
        print(f"[INFO] Открытие слоя: {idx_path}")

        if not read_yzl_header(idx_file):
            return

        sqt_index = 0
        while True:
            sqt_header = idx_file.read(16)
            if not sqt_header or len(sqt_header) < 16:
                break

            magic, topo_marker, mode, count = struct.unpack("<4sIII", sqt_header)
            if magic != b"SQT\x01":
                print(f"[WARN] Некорректный заголовок SQT: {magic}")
                break

            print(f"[INFO] Найдена секция SQT #{sqt_index}: Mode={mode}, Root Nodes Count={count}")
            sqt_index += 1

            if count == 0:
                continue

            is_nav = mode > 0
            level = mode - 1 if mode > 0 else 0

            for _ in range(count):
                parse_node(idx_file, mlp_file, is_nav, level, camera_bbox, screen)

        if mlp_file:
            mlp_file.close()

    print("\n=== СТАТИСТИКА РЕНДЕРИНГА ===")
    print(f"Nav Nodes обработано:  {stats['nav_visited']}")
    print(f"Nav Nodes отброшено:   {stats['nav_culled']} (Z-Culling)")
    print(f"Data Nodes проверено:  {stats['data_visited']}")
    print(f"Data Nodes видимы:     {stats['data_drawn']}")
    print(f"Отрисовано линий:      {stats['lines_drawn']}")
    print(f"Отрисовано полигонов:  {stats['polygons_drawn']}")
    print(f"Отрисовано POI точек:  {stats['pois_drawn']}")
    print("=============================\n")

    pygame.display.flip()

    running = True
    while running:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False

    pygame.quit()


if __name__ == "__main__":
    IDX_FILE = "roads.idx"
    MLP_FILE = "roads.mlp"
    MAP_NAME = "map.name"

    render_map(IDX_FILE, MLP_FILE, MAP_NAME)