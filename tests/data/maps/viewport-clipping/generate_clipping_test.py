import math

# Целевые константы PurrGO Test Suite
LAT_CENTER = 53.713536
LON_CENTER = 28.4193615
NUM_SPIKES = 24

def generate_clipping_test_osm(filename="viewport-clipping.osm"):
    with open(filename, "w", encoding="utf-8") as f:
        f.write('<?xml version="1.0" encoding="UTF-8"?>\n')
        f.write('<osm version="0.6" generator="PurrGO_Test_Suite">\n')
        
        # BBox с запасом для лучей звезды
        f.write(f'  <bounds minlat="{LAT_CENTER - 0.015}" minlon="{LON_CENTER - 0.015}" '
                f'maxlat="{LAT_CENTER + 0.015}" maxlon="{LON_CENTER + 0.015}"/>\n')

        nodes = []
        node_id = 1
        
        # Радиусы: внутренний (в пределах видимости) и внешний (гарантированно за экраном)
        r_inner = 0.0005  # ~55 метров
        r_outer = 0.01    # ~1.1 километра

        # Генерация 48 точек (24 луча)
        for i in range(NUM_SPIKES * 2):
            angle = i * math.pi / NUM_SPIKES
            # Чередование внутреннего и внешнего радиуса
            r = r_outer if i % 2 == 0 else r_inner
            
            lat = LAT_CENTER + r * math.cos(angle)
            lon = LON_CENTER + r * math.sin(angle)
            
            f.write(f'  <node id="{node_id}" visible="true" version="1" lat="{lat:.7f}" lon="{lon:.7f}"/>\n')
            nodes.append(node_id)
            node_id += 1

        # Запись единого полигона с 48 пересечениями
        f.write(f'\n  <way id="{node_id}" visible="true" version="1">\n')
        for ref in nodes:
            f.write(f'    <nd ref="{ref}"/>\n')
        f.write(f'    <nd ref="{nodes[0]}"/>\n') # Замыкаем кольцо
        f.write('    <tag k="natural" v="wood"/>\n')
        f.write('    <tag k="name" v="Clipping Test Star"/>\n')
        f.write('  </way>\n')

        f.write('</osm>\n')
        print(f"Файл {filename} успешно сгенерирован (Звезда на {NUM_SPIKES} лучей).")

if __name__ == "__main__":
    generate_clipping_test_osm()