import math

# Целевые константы PurrGO Test Suite
LAT_CENTER = 53.713536
LON_CENTER = 28.4193615
GRID_SIZE = 70        # Сетка 70x70 = 4900 независимых полигонов
SPACING = 0.0005      # Шаг сетки (~55 метров)
POLY_SIZE = 0.0002    # Размер самого полигона (~22 метра)

def generate_large_v3_jump_osm(filename="large-v3-jump.osm"):
    with open(filename, "w", encoding="utf-8") as f:
        f.write('<?xml version="1.0" encoding="UTF-8"?>\n')
        f.write('<osm version="0.6" generator="PurrGO_Test_Suite">\n')
        
        # Вычисление границ BBox
        half_span = (GRID_SIZE * SPACING) / 2.0
        f.write(f'  <bounds minlat="{LAT_CENTER - half_span:.7f}" minlon="{LON_CENTER - half_span:.7f}" '
                f'maxlat="{LAT_CENTER + half_span:.7f}" maxlon="{LON_CENTER + half_span:.7f}"/>\n')

        node_id = 1
        way_id = 1000000

        # Генерация 4900 полигонов
        for row in range(GRID_SIZE):
            for col in range(GRID_SIZE):
                # Вычисляем юго-западный угол текущего полигона
                base_lat = (LAT_CENTER - half_span) + (row * SPACING)
                base_lon = (LON_CENTER - half_span) + (col * SPACING)

                # 4 вершины квадрата
                n1 = node_id;     f.write(f'  <node id="{n1}" visible="true" version="1" lat="{base_lat:.7f}" lon="{base_lon:.7f}"/>\n')
                n2 = node_id + 1; f.write(f'  <node id="{n2}" visible="true" version="1" lat="{base_lat + POLY_SIZE:.7f}" lon="{base_lon:.7f}"/>\n')
                n3 = node_id + 2; f.write(f'  <node id="{n3}" visible="true" version="1" lat="{base_lat + POLY_SIZE:.7f}" lon="{base_lon + POLY_SIZE:.7f}"/>\n')
                n4 = node_id + 3; f.write(f'  <node id="{n4}" visible="true" version="1" lat="{base_lat:.7f}" lon="{base_lon + POLY_SIZE:.7f}"/>\n')
                node_id += 4

                # Замкнутый контур
                f.write(f'  <way id="{way_id}" visible="true" version="1">\n')
                f.write(f'    <nd ref="{n1}"/>\n    <nd ref="{n2}"/>\n    <nd ref="{n3}"/>\n    <nd ref="{n4}"/>\n    <nd ref="{n1}"/>\n')
                f.write('    <tag k="natural" v="wood"/>\n')
                # Добавляем имя только некоторым объектам, чтобы усложнить структуру дерева
                if (row + col) % 5 == 0:
                    f.write(f'    <tag k="name" v="Участок {row}-{col}"/>\n')
                f.write('  </way>\n')
                way_id += 1

        f.write('</osm>\n')
        print(f"Файл {filename} сгенерирован (Сетка: {GRID_SIZE}x{GRID_SIZE}, {GRID_SIZE**2} полигонов).")

if __name__ == "__main__":
    generate_large_v3_jump_osm()