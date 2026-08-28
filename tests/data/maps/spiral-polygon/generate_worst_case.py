import math

# Требуемый географический центр PurrGO Test Suite
LAT_CENTER = 53.713536
LON_CENTER = 28.4193615

def generate_star_polygon(filename="star-polygon.osm"):
    """Создает 50-лучевую звезду (100 вершин) с экстремальным количеством локальных экстремумов."""
    num_spikes = 50
    total_points = num_spikes * 2
    r_outer = 0.007  # ~770 метров
    r_inner = 0.0025 # ~275 метров

    with open(filename, "w", encoding="utf-8") as f:
        f.write('<?xml version="1.0" encoding="UTF-8"?>\n')
        f.write('<osm version="0.6" generator="PurrGO_Stress_Test">\n')
        f.write(f'  <bounds minlat="{LAT_CENTER - 0.01}" minlon="{LON_CENTER - 0.01}" '
                f'maxlat="{LAT_CENTER + 0.01}" maxlon="{LON_CENTER + 0.01}"/>\n')

        node_id = 1
        nodes = []
        for i in range(total_points):
            angle = 2 * math.pi * i / total_points
            r = r_outer if i % 2 == 0 else r_inner
            lat = LAT_CENTER + r * math.cos(angle)
            lon = LON_CENTER + r * math.sin(angle)
            f.write(f'  <node id="{node_id}" visible="true" version="1" lat="{lat:.7f}" lon="{lon:.7f}"/>\n')
            nodes.append(node_id)
            node_id += 1

        way_id = node_id
        f.write(f'  <way id="{way_id}" visible="true" version="1">\n')
        for ref in nodes:
            f.write(f'    <nd ref="{ref}"/>\n')
        f.write(f'    <nd ref="{nodes[0]}"/>\n') # Замыкаем кольцо
        f.write('    <tag k="natural" v="wood"/>\n')
        f.write('    <tag k="name" v="50-Spoke Star Worst Case"/>\n')
        f.write('  </way>\n')
        f.write('</osm>\n')
    print(f"Сгенерирован файл: {filename} (Звезда на {num_spikes} лучей, {total_points} вершин).")

def generate_spiral_polygon(filename="spiral-polygon.osm"):
    """Создает плотную многовитковую спираль-ленту для стресс-теста Active Edge Table."""
    turns = 6
    points_per_turn = 50
    total_points = turns * points_per_turn

    with open(filename, "w", encoding="utf-8") as f:
        f.write('<?xml version="1.0" encoding="UTF-8"?>\n')
        f.write('<osm version="0.6" generator="PurrGO_Stress_Test">\n')
        f.write(f'  <bounds minlat="{LAT_CENTER - 0.012}" minlon="{LON_CENTER - 0.012}" '
                f'maxlat="{LAT_CENTER + 0.012}" maxlon="{LON_CENTER + 0.012}"/>\n')

        node_id = 1
        inner_nodes = []
        outer_nodes = []

        # Прямой ход спирали к центру
        for i in range(total_points):
            t = i / total_points
            angle = t * turns * 2 * math.pi
            r = 0.009 * (1.0 - 0.85 * t)
            lat = LAT_CENTER + r * math.cos(angle)
            lon = LON_CENTER + r * math.sin(angle)
            f.write(f'  <node id="{node_id}" visible="true" version="1" lat="{lat:.7f}" lon="{lon:.7f}"/>\n')
            inner_nodes.append(node_id)
            node_id += 1

        # Обратный ход спирали с небольшим смещением (формирует толстую замкнутую ленту)
        for i in range(total_points - 1, -1, -1):
            t = i / total_points
            angle = t * turns * 2 * math.pi
            r = 0.009 * (1.0 - 0.85 * t) + 0.0008
            lat = LAT_CENTER + r * math.cos(angle)
            lon = LON_CENTER + r * math.sin(angle)
            f.write(f'  <node id="{node_id}" visible="true" version="1" lat="{lat:.7f}" lon="{lon:.7f}"/>\n')
            outer_nodes.append(node_id)
            node_id += 1

        way_id = node_id
        f.write(f'  <way id="{way_id}" visible="true" version="1">\n')
        for ref in inner_nodes + outer_nodes:
            f.write(f'    <nd ref="{ref}"/>\n')
        f.write(f'    <nd ref="{inner_nodes[0]}"/>\n') # Замыкаем полигон
        f.write('    <tag k="natural" v="wood"/>\n')
        f.write('    <tag k="name" v="Dense Spiral Worst Case"/>\n')
        f.write('  </way>\n')
        f.write('</osm>\n')
    print(f"Сгенерирован файл: {filename} (Спираль: {turns} витков, {total_points * 2} вершин).")

if __name__ == "__main__":
    generate_star_polygon()
    generate_spiral_polygon()