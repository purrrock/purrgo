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


def main() -> None:
    cli_parser = argparse.ArgumentParser(description="PurrGO Map Compiler")
    # Добавляем опциональный аргумент для файла, по умолчанию map.osm
    cli_parser.add_argument("input_map", nargs="?", default="map.osm", help="Path to the input .osm file (default: map.osm)")
    args = cli_parser.parse_args()

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
    out_dir = os.path.join(base_dir, map_name)
    
    # Создаем папку, если ее нет
    if not os.path.exists(out_dir):
        os.makedirs(out_dir)

    features_csv_path = os.path.join(base_dir, "features.csv")
    # routes_dir_path = os.path.join(base_dir, "routes") # Если папка routes нужна в будущем, можно тоже перенаправить

    print("=========================================")
    print("PurrGO MAP COMPILER")
    print(f"Base Directory: {base_dir}")
    print(f"Input Map: {map_osm_path}")
    print(f"Output Folder: {out_dir}")
    print(f"Map Name: {map_name}")
    print("=========================================")

    # 1. Initialize Look-Up Tables
    LookupTables.load_from_csv(features_csv_path)

    # 2. Parse Source Data
    parser = OSMParser(map_osm_path)
    roads_data, landuse_data, water_data, pois_data = parser.parse()

    # 3. Serialize Layers
    # Обновляем помощник, чтобы он сохранял файлы в новую директорию out_dir
    def out_path(filename: str) -> str:
        return os.path.join(out_dir, filename)

    meta_all: List[MapFeature] = []

    # 4.1 Roads Layer
    if roads_data:
        MapCompiler.compile_mlp(roads_data, out_path("roads.mlp"))
        MapCompiler.compile_db(roads_data, out_path("roads.db"))
        MapCompiler.compile_idx(roads_data, out_path("roads.idx"))
        meta_all.extend(roads_data)

    # 3.2 Landuse and Water Layers
    if landuse_data:
        MapCompiler.compile_mlp(landuse_data, out_path("landuse.mlp"))
        MapCompiler.compile_db(landuse_data, out_path("landuse.db"))
        MapCompiler.compile_idx(landuse_data, out_path("landuse.idx"))
        meta_all.extend(landuse_data)
    else:
        # Pass the absolute prefix to the empty layer creation method
        MapCompiler.create_empty_layer(out_path("landuse"))

    if water_data:
        MapCompiler.compile_mlp(water_data, out_path("water.mlp"))
        MapCompiler.compile_db(water_data, out_path("water.db"))
        MapCompiler.compile_idx(water_data, out_path("water.idx"))
        meta_all.extend(water_data)

    # 3.3 Native POI Layer
    if pois_data:
        MapCompiler.compile_db(pois_data, out_path("pois.db"), is_poi=True)
        MapCompiler.compile_idx(pois_data, out_path("pois.idx"), is_poi=True)
        meta_all.extend(pois_data)
    else:
        print("[~] Point objects (POI) are missing in the source data.")

    # 4. Export JSON Metadata
    # Записываем извлеченное имя в map.name вместо старого "PurrGO_Map"
    if meta_all:
        MapCompiler.create_map_name(map_name, meta_all, out_path("map.name"))

    print("\n[SUCCESS] Map package compiled successfully!")

if __name__ == "__main__":
    main()