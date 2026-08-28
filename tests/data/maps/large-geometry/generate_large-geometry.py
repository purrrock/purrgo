import math

# Целевые константы PurrGO Test Suite
LAT_CENTER = 53.713536
LON_CENTER = 28.4193615
POINTS_LINE = 500
POINTS_POLY = 500
POINTS_HOLE = 100

def generate_large_osm(filename="large-geometry.osm"):
    with open(filename, "w", encoding="utf-8") as f:
        f.write('<?xml version="1.0" encoding="UTF-8"?>\n')
        f.write('<osm version="0.6" generator="PurrGO_Test_Suite">\n')
        f.write(f'  <bounds minlat="{LAT_CENTER-0.02}" minlon="{LON_CENTER-0.02}" maxlat="{LAT_CENTER+0.02}" maxlon="{LON_CENTER+0.02}"/>\n')

        node_id = 1
        
        # 1. Генерация вершин для длинной линии (Синусоида)
        line_refs = []
        for i in range(POINTS_LINE):
            # Линия проходит чуть севернее центра, растягиваясь с запада на восток
            lat = LAT_CENTER + 0.01 + 0.002 * math.sin(i * 0.2)
            lon = LON_CENTER - 0.015 + (0.03 * i / POINTS_LINE)
            f.write(f'  <node id="{node_id}" visible="true" version="1" lat="{lat:.7f}" lon="{lon:.7f}"/>\n')
            line_refs.append(node_id)
            node_id += 1

        # 2. Генерация вершин для полигона (Внешнее кольцо - 500 точек)
        outer_refs = []
        radius_outer = 0.008
        for i in range(POINTS_POLY):
            angle = 2 * math.pi * i / POINTS_POLY
            lat = LAT_CENTER + radius_outer * math.cos(angle)
            lon = LON_CENTER + radius_outer * math.sin(angle)
            f.write(f'  <node id="{node_id}" visible="true" version="1" lat="{lat:.7f}" lon="{lon:.7f}"/>\n')
            outer_refs.append(node_id)
            node_id += 1

        # 3. Генерация вершин для дырки в полигоне (Внутреннее кольцо - 100 точек)
        inner_refs = []
        radius_inner = 0.003
        for i in range(POINTS_HOLE):
            angle = 2 * math.pi * i / POINTS_HOLE
            lat = LAT_CENTER + radius_inner * math.cos(angle)
            lon = LON_CENTER + radius_inner * math.sin(angle)
            f.write(f'  <node id="{node_id}" visible="true" version="1" lat="{lat:.7f}" lon="{lon:.7f}"/>\n')
            inner_refs.append(node_id)
            node_id += 1

        # --- Запись геометрии (Ways) ---
        
        # Объект 1: Сложная линия (500 точек)
        f.write(f'\n  <way id="{node_id}" visible="true" version="1">\n')
        for ref in line_refs:
            f.write(f'    <nd ref="{ref}"/>\n')
        f.write('    <tag k="highway" v="primary"/>\n')
        f.write('    <tag k="name" v="Длинный серпантин"/>\n')
        f.write('  </way>\n')
        way_line_id = node_id
        node_id += 1

        # Контур Outer для полигона
        f.write(f'\n  <way id="{node_id}" visible="true" version="1">\n')
        for ref in outer_refs:
            f.write(f'    <nd ref="{ref}"/>\n')
        f.write(f'    <nd ref="{outer_refs[0]}"/>\n') # Замыкаем кольцо
        f.write('  </way>\n')
        way_outer_id = node_id
        node_id += 1

        # Контур Inner (Hole) для полигона
        f.write(f'\n  <way id="{node_id}" visible="true" version="1">\n')
        for ref in inner_refs:
            f.write(f'    <nd ref="{ref}"/>\n')
        f.write(f'    <nd ref="{inner_refs[0]}"/>\n') # Замыкаем кольцо
        f.write('  </way>\n')
        way_inner_id = node_id
        node_id += 1

        # Объект 2: Мультиполигон с дыркой (602 точки в сумме: 500 + 1 (замыкание) + 100 + 1)
        f.write(f'\n  <relation id="{node_id}" visible="true" version="1">\n')
        f.write(f'    <member type="way" ref="{way_outer_id}" role="outer"/>\n')
        f.write(f'    <member type="way" ref="{way_inner_id}" role="inner"/>\n')
        f.write('    <tag k="type" v="multipolygon"/>\n')
        f.write('    <tag k="natural" v="forest"/>\n')
        f.write('    <tag k="name" v="Forest с островом"/>\n')
        f.write('  </relation>\n')

        f.write('</osm>\n')
        print(f"Файл {filename} успешно сгенерирован (Линия: {POINTS_LINE} тчк, Полигон: {POINTS_POLY} + {POINTS_HOLE} тчк).")

if __name__ == "__main__":
    generate_large_osm()