# PurrGO Map Compiler

Python tools for compiling OpenStreetMap (`.osm`) data into the binary map format used by PurrGO.

The compiler generates the `.idx`, `.mlp`, and `.db` and map.name files required by the PurrGO map renderer.

---

## Requirements

- Python 3.10 or newer
- Dependencies from `requirements.txt`

Install dependencies:

```bash
pip install -r requirements.txt
````

---

## Getting OSM Data

The compiler uses OpenStreetMap XML (`.osm`) as its input.

You can obtain an `.osm` file from:

* OpenStreetMap Export / Overpass API:
  [https://www.openstreetmap.org](https://www.openstreetmap.org)
* BBBike extracts:
  [https://extract.bbbike.org/](https://extract.bbbike.org/)

For the current compiler workflow, prepare the source file as:

```text
map.osm
```

---

## Compiling a Map

Place the compiler files, `features.csv`, and `map.osm` in the working directory.

Run:

```bash
python purrgo_map_compiler.py
```

The compiler processes the OSM data and generates the map layers.

Depending on the size and complexity of the source data, compilation can take from several seconds to several minutes.

---

## Generated Files

The compiler generates the following map layers:

```text
roads.mlp
roads.idx
roads.db

landuse.mlp
landuse.idx
landuse.db

water.mlp
water.idx
water.db

pois.idx
pois.db

map.name
```

Not every `.db` file is necessarily generated. If a layer contains no named objects, its attribute database may be omitted according to the map format specification.

### Map layers

* **roads** — roads and paths.
* **landuse** — land-use polygons such as forests, parks, and residential areas.
* **water** — rivers, lakes, and other water bodies.
* **pois** — native POI layer.

POIs are stored without .mlp.

---

## Installing a Map

1. Connect the PurrGO SD card to the PC.
2. Find or create the maps directory.
3. Create a directory for the new map, for example:

```text
maps/
└── London/
```

4. Copy the generated map files into that directory.
5. Safely eject the SD card.
6. Insert the card into PurrGO and select the map.

---

## Compiler Configuration

### `features.csv`

`features.csv` is the main map style and filtering configuration.

It defines how OSM objects are classified and which objects are included in the generated map.

The current table contains the following fields:

```text
Code;fclass;Color;LOD;Layer;OSM_Tags;Description;Remap_Code;Remap_Color;Remap_LOD;Enabled;;Shape
```

Important fields include:

### `Code`

The object class code written into the generated map.

### `fclass`

The logical feature class used by the compiler.

### `LOD`

The default Level of Detail assigned to the feature.

### `Layer`

The target map layer:

```text
roads
landuse
water
pois
```

### `OSM_Tags`

OSM tag matching rules used to classify objects.

For example:

```text
shop=bicycle
```

can be used to identify bicycle shops.

### `Remap_Code`

Allows an object to be converted to another map class.

### `Remap_Color`

Overrides the default style color.

### `Remap_LOD`

Overrides the Level of Detail assigned to the object.

### `Enabled`

Controls whether a feature is compiled.

```text
1 / true  → enabled
0 / false → disabled
```

Disabled features are discarded during OSM parsing and are not included in the generated map.

---

## Large OSM Files

The compiler supports large map areas through hierarchical spatial indexing.

The generated `.idx` files use the existing PurrGO map format with:

* LOD 0
* LOD 1
* LOD 2
* STR spatial indexing
* hierarchical R-tree nodes
* bounding-box culling

Very large OSM files can still require significant amounts of RAM during compilation.

The repository also contains `osm_optimizer.py`, which can be used as an optional preprocessing step for particularly large datasets.

Example:

```bash
python osm_optimizer.py map.osm map_optimized.osm
```

Then use the resulting file as the compiler input.

The optimizer is not required for normal map compilation.

---

## Toolkit Structure

The compiler consists of several modules:

```text
purrgo_map_compiler.py
purrgo_models.py
purrgo_osmparser.py
purrgo_bin_writer.py
purrgo_lookup.py
features.csv
```

Additional utilities and documentation are located in the same directory.

### `purrgo_map_compiler.py`

Main compiler entry point. Coordinates the complete map generation process.

### `purrgo_models.py`

Internal data structures used by the compiler, including map features and spatial-index structures.

### `purrgo_osmparser.py`

Parses OSM XML and converts OSM objects into internal map features.

### `purrgo_bin_writer.py`

Writes the binary `.idx`, `.mlp`, and `.db` files.

### `purrgo_lookup.py`

Contains the feature classification and OSM tag lookup logic.

### `features.csv`

Configurable feature/style lookup table.

### `docs/`

Contains the technical specification of the current binary map format.

---

## Map Format

The compiler currently generates the PurrGO V3 map format. The format consists of three primary file types:

### `.idx`

Spatial index containing LOD sections and hierarchical spatial nodes.

### `.mlp`

Vector geometry containing the coordinates of linear and polygonal objects.

### `.db`

dBase III-compatible attribute database containing object names and related metadata.

See:

```text
docs/purrgo_map_specification_v3.md
docs/New_Global_Header.md
```

for the detailed binary format specification.

---

## Important Limitations

The compiler targets the PurrGO V3 map format.

In particular:

* LOD 0/1/2 are generated;
* the compiler uses the coordinate representation defined by the format;
* map rendering behaviour is determined by the PurrGO firmware.

The compiler should therefore be considered a build-time component of the PurrGO project rather than a general-purpose OSM conversion tool.