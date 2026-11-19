#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
PurrGO Map Compiler
Main orchestrator. Converts OpenStreetMap (XML) data into binary formats
(.mlp, .idx, .db).
"""

import sys
import os
import argparse
import struct

from typing import List

from purrgo_models import MapFeature, HWConfig
from purrgo_osmparser import OSMParser
from purrgo_bin_writer import MapCompiler
from purrgo_lookup import LookupTables


def get_base_directory() -> str:
    """
    Determines the base directory for program execution.
    """
    return os.path.dirname(os.path.abspath(__file__))


def compile_layers_v4(args, out_path, roads_data, landuse_data, water_data,
                      waterways_data, pois_data) -> List[MapFeature]:
    """Writes a PurrGO map format V4 package (docs/map-format-v4.md)."""
    from purrgo_bin_writer_v4 import MapCompilerV4, check_lods

    meta_all: List[MapFeature] = []
    layers = (
        ("roads", roads_data, False),
        ("landuse", landuse_data, True),
        ("water", water_data, True),
        ("waterways", waterways_data, False),
    )
    for name, data, polygons in layers:
        if not data:
            if name == "landuse":
                MapCompilerV4.create_empty_layer_v4(out_path(name))
            continue
        check_lods(data, name)
        MapCompilerV4.build_spatial_order_v4(data)
        variants = MapCompilerV4.build_variants(
            data, polygons, args.tol_lod2, args.tol_lod3)
        var_v1 = MapCompilerV4.compile_mlp_v4(data, out_path(name + ".mlp"), variants)
        MapCompilerV4.compile_db(data, out_path(name + ".db"))
        MapCompilerV4.compile_idx_v4(data, out_path(name + ".idx"), var_v1, variants)
        meta_all.extend(data)

    if pois_data:
        check_lods(pois_data, "pois")
        MapCompilerV4.build_spatial_order_v4(pois_data)
        MapCompilerV4.compile_db(pois_data, out_path("pois.db"), is_poi=True)
        MapCompilerV4.compile_idx_v4(pois_data, out_path("pois.idx"), is_poi=True)
        meta_all.extend(pois_data)
        if args.search:
            MapCompilerV4.compile_search(pois_data, out_path("search.idx"))
    else:
        print("[~] Point objects (POI) are missing in the source data.")
        if args.search:
            print("[~] search.idx skipped: no POI.")
    return meta_all


def main() -> None:
    cli_parser = argparse.ArgumentParser(description="PurrGO Map Compiler")
    # Добавляем опциональный аргумент для файла, по умолчанию map.osm
    cli_parser.add_argument("input_map", nargs="?", default="map.osm", help="Path to the input .osm file (default: map.osm)")
    cli_parser.add_argument("--format", type=int, choices=(3, 4), default=3,
                            help="Map format version (default: 3; 4 = docs/map-format-v4.md)")
    cli_parser.add_argument("-o", "--out-dir", default=None,
                            help="Output folder (default: <script dir>/<input name>)")
    cli_parser.add_argument("--features", default=None,
                            help="Path to features.csv (default: next to the script)")
    cli_parser.add_argument("--search", action="store_true",
                            help="V4: also write search.idx (docs/map-format-v4.md, section 21)")
    cli_parser.add_argument("--search-categories", default=None,
                            help="V4: path to search_categories.csv (default: next to the script)")
    cli_parser.add_argument("--tol-lod2", type=float, default=50.0,
                            help="V4: simplification tolerance for LOD 2, metres (default: 50)")
    cli_parser.add_argument("--tol-lod3", type=float, default=250.0,
                            help="V4: simplification tolerance for LOD 3, metres (default: 250)")
    args = cli_parser.parse_args()
    if args.format == 3 and args.search:
        cli_parser.error("--search needs --format 4")

    # Инициализация путей
    base_dir = get_base_directory()
    
    # Берем путь из аргументов
    map_osm_path = os.path.abspath(args.input_map)

    if not os.path.exists(map_osm_path):
        print(f"[-] Error: {map_osm_path} file not found. Terminating.")
        return

    # Извлекаем имя файла и отрезаем расширение (например, "map" из "map.osm")
    map_filename = os.path.basename(map_osm_path)
    map_name, _ = os.path.splitext(map_filename)

    # Формируем путь для папки результатов
    out_dir = os.path.abspath(args.out_dir) if args.out_dir else os.path.join(base_dir, map_name)
    
    # Создаем папку, если ее нет
    if not os.path.exists(out_dir):
        os.makedirs(out_dir)

    features_csv_path = args.features or os.path.join(base_dir, "features.csv")
    # routes_dir_path = os.path.join(base_dir, "routes") # Если папка routes нужна в будущем, можно тоже перенаправить

    print("=========================================")
    print("PurrGO MAP COMPILER")
    print(f"Base Directory: {base_dir}")
    print(f"Input Map: {map_osm_path}")
    print(f"Output Folder: {out_dir}")
    print(f"Map Name: {map_name}")
    print(f"Format: V{args.format}")
    print("=========================================")

    # 1. Initialize Look-Up Tables
    LookupTables.load_from_csv(features_csv_path)

    if args.format == 4:
        import purrgo_osmparser
        purrgo_osmparser.set_v4_names(True)
        if args.search:
            from purrgo_search import SearchCategories
            SearchCategories.load(args.search_categories
                                  or os.path.join(base_dir, "search_categories.csv"))

    # 2. Parse Source Data
    parser = OSMParser(map_osm_path)
    roads_data, landuse_data, water_data, waterways_data, pois_data = parser.parse()

    # 3. Serialize Layers
    # Обновляем помощник, чтобы он сохранял файлы в новую директорию out_dir
    def out_path(filename: str) -> str:
        return os.path.join(out_dir, filename)

    meta_all: List[MapFeature] = []

    if args.format == 4:
        meta_all = compile_layers_v4(args, out_path, roads_data, landuse_data,
                                     water_data, waterways_data, pois_data)
        MapCompiler.create_map_name(map_name, meta_all, out_path("map.name"))
        print("\n[SUCCESS] Map package (V4) compiled successfully!")
        return

    # 4.1 Roads Layer
    if roads_data:
        MapCompiler.build_spatial_order(roads_data)
        MapCompiler.compile_mlp(roads_data, out_path("roads.mlp"))
        MapCompiler.compile_db(roads_data, out_path("roads.db"))
        MapCompiler.compile_idx(roads_data, out_path("roads.idx"))
        meta_all.extend(roads_data)

    # 3.2 Landuse and Water Layers
    if landuse_data:
        MapCompiler.build_spatial_order(landuse_data)
        MapCompiler.compile_mlp(landuse_data, out_path("landuse.mlp"))
        MapCompiler.compile_db(landuse_data, out_path("landuse.db"))
        MapCompiler.compile_idx(landuse_data, out_path("landuse.idx"))
        meta_all.extend(landuse_data)
    else:
        # Pass the absolute prefix to the empty layer creation method
        MapCompiler.create_empty_layer(out_path("landuse"))

    if water_data:
        MapCompiler.build_spatial_order(water_data)
        MapCompiler.compile_mlp(water_data, out_path("water.mlp"))
        MapCompiler.compile_db(water_data, out_path("water.db"))
        MapCompiler.compile_idx(water_data, out_path("water.idx"))
        meta_all.extend(water_data)

    # 3.2.1 Waterways Layer (linear water objects)
    #
    # Optional layer: the files are written only when the source data has
    # waterway lines. No dummy files are created for an empty layer.
    if waterways_data:
        MapCompiler.build_spatial_order(waterways_data)
        MapCompiler.compile_mlp(waterways_data, out_path("waterways.mlp"))
        MapCompiler.compile_db(waterways_data, out_path("waterways.db"))
        MapCompiler.compile_idx(waterways_data, out_path("waterways.idx"))
        meta_all.extend(waterways_data)

    # 3.3 Native POI Layer
    if pois_data:
        MapCompiler.build_spatial_order(pois_data)
        MapCompiler.compile_db(pois_data, out_path("pois.db"), is_poi=True)
        MapCompiler.compile_idx(pois_data, out_path("pois.idx"), is_poi=True)
        meta_all.extend(pois_data)
    else:
        print("[~] Point objects (POI) are missing in the source data.")

    # 4. Export JSON Metadata
    # Записываем извлеченное имя в map.name вместо старого "PurrGO_Map"
    MapCompiler.create_map_name(map_name, meta_all, out_path("map.name"))

    print("\n[SUCCESS] Map package compiled successfully!")

if __name__ == "__main__":
    main()