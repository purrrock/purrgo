import math

# Целевые константы PurrGO Test Suite
LAT_CENTER = 53.713536
LON_CENTER = 28.4193615
SPACING = 0.002  # Шаг сетки (~220 метров между объектами)

RAW_TAGS = [
    "highway=motorway", "highway=trunk", "highway=primary", "highway=secondary", "highway=tertiary",
    "highway=unclassified", "highway=residential", "highway=living_street", "highway=pedestrian",
    "highway=busway", "highway=motorway_link", "highway=trunk_link", "highway=primary_link",
    "highway=secondary_link", "highway=tertiary_link", "highway=service", "highway=track",
    "tracktype=grade1", "tracktype=grade2", "tracktype=grade3", "tracktype=grade4", "tracktype=grade5",
    "highway=bridleway", "highway=cycleway", "highway=footway", "highway=path", "highway=steps",
    "highway=road", "railway=rail", "railway=construction", "railway=disused", "railway=funicular",
    "railway=light_rail", "railway=miniature", "railway=monorail", "railway=narrow_gauge",
    "landuse=forest", "natural=wood", "leisure=park", "leisure=common", "landuse=residential",
    "landuse=industrial", "landuse=cemetery", "landuse=allotments", "landuse=meadow",
    "landuse=commercial", "leisure=nature_reserve", "landuse=recreation_ground", "landuse=retail",
    "landuse=military", "landuse=quarry", "landuse=orchard", "landuse=vineyard", "landuse=scrub",
    "landuse=grass", "natural=heath", "landuse=farmland", "landuse=farmyard", "landuse=landfill",
    "natural=water", "landuse=reservoir", "waterway=riverbank", "water=river", "waterway=dock",
    "natural=glacier", "natural=wetland", "place=city, capital=yes", "place=city", "place=town",
    "place=village", "place=hamlet", "place=suburb", "place=island", "place=farm",
    "place=isolated_dwelling", "place=region", "place=county", "place=locality", "amenity=police",
    "amenity=fire_station", "amenity=marketplace", "amenity=pharmacy", "amenity=hospital",
    "amenity=clinic", "amenity=doctors", "leisure=park", "amenity=restaurant", "amenity=fast_food",
    "amenity=cafe", "amenity=pub", "amenity=bar", "amenity=food_court", "amenity=biergarten",
    "tourism=hotel", "tourism=motel", "tourism=bed_and_breakfast", "tourism=guest_house",
    "tourism=hostel", "tourism=chalet", "shelter_type=lean_to", "shelter_type=picnic_shelter",
    "shelter_type=basic_hut", "shelter_type=weather_shelter", "amenity=shelter", "tourism=camp_site",
    "tourism=alpine_hut", "tourism=wilderness_hut", "tourism=caravan_site", "shop=supermarket",
    "shop=bakery", "shop=kiosk", "shop=mall", "shop=department_store", "shop=general",
    "shop=convenience", "shop=clothes", "shop=florist", "shop=chemist", "shop=books", "shop=butcher",
    "shop=shoes", "shop=beverages", "shop=alcohol", "shop=optician", "shop=jewelry", "shop=gift",
    "shop=sports", "shop=stationery", "shop=outdoor", "shop=mobile_phone", "shop=toys",
    "shop=newsagent", "shop=greengrocer", "shop=beauty", "shop=video", "shop=car", "shop=bicycle",
    "shop=doityourself", "shop=hardware", "shop=furniture", "shop=computer", "shop=garden_centre",
    "shop=hairdresser", "shop=car_repair", "amenity=car_rental", "amenity=car_wash",
    "amenity=car_sharing", "amenity=bicycle_rental", "shop=travel_agency", "shop=laundry",
    "shop=dry_cleaning", "amenity=vending_machine", "amenity=bank", "amenity=atm",
    "tourism=information", "tourism=attraction", "tourism=museum", "historic=monument",
    "historic=memorial", "tourism=artwork", "historic=castle", "historic=ruins",
    "historic=archaeological_site", "historic=wayside_cross", "historic=wayside_shrine",
    "historic=battlefield", "historic=fort", "tourism=picnic_site", "tourism=viewpoint",
    "tourism=zoo", "tourism=theme_park", "amenity=toilets", "amenity=bench", "amenity=drinking_water",
    "man_made=lighthouse", "man_made=water_well", "man_made=watermill", "amenity=place_of_worship",
    "natural=spring", "natural=glacier", "natural=peak", "natural=cliff", "natural=volcano",
    "natural=cave_entrance", "natural=beach", "highway=ford", "amenity=fuel", "highway=services",
    "amenity=parking", "amenity=bicycle_parking", "amenity=bicycle_repair_station",
    "waterway=waterfall", "railway=station", "railway=halt", "railway=tram_stop", "highway=bus_stop",
    "amenity=bus_station", "amenity=taxi", "amenity=airport", "aeroway=aerodrome", "aeroway=airfield",
    "aeroway=helipad", "aeroway=apron", "amenity=ferry_terminal", "aerialway=station",
    "amenity=shower", "natural=saddle", "natural=hot_spring", "highway=rest_area"
]

