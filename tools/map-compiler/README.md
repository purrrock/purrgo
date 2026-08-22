# Maps for PurrGO

The toolset allows building your own highly detailed maps from open sources (e.g., OpenStreetMap). These custom maps are natively hardware-supported and perfectly rendered by the PurrGO engine. 
---

## 🚀 Core Compiler Features

The compiler has been significantly upgraded to bypass native firmware limitations and optimize resource consumption:
* **Country-Sized Map Support (Hierarchical R-Trees):** Generates true STR (Sort-Tile-Recursive) spatial index trees. By utilizing nested Macro-nodes and calculating recursive byte jumps (`v3_jump`), the compiler allows the PurrGO to skip entire regions in a single instruction. This bypasses SRAM limits and ensures butter-smooth panning on maps of any size.
* **Point of Interest (POI) Icon Baking:** Circumvents the hardware graphics pipeline limitation (which natively drops POI rendering) by parametrically "baking" point objects into the landuse layer. Generates low-poly geometric primitives (triangles, squares, hexagons) with automatic display perspective distortion compensation (Y-multiplier = 1.5).
* **Software Culling (Early Exit Parsing):** Highly optimized two-pass XML streaming (`xml.etree.ElementTree.iterparse`). Drops disabled routing nodes immediately during tree traversal via the 11th column (`Enabled`) in the LUT configuration.
* **Dynamic Hardware Overrides (Tag Interception):** Overrides standard LUT routing rules on the fly based on multidimensional OSM tags.
    * *Road Surface Analysis:* Dynamically analyzes `surface` and `smoothness` tags. Automatically downgrades routing classes (e.g., primary roads) to unpaved gray paths if `smoothness=bad` or `surface=dirt`. Preserves original LUT colors for non-vehicle infrastructure (footways, cycleways) via an internal exclusion mask.
    * *Access Restrictions:* Physical barriers with restricted access (`access=private/no/permit`) are intercepted prior to LUT evaluation and forced into pink diagonal crosses.
* **Namespace Collision Isolation:** Blacklist registries are strictly isolated by layer (`pois`, `roads`, `landuse`, `water`) to prevent `fclass` routing conflicts between differently categorized objects.
* **GPX Track Integration:** Natively compiles custom `.gpx` user routes directly into the hardware vector graph.
* **Advanced Key-Value Tag Routing:** Fully parses the `OSM_Tags` column from `features.csv` to resolve namespace collisions. Objects are strictly routed using precise `key=value` hash table lookups (e.g., `shop=bicycle -> bicycle_shop`) before applying fallback heuristics. This ensures all complex GIS classes are compiled without data loss.

---

## 📂 Toolkit Composition

The project has transitioned to a fully **modular architecture** for better maintainability and isolated debugging. The codebase provides 100% binary compatibility with the hardware parser of the PurrGO and includes:

* **`purrgo_map_compiler.py`** — Main CLI Orchestrator. Coordinates the map building process.
* **`purrgo_models.py`** — Data structures and system constants (`MapFeature`, `HWConfig`).
* **`purrgo_osmparser.py`** — Map and route parsing logic (`OSMParser`, `GPXParser`).
* **`purrgo_geometry.py`** — Geometric algorithms and POI baking (`POIGeometryFactory`).
* **`purrgo_bin_writer.py`** — Low-level binary serialization for target files (`MapCompiler`).
* **`purrgo_lookup.py`** — Advanced LUT configuration and tag routing (`LookupTables`).
* **`features.csv`** — Modifiable style routing table (LUT) with software culling (Blacklist) support.
* **`features_factory.csv`** — Original dump of the factory style table.
* **`purrgo_map_specification.md`** — Technical format specification. Contains the byte-by-byte structure of `.mlp`, `.idx`, and `.db` files.

---


## 📥 Download & Usage

1. Ensure you have Python 3.10 or higher installed.
2. Clone the repository and install dependencies: `pip install -r requirements.txt`.
3. Place your exported `map.osm` file in the working directory next to the compiler.
4. Run the script: `python purrgo_map_compiler.py`.
5. Copy the generated files (`roads.mlp`, `roads.idx`, `landuse.db`, `map.name`) to the PurrGO SD card /MAPS/MAPNAME/.

---

## ⚡ Preprocessing Large Maps (Highly Recommended)

If you are compiling large areas, entire countries, or experiencing Out-Of-Memory errors on your PC during compilation, you should preprocess your raw `.osm` file using the `osm_optimizer.py` utility. The PurrGO's hardware has limitations. Rendering excessively long, continuous lines (like major highways) as single objects can cause the PurrGO UI to freeze or trigger a Soft Reset. 

The optimizer solves this by:
1.  **Hardware-Safe Chunking:** Safely slicing extremely long linear routes into smaller segments (e.g., 100 vertices per chunk) while preserving the mathematical topology of closed polygons (lakes, forests) to prevent scanline rendering glitches.
2.  **Aggressive Metadata Stripping:** Removing heavy OSM metadata (timestamps, users, changesets) and dropping globally blacklisted tags (e.g., `power`, `building`, `addr:*`) to drastically reduce the intermediate file size. 

---

## 🎨 Style Customization and Object Filtering

The `features.csv` file (Look-Up Table) is the main configuration file of the compiler. It is loaded dynamically upon each launch.

### LUT Table Structure (11 columns)

The configuration consists of 11 columns separated by a semicolon (`;`).
**Header format:**
```text
Code;fclass;Color;LOD;Layer;OSM_Tags;Description;Remap_Code;Remap_Color;Remap_LOD;Enabled;;Shape
```

**Remapping (aliasing) parameters are of particular importance:**
* **Remap_Code:** The system 32-bit ID into which the object will be forcibly converted. 
    * *Example:* Paved roads are mapped to the yellow color ID 5113.
* **Remap_LOD:** The hardware Z-Culling hide distance (in meters) at which the object will appear on the screen when zooming.

### Software Culling (Blacklist)
To protect the PurrGO's graphics pipeline from RAM overflow and `.idx` binary graph bloating, a software culling system is implemented during the stream parsing stage. The 11th column of the configuration — `Enabled` — is responsible for filtering.

* `1` (or `true`) — The object is loaded into the compiler and participates in map generation.
* `0` (or `false`) — Muted class. Hardware-culled. The algorithm utilizes an Early Exit parsing interrupt: upon encountering a tag with the `Enabled=0` value, the parser immediately discards the XML node prior to calculating the Bounding Box. This saves CPU time and prevents replacing excluded objects with default gray or green styles.
