import struct
import os
import json
import math
import pygame

# --- Настройки экрана ---
WIDTH, HEIGHT = 600, 600

# Коэффициент координат нового формата PurrGO.
#
# В бинарных файлах географические координаты хранятся как int32:
#
#     coordinate = degrees * COORD_SCALE
#
# Например:
#
#     55.7558000° -> 557558000
#
# Это относится как к MLP geometry, так и к IDX BBox.
COORD_SCALE = 10_000_000.0

# Глобальные счетчики для диагностики
stats = {
    "nav_visited": 0,
    "nav_culled": 0,
    "data_visited": 0,
    "data_drawn": 0,
    "lines_drawn": 0,
    "polygons_drawn": 0
}


def load_camera_bbox(map_name_path, size_km=5.0):
    """
    Загрузка центра из map.name и расчет BBox размером size_km x size_km
    в градусах.

    map.name содержит координаты в обычных градусах.
    В отличие от координат карты, координаты камеры НЕ масштабируются
    до COORD_SCALE до тех пор, пока это не требуется для сравнения.
    """
    if not os.path.exists(map_name_path):
        print(f"[ERROR] Файл {map_name_path} не найден! Проверьте путь.")

        # Дефолтные координаты в случае отсутствия файла.
        center_lat, center_lon = 55.7558, 37.6173
    else:
        with open(map_name_path, "r", encoding="utf-8") as f:
            data = json.load(f)

            center_lat = float(data["centerLat"])
            center_lon = float(data["centerLon"])

            print(
                f"[INFO] Прочитан {map_name_path}: "
                f"centerLat={center_lat}, centerLon={center_lon}"
            )

    # Половина ширины/высоты области в километрах.
    half_km = size_km / 2.0

    # 1 градус широты ≈ 111.139 км.
    d_lat = half_km / 111.139

    # 1 градус долготы ≈ 111.139 * cos(lat) км.
    d_lon = half_km / (
        111.139 * math.cos(math.radians(center_lat))
    )

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
    Проекция географических координат в пиксели окна 600x600.

    x и y передаются в градусах.
    """
    cam_min_x, cam_min_y, cam_max_x, cam_max_y = camera_bbox

    screen_x = int(
        (x - cam_min_x)
        / (cam_max_x - cam_min_x)
        * WIDTH
    )

    # Инверсия оси Y:
    # в графических системах координата Y=0 находится сверху.
    screen_y = HEIGHT - int(
        (y - cam_min_y)
        / (cam_max_y - cam_min_y)
        * HEIGHT
    )

    return screen_x, screen_y


def is_in_screen(xmin, ymin, xmax, ymax, camera_bbox):
    """
    Проверка пересечения BBox узла с BBox камеры (AABB Culling).

    Координаты узла передаются в градусах.
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

    print(
        f"[INFO] Заголовок YZL корректен. "
        f"Payload Size: {payload_size} байт."
    )

    return True


def parse_geometry_mlp(
    mlp_file,
    v1_offset,
    camera_bbox,
    screen_surface
):
    """
    Чтение geometry из MLP по указателю v1 из IDX.

    v1_offset — относительное смещение от конца глобального
    YZL-заголовка (32 байта).

    v1 указывает непосредственно на тело geometry record,
    то есть пропускает 8-байтный локальный заголовок.

    Новый формат координат:
        int32 = degrees * 10^7
    """

    # Вычисление абсолютного адреса поля minx.
    absolute_body_offset = 32 + v1_offset

    mlp_file.seek(absolute_body_offset)

    # Фиксированная часть geometry body:
    #
    # bbox_int      = 4 * int32 = 16 байт
    # num_parts     = int32      = 4 байта
    # num_points    = int32      = 4 байта
    #
    # Итого: 24 байта.
    head_data = mlp_file.read(24)

    if len(head_data) < 24:
        return

    minx, miny, maxx, maxy, num_parts, num_points = struct.unpack(
        "<iiiiii",
        head_data
    )

    # Защита от поврежденных указателей или мусорных данных.
    if num_parts < 0 or num_points < 0 or num_points > 50000:
        return

    # Payload:
    #
    # parts:
    #     num_parts * uint32
    #
    # points:
    #     num_points * (int32 X + int32 Y)
    #
    # По 8 байт на точку.
    remaining_size = (
        num_parts * 4
        + num_points * 8
    )

    if remaining_size <= 0:
        return

    body_data = mlp_file.read(remaining_size)

    if len(body_data) < remaining_size:
        return

    # Распаковка массива parts.
    parts = struct.unpack_from(
        f"<{num_parts}I",
        body_data,
        0
    )

    points_offset = num_parts * 4

    # Распаковка координат как int32.
    raw_points = struct.unpack_from(
        f"<{num_points * 2}i",
        body_data,
        points_offset
    )

    # Проекция.
    screen_points = []

    for i in range(num_points):
        # В бинарном формате:
        #
        #     integer = coordinate * 10^7
        #
        # Для pygame/world_to_screen возвращаемся
        # к обычным градусам.
        lon = raw_points[i * 2] / COORD_SCALE
        lat = raw_points[i * 2 + 1] / COORD_SCALE

        screen_points.append(
            world_to_screen(
                lon,
                lat,
                camera_bbox
            )
        )

    # Отрисовка.
    if len(screen_points) >= 2:

        # Если num_parts > 1, это мультиполигон с несколькими
        # контурами. В базовом renderer пока отрисовываем только
        # первый внешний контур.
        end_idx = (
            parts[1]
            if num_parts > 1
            else num_points
        )

        contour = screen_points[0:end_idx]

        if (
            len(contour) > 2
            and contour[0] == contour[-1]
            and parts[0] == 0
        ):
            # Замкнутый полигон.
            pygame.draw.polygon(
                screen_surface,
                (80, 140, 80),
                contour,
                1
            )

            stats["polygons_drawn"] += 1

        else:
            # Линия.
            pygame.draw.lines(
                screen_surface,
                (220, 220, 220),
                False,
                screen_points,
                2
            )

            stats["lines_drawn"] += 1