def parse_tags(raw_str):
    tags = {}
    
    # Обработка зависимых тегов
    if "tracktype=" in raw_str:
        tags["highway"] = "track"
    if "shelter_type=" in raw_str:
        tags["amenity"] = "shelter"
        
    parts = raw_str.split(',')
    for p in parts:
        k, v = p.split('=')
        tags[k.strip()] = v.strip()
        
    # Имя на английском по запросу (заменяем = и , на пробелы/дефисы)
    tags["name"] = raw_str.replace("=", "_").replace(", ", "__")
    return tags

def get_geom_type(tags):
    if any(k in tags for k in ["highway", "railway", "waterway", "aerialway"]):
        return "line"
    if any(k in tags for k in ["landuse", "leisure", "natural", "water"]):
        if "natural" in tags and tags["natural"] in ["peak", "spring", "volcano", "saddle", "cave_entrance"]:
            return "node" # Специфичные природные объекты - точки
        return "polygon"
    if any(k in tags for k in ["aeroway"]):
        if tags.get("aeroway") in ["helipad"]:
            return "node"
        return "polygon"
    return "node" # amenity, shop, place, tourism, historic, man_made

def generate_all_features_osm(filename="all-features.osm"):
    cols = 15
    
    with open(filename, "w", encoding="utf-8") as f:
        f.write('<?xml version="1.0" encoding="UTF-8"?>\n')
        f.write('<osm version="0.6" generator="PurrGO_Test_Suite">\n')
        f.write(f'  <bounds minlat="{LAT_CENTER - 0.05}" minlon="{LON_CENTER - 0.05}" '
                f'maxlat="{LAT_CENTER + 0.05}" maxlon="{LON_CENTER + 0.05}"/>\n')

        node_id = 1
        way_id = 1000000

        for i, raw_str in enumerate(RAW_TAGS):
            row = i // cols
            col = i % cols
            
            # Базовая координата ячейки
            base_lat = LAT_CENTER + (row * SPACING) - (len(RAW_TAGS)//cols * SPACING / 2)
            base_lon = LON_CENTER + (col * SPACING) - (cols * SPACING / 2)
            
            tags = parse_tags(raw_str)
            geom_type = get_geom_type(tags)
            
            if geom_type == "node":
                f.write(f'  <node id="{node_id}" visible="true" version="1" lat="{base_lat:.7f}" lon="{base_lon:.7f}">\n')
                for k, v in tags.items():
                    f.write(f'    <tag k="{k}" v="{v}"/>\n')
                f.write('  </node>\n')
                node_id += 1
                
            elif geom_type == "line":
                n1 = node_id;     f.write(f'  <node id="{n1}" visible="true" version="1" lat="{base_lat:.7f}" lon="{base_lon:.7f}"/>\n')
                n2 = node_id + 1; f.write(f'  <node id="{n2}" visible="true" version="1" lat="{base_lat + 0.0005:.7f}" lon="{base_lon + 0.0005:.7f}"/>\n')
                node_id += 2
                
                f.write(f'  <way id="{way_id}" visible="true" version="1">\n')
                f.write(f'    <nd ref="{n1}"/>\n    <nd ref="{n2}"/>\n')
                for k, v in tags.items():
                    f.write(f'    <tag k="{k}" v="{v}"/>\n')
                f.write('  </way>\n')
                way_id += 1
                
            elif geom_type == "polygon":
                n1 = node_id;     f.write(f'  <node id="{n1}" visible="true" version="1" lat="{base_lat:.7f}" lon="{base_lon:.7f}"/>\n')
                n2 = node_id + 1; f.write(f'  <node id="{n2}" visible="true" version="1" lat="{base_lat + 0.0008:.7f}" lon="{base_lon:.7f}"/>\n')
                n3 = node_id + 2; f.write(f'  <node id="{n3}" visible="true" version="1" lat="{base_lat + 0.0008:.7f}" lon="{base_lon + 0.0008:.7f}"/>\n')
                n4 = node_id + 3; f.write(f'  <node id="{n4}" visible="true" version="1" lat="{base_lat:.7f}" lon="{base_lon + 0.0008:.7f}"/>\n')
                node_id += 4
                
                f.write(f'  <way id="{way_id}" visible="true" version="1">\n')
                f.write(f'    <nd ref="{n1}"/>\n    <nd ref="{n2}"/>\n    <nd ref="{n3}"/>\n    <nd ref="{n4}"/>\n    <nd ref="{n1}"/>\n')
                for k, v in tags.items():
                    f.write(f'    <tag k="{k}" v="{v}"/>\n')
                f.write('  </way>\n')
                way_id += 1

        f.write('</osm>\n')
        print(f"Сгенерирован файл {filename} с {len(RAW_TAGS)} уникальными объектами.")

if __name__ == "__main__":
    generate_all_features_osm()