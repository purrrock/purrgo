#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
PurrGO Map Compiler (Platform ATS3085S)
===============================================
v1.5.0 (Fully Modular Architecture)
Main orchestrator. Converts OpenStreetMap (XML) data into closed binary formats
of ATS3085S smartwatches (.mlp, .idx, .db).
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
    cli_parser = argparse.ArgumentParser(description="PurrGO Map Compiler (Platform ATS3085S)")
    args = cli_parser.parse_args()

    # Initialize hardware-independent paths
    base_dir = get_base_directory()
    map_osm_path = os.path.join(base_dir, "map.osm")
    features_csv_path = os.path.join(base_dir, "features.csv")
    routes_dir_path = os.path.join(base_dir, "routes")

    if not os.path.exists(map_osm_path):
        print(f"[-] Error: {map_osm_path} file not found. Terminating.")
        return

    print("=========================================")
    print("PurrGO MAP COMPILER")
    print(f"Base Directory: {base_dir}")
    print("=========================================")

    # 1. Initialize Look-Up Tables
    LookupTables.load_from_csv(features_csv_path)

    # 2. Parse Source Data
    parser = OSMParser(map_osm_path)
    roads_data, landuse_data, pois_data = parser.parse()

    # 3. Serialize Layers
    # Helper to route output binary files to the base directory
    def out_path(filename: str) -> str:
        return os.path.join(base_dir, filename)

    meta_all: List[MapFeature] = []

    # 4.1 Roads Layer
    if roads_data:
        MapCompiler.compile_mlp(roads_data, out_path("roads.mlp"))
        MapCompiler.compile_db(roads_data, out_path("roads.db"))
        MapCompiler.compile_idx(roads_data, out_path("roads.idx"))
        meta_all.extend(roads_data)

    # 3.2 Landuse and Water Layers
    landuse_only = [f for f in landuse_data if f.code != HWConfig.WATER_CODE]
    water_only = [f for f in landuse_data if f.code == HWConfig.WATER_CODE]

    if landuse_only:
        MapCompiler.compile_mlp(landuse_only, out_path("landuse.mlp"))
        MapCompiler.compile_db(landuse_only, out_path("landuse.db"))
        MapCompiler.compile_idx(landuse_only, out_path("landuse.idx"))
        meta_all.extend(landuse_only)
    else:
        # Pass the absolute prefix to the empty layer creation method
        MapCompiler.create_empty_layer(out_path("landuse"))

    if water_only:
        MapCompiler.compile_mlp(water_only, out_path("water.mlp"))
        MapCompiler.compile_db(water_only, out_path("water.db"))
        MapCompiler.compile_idx(water_only, out_path("water.idx"))
        meta_all.extend(water_only)

    # 3.3 Native POI Layer
    if pois_data:
        MapCompiler.compile_db(pois_data, out_path("pois.db"), is_poi=True)
        MapCompiler.compile_idx(pois_data, out_path("pois.idx"), is_poi=True)
        meta_all.extend(pois_data)
    else:
        print("[~] Point objects (POI) are missing in the source data.")

    # 4. Export JSON Metadata
    if meta_all:
        MapCompiler.create_map_name("PurrGO_Map", meta_all, out_path("map.name"))

    print("\n[SUCCESS] Map package compiled successfully!")

if __name__ == "__main__":
    main()