def parse_node(
    idx_file,
    mlp_file,
    is_nav_node,
    current_level,
    camera_bbox,
    screen_surface
):
    """
    Разбор 28-байтного IDX узла.

    Новый формат IDX:

    Data Node:
        <iiiiIII>

        int32 xmin
        int32 ymin
        int32 xmax
        int32 ymax
        uint32 type
        uint32 v1
        uint32 v2

    Nav Node:
        <IiiiiII>

        uint32 v3_jump
        int32  xmin
        int32  ymin
        int32  xmax
        int32  ymax
        uint32 level
        uint32 child_count

    Все координаты имеют масштаб 10^7.
    """

    node_data = idx_file.read(28)

    if len(node_data) < 28:
        return

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

        # IDX хранит BBox как int32 * 10^7.
        #
        # Для геометрического сравнения с camera_bbox
        # переводим значения обратно в градусы.
        if is_in_screen(
            xmin / COORD_SCALE,
            ymin / COORD_SCALE,
            xmax / COORD_SCALE,
            ymax / COORD_SCALE,
            camera_bbox
        ):
            stats["data_drawn"] += 1

            if v1 > 0:
                parse_geometry_mlp(
                    mlp_file,
                    v1,
                    camera_bbox,
                    screen_surface
                )

        return

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

    # Nav Node BBox хранится как int32 * 10^7.
    c_xmin_f = c_xmin / COORD_SCALE
    c_ymin_f = c_ymin / COORD_SCALE
    c_xmax_f = c_xmax / COORD_SCALE
    c_ymax_f = c_ymax / COORD_SCALE

    if not is_in_screen(
        c_xmin_f,
        c_ymin_f,
        c_xmax_f,
        c_ymax_f,
        camera_bbox
    ):
        stats["nav_culled"] += 1

        # Аппаратный пропуск дочернего поддерева.
        #
        # v3_jump содержит компенсацию +8 байт.
        idx_file.seek(
            v3_jump - 8,
            os.SEEK_CUR
        )

        return

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


def render_map(idx_path, mlp_path, map_name_path):
    print("=== ЗАПУСК РЕНДЕРА PURRGO MAP ===")

    # 1. Расчет камеры по map.name.
    camera_bbox = load_camera_bbox(
        map_name_path,
        size_km=5.0
    )

    pygame.init()

    screen = pygame.display.set_mode(
        (WIDTH, HEIGHT)
    )

    pygame.display.set_caption(
        "PurrGO Vector Renderer (600x600)"
    )

    screen.fill((20, 20, 20))

    if (
        not os.path.exists(idx_path)
        or not os.path.exists(mlp_path)
    ):
        print(
            f"[ERROR] Не найдены файлы карт: "
            f"{idx_path} или {mlp_path}"
        )
        return

    # 2. Обход IDX.
    with (
        open(idx_path, "rb") as idx_file,
        open(mlp_path, "rb") as mlp_file
    ):
        print(
            f"[INFO] Открытие слоя: {idx_path}"
        )

        if not read_yzl_header(idx_file):
            return

        sqt_index = 0

        while True:
            sqt_header = idx_file.read(16)

            if not sqt_header or len(sqt_header) < 16:
                break

            magic, topo_marker, mode, count = struct.unpack(
                "<4sIII",
                sqt_header
            )

            if magic != b"SQT\x01":
                print(
                    f"[WARN] Некорректный заголовок SQT: "
                    f"{magic}"
                )
                break

            print(
                f"[INFO] Найдена секция SQT #{sqt_index}: "
                f"Mode={mode}, Root Nodes Count={count}"
            )

            sqt_index += 1

            if count == 0:
                continue

            is_nav = mode > 0

            level = (
                mode - 1
                if mode > 0
                else 0
            )

            for _ in range(count):
                parse_node(
                    idx_file,
                    mlp_file,
                    is_nav,
                    level,
                    camera_bbox,
                    screen
                )

    # 3. Итоговая статистика.
    print("=== СТАТИСТИКА РЕНДЕRИНГА ===")
    print(
        f"Nav Nodes обработано:  "
        f"{stats['nav_visited']}"
    )
    print(
        f"Nav Nodes отброшено:   "
        f"{stats['nav_culled']} (Z-Culling)"
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
        f"Отрисовано линий:      "
        f"{stats['lines_drawn']}"
    )
    print(
        f"Отрисовано полигонов:  "
        f"{stats['polygons_drawn']}"
    )
    print("=============================")

    pygame.display.flip()

    # Главный цикл Pygame.
    running = True

    while running:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False

    pygame.quit()


if __name__ == "__main__":
    # Пути к тестовому слою и конфигурации.
    IDX_FILE = "roads.idx"
    MLP_FILE = "roads.mlp"
    MAP_NAME = "map.name"

    render_map(
        IDX_FILE,
        MLP_FILE,
        MAP_NAME
    )