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

For large areas, `osm_optimizer.py` can optionally be used before compilation:

```bash
python osm_optimizer.py map.osm map_optimized.osm
```

---

## 3. Compile

Place the following in the compiler working directory:

```text
purrgo_map_compiler.py
features.csv
map.osm
```

Run:

```bash
python purrgo_map_compiler.py
```

The compiler processes the OSM data according to `features.csv` and creates the map files.

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

pois.idx
pois.db

map.name
```

Some `.db` files may be omitted when the corresponding layer contains no named objects.

Layers:

- `roads` — roads and paths;
- `landuse` — land-use areas;
- `water` — water bodies;
- `pois` — points of interest.

POIs are stored without `.mlp` geometry.

---

## 5. `features.csv`

`features.csv` controls which OSM objects are included and how they are classified.

Rules are evaluated from top to bottom:

**the first matching enabled rule wins.**

The detailed semantics of the feature codes and generated map format are defined in:

```text
docs/purrgo_map_specification_v3.md
```

---

## 6. Install the Map

Create a directory for the map on the PurrGO SD card:

```text
maps/
└── London/
```

Copy all generated map files into that directory:

```text
maps/
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
    ├── pois.idx
    └── pois.db
```

Then safely eject the SD card and insert it into PurrGO.

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