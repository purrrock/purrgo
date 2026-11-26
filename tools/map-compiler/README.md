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
docs/map-format-v3.md
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

## 3.1. Map format V4

By default the compiler writes **format V3**. Format V4 (`docs/map-format-v4.md`) is selected with `--format 4`:

```bash
python purrgo_map_compiler.py belarus.osm --format 4 --search -o out/belarus
```

| Option | Meaning |
| ------ | ------- |
| `--format 3\|4` | map format version (default 3); a package is entirely V3 or entirely V4 |
| `-o`, `--out-dir` | output folder (default: next to the script, named after the input file) |
| `--features` | path to `features.csv` (default: next to the script) |
| `--search` | V4: also write `search.idx` (section 21); subtypes come from `search_categories.csv` |
| `--search-categories` | V4: path to `search_categories.csv` (default: next to the script) |
| `--tol-lod2`, `--tol-lod3` | V4: simplification tolerance of the LOD 2 / LOD 3 geometry variants, metres (default 50 / 250) |

V4 differences in the compiler:

- `.idx` is written in the block layout with four LODs (`purrgo_idx_v4.py`); the `LOD` column of `features.csv` may be `0..3`;
- LOD 2 and LOD 3 objects point to simplified geometry (`purrgo_simplify.py`, Douglas-Peucker in metres);
- names are not shortened to 22 characters (limit: 100 bytes, section 10.6);
- `search.idx` indexes POI and the named objects of the other layers (streets and roads, waterways, water, landuse); objects of one layer with the same name whose anchor points fall into the same 0.05 degree cell are merged into one entry (section 21.5). The `.db` files are not changed by this.
- `search_categories.csv` (`Group;Subtype;Keywords_RU;Keywords_EN;OSM_Tags`) assigns search subtypes to POIs and to named line and polygon objects and defines the category keywords; a keyword is one word of at most 16 bytes. A row with empty `Subtype` and `OSM_Tags` is a group row.

Tests: `python3 -m unittest tests/test_map_compiler_v4.py`.

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

The `Code` of a rule must lie in the range of its `Layer`: `roads` 0..31, `waterways` 32..63, `water` 64..95,
`landuse` 96..127, `pois` 128..191 (192..223 are UI icon numbers, never stored in maps; 224..255 are reserved). A new class takes the next free code at the end of
its range. The compiler stops with an error if a rule uses a code outside the range of its layer.

The detailed semantics of the feature codes and generated map format are defined in:

```text
docs/map-format-v3.md
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

---

## 5. Comparing V3 and V4 (load estimate)

`sim_map_io.py` models the SD-card load of one map frame (sectors read, random vs sequential transfers, projected vertices, time). It detects the map format (V3 or V4) automatically.

```bash
# the same OSM, two formats, two folders (features.csv is shared)
python purrgo_map_compiler.py belarus.osm --format 3 -o out/belarus-v3
python purrgo_map_compiler.py belarus.osm --format 4 -o out/belarus-v4

# one map
python sim_map_io.py out/belarus-v4 53.9 27.56 --scales 100,500,2000,10000,50000,200000 --breakdown

# V3 (A) against V4 (B) on identical frames
python sim_map_io.py out/belarus-v3 53.9 27.56 --compare out/belarus-v4
```

In the `--compare` table `B/A < 1` means V4 is better. Useful keys (they apply to both maps): `--max-lod 2` (V4 selects LOD like V3: isolates the index layout from LOD 3 and geometry simplification), `--line-idx N` (read-ahead for `.idx`; V4 child blocks are consecutive), `--pan 0.3` (second frame after a pan, warm cache), `--cpu 84|160`, `--spi-mhz`.

Read the columns this way: `xfers`/`sect` — SD transfers and sectors (index layout), `idx xf` — transfers for `.idx` only, `verts` — projected vertices (effect of LOD 3 and simplification, mostly at 10 km and above), `total s` — mid estimate of the model. The numbers are a model, not a hardware measurement.

