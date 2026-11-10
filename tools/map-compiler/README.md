# PurrGO Map Compiler

Builds offline maps for PurrGO from OpenStreetMap (`.osm`) data.

The compiler generates the files used by the PurrGO map renderer:

```text
.idx   spatial index
.mlp   vector geometry
.db    object attributes
map.name
```

The generated map uses **PurrGO Map Format V3**.

For the binary format, see:

```text
docs/purrgo_map_specification_v3.md
```

---

## 1. Install

Requirements:

- Python 3.10+
- packages from `requirements.txt`

Install dependencies:

```bash
pip install -r requirements.txt
```

---

## 2. Get OSM Data

The compiler accepts OpenStreetMap XML (`.osm`).

Obtain an extract for the required area from OpenStreetMap or another OSM data provider.

Prepare the input file as:

```text
map.osm
```

For large areas, `purrgo_osm_optimizer.py` can optionally be used before compilation:

```bash
python purrgo_osm_optimizer.py map.osm map_optimized.osm
```

---

## 3. Compile

`features.csv` is read from the directory of `purrgo_map_compiler.py`. The input file is the optional
argument (default `map.osm`):

```bash
python purrgo_map_compiler.py                  # compiles map.osm
python purrgo_map_compiler.py london.osm       # compiles london.osm
```

The compiler processes the OSM data according to `features.csv` and writes the map files to a folder next to
the script, named after the input file without the extension (`london/`).

Compilation time and RAM usage depend on the size of the OSM extract.

---

## 4. Generated Map

A typical result is:

```text
roads.idx
roads.mlp
roads.db

landuse.idx
landuse.mlp
landuse.db

water.idx
water.mlp
water.db

waterways.idx
waterways.mlp
waterways.db

pois.idx
pois.db

map.name
```

Some `.db` files may be omitted when the corresponding layer contains no named objects.

The `waterways.*` files are written only if the source data contains linear water objects.

Layers:

- `roads` — roads and paths;
- `landuse` — land-use areas;
- `water` — water bodies (polygons);
- `waterways` — rivers, streams and canals (lines);
- `pois` — points of interest.

POIs are stored without `.mlp` geometry.

---

## 5. `features.csv`

`features.csv` controls which OSM objects are included and how they are classified.

Rules are evaluated from top to bottom:

**the first matching enabled rule wins.**

`OSM_Tags` is a comma-separated list of `key=value` pairs that must all match.
The value `*` matches any value of the key, e.g. `barrier=*,access=private`.

The detailed semantics of the feature codes and generated map format are defined in:

```text
docs/purrgo_map_specification_v3.md
```

---

## 6. Install the Map

Create a directory for the map on the PurrGO SD card, inside `PURRGO/MAPS/` (created by the device on the
first start):

```text
PURRGO/MAPS/
└── London/
```

Copy all generated map files into that directory:

```text
PURRGO/MAPS/
└── London/
    ├── map.name
    ├── roads.idx
    ├── roads.mlp
    ├── roads.db
    ├── landuse.idx
    ├── landuse.mlp
    ├── landuse.db
    ├── water.idx
    ├── water.mlp
    ├── water.db
    ├── waterways.idx
    ├── waterways.mlp
    ├── waterways.db
    ├── pois.idx
    └── pois.db
```

Then safely eject the SD card and insert it into PurrGO; the map is selected in the main menu, item `MAP DIR`.

---

## 7. Result

The complete workflow is:

```text
OSM extract
    │
    ▼
map.osm
    │
    ▼
purrgo_map_compiler.py
    │
    ├── features.csv
    │
    ▼
PurrGO map package
    │
    ├── map.name
    ├── *.idx
    ├── *.mlp
    └── *.db
    │
    ▼
SD card
    │
    ▼
PurrGO
```

The compiler is a build-time tool. Map rendering and map navigation are performed by PurrGO firmware.