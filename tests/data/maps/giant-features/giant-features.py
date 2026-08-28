import math

# Целевые константы PurrGO Test Suite
LAT_CENTER = 53.713536
LON_CENTER = 28.4193615

def generate_giant_objects_osm(filename="giant-features.osm"):
    with open(filename, "w", encoding="utf-8") as f:
        f.write('<?xml version="1.0" encoding="UTF-8"?>\n')
        f.write('<osm version="0.6" generator="PurrGO_Test_Suite">\n')
        
        # Общий BBox с запасом для огромных объектов
        f.write(f'  <bounds minlat="{LAT_CENTER - 45.0}" minlon="{LON_CENTER - 45.0}" '
                f'maxlat="{LAT_CENTER + 45.0}" maxlon="{LON_CENTER + 45.0}"/>\n')

        node_id = 1
        
        # --- 1. Автомагистраль длиной ~15 000 км ---
        # 1 градус широты ~= 111 км. 15000 км / 111 ~= 135 градусов. 
        # Проложим длинную дугу с запада на восток через центр.
        highway_nodes = []
        num_hw_points = 10  # Малое количество узлов для тестирования chunking длинных сегментов
        
        for i in range(num_hw_points):
            # Растягиваем по долготе на ±60 градусов (~13300-15000 км по дуге)
            lon_offset = -60.0 + (120.0 * i / (num_hw_points - 1))
            lat = LAT_CENTER + 5.0 * math.sin(i * math.pi / (num_hw_points - 1))
            lon = LON_CENTER + lon_offset
            
            f.write(f'  <node id="{node_id}" visible="true" version="1" lat="{lat:.6f}" lon="{lon:.6f}"/>\n')
            highway_nodes.append(node_id)
            node_id += 1

        # --- 2. Лес размером ~700 км ---
        # 700 км / 111 ~= 6.3 градуса в поперечнике
        forest_nodes = []
        num_fo_points = 8   # Малое количество точек для полигона
        forest_radius = 3.2 # ~350 км радиус
        
        for i in range(num_fo_points):
            angle = 2 * math.pi * i / num_fo_points
            lat = LAT_CENTER + forest_radius * math.cos(angle)
            lon = LON_CENTER + forest_radius * math.sin(angle)
            
            f.write(f'  <node id="{node_id}" visible="true" version="1" lat="{lat:.6f}" lon="{lon:.6f}"/>\n')
            forest_nodes.append(node_id)
            node_id += 1

        # Запись геометрии линии (Автомагистраль)
        way_hw_id = node_id
        node_id += 1
        f.write(f'\n  <way id="{way_hw_id}" visible="true" version="1">\n')
        for ref in highway_nodes:
            f.write(f'    <nd ref="{ref}"/>\n')
        f.write('    <tag k="highway" v="motorway"/>\n')
        f.write('    <tag k="name" v="Transcontinental Motorway"/>\n')
        f.write('  </way>\n')

        # Запись геометрии полигона (Лес)
        way_fo_id = node_id
        node_id += 1
        f.write(f'\n  <way id="{way_fo_id}" visible="true" version="1">\n')
        for ref in forest_nodes:
            f.write(f'    <nd ref="{ref}"/>\n')
        f.write(f'    <nd ref="{forest_nodes[0]}"/>\n') # Замыкаем кольцо
        f.write('    <tag k="natural" v="wood"/>\n')
        f.write('    <tag k="name" v="Mega Forest"/>\n')
        f.write('  </way>\n')

        f.write('</osm>\n')
        print(f"Файл {filename} успешно сгенерирован.")

if __name__ == "__main__":
    generate_giant_objects_osm()