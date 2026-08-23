# PurrGO MAP FORMAT TECHNICAL SPECIFICATION — V2

---

## 1. GLOBAL LAYER HEADER (YZL HEADER)

Each binary map layer file (`.idx`, `.mlp`, `.db`) must begin with a global system header with a fixed length of **32 bytes**.

The header is used by the firmware bootloader for memory mapping and integrity checking via the MD5 algorithm.

### 1.1. YZL Header Byte Structure

| Offset | Size | Data Type  | Field Description           | Value / Format                                                                                                                |
| :----- | :--- | :--------- | :-------------------------- | :---------------------------------------------------------------------------------------------------------------------------- |
| `0x00` | 3    | `char[3]`  | **Magic Signature (Magic)** | Strictly `b'YZL'` (`59 5A 4C`)                                                                                                |
| `0x03` | 1    | `uint8`    | **File Magic Extension**    | For `.mlp` and `.db` = `0x00`. For `.idx` variable (`0x08`, `0x10`, `0x0C`, etc.)                                             |
| `0x04` | 4    | `uint32`   | **Payload Size**            | In bytes (Little-Endian). Formula: `File Size - 32`                                                                           |
| `0x08` | 4    | `uint32`   | **RAM Load Type**           | For `.mlp`/`.db`/`pois.idx` = `0x04000000` (LE: `b'\x00\x00\x00\x04'`). For `.idx` = `0x04000002` (LE: `b'\x02\x00\x00\x04'`) |
| `0x0C` | 4    | `uint32`   | **LOD 2 Section Size**      | In bytes (Big-Endian). Used for fast pointer calculation from EOF. For `.mlp` and `.db` = `0x00000000`                        |
| `0x10` | 16   | `byte[16]` | **MD5 Checksum**            | Computed from the payload body (`0x20` to EOF)                                                                                |

> **Exception for the POI layer (`pois.idx`):**
>
> * **Offset `0x03` (Magic Extension):** `0x00`.
> * **Offset `0x08` (RAM Load Type):** `0x04000000`, identical to `.mlp` and `.db`.

---

# 2. GEOMETRY FILE STRUCTURE (`.MLP`)

The `.mlp` file contains raw ordered coordinates of vertices for linear and polygonal objects.

Coordinates are represented using **signed 32-bit integers with a fixed precision of `10⁻⁷` degrees**.

No floating-point coordinates are stored in `.mlp`.

### 2.1. Coordinate Representation

A geographic coordinate is converted as:

```text
integer_coordinate = geographic_coordinate × 10⁷
```

For example:

```text
55.7558000° → 557558000
37.6173000° → 376173000
```

To convert an integer back to degrees:

```text
geographic_coordinate = integer_coordinate / 10000000.0
```

The coordinate order is:

```text
X = longitude
Y = latitude
```

### 2.2. Local Geometry Header

Immediately following the 32-byte global YZL header is an array of geometry records.

Each record is prefaced by its own 8-byte mini-header.

| Offset | Size | Data Type | Endianness               | Field Description                                |
| :----- | :--- | :-------- | :----------------------- | :----------------------------------------------- |
| `0x00` | 4    | `uint32`  | **Big-Endian** (`>I`)    | Geometry record sequence number, starting from 1 |
| `0x04` | 4    | `uint32`  | **Little-Endian** (`<I`) | Geometry record body length in bytes             |

### 2.3. Geometry Record Body

The record body size is strictly equal to the `Content Length` field.

| Offset                 | Size             | Data Type    | Field Description                                   |
| :--------------------- | :--------------- | :----------- | :-------------------------------------------------- |
| `0x00`                 | 16               | `int32[4]`   | `bbox_int = [minx, miny, maxx, maxy]`               |
| `0x10`                 | 4                | `int32`      | `num_parts` — number of segments/rings              |
| `0x14`                 | 4                | `int32`      | `num_points` — total number of points               |
| `0x18`                 | `num_parts * 4`  | `uint32[]`   | `parts` array; point indices where each part begins |
| `0x18 + num_parts * 4` | `num_points * 8` | `int32[2][]` | Points as `[X, Y]` pairs                            |

All coordinate values in `bbox_int` and `points` use the `10⁻⁷` degree representation.

`parts` contains **indices, not contour lengths**.

---

## 2.4. Polygon Topology (Multipolygons)

Complex polygon structures are stored in the same unified geometry representation.

* All points of all rings are flattened into a single `points` array.
* `num_parts` specifies the number of rings.
* `parts` contains the start index of each ring.
* Rings must be closed: the first and last coordinate must be identical.
* A single closed polygon consists of one part.
* Outer and inner rings use different winding directions.

### Winding Rules

* **Outer contours:** strictly **Clockwise (CW)**.
* **Inner contours / holes:** strictly **Counter-Clockwise (CCW)**.

---

# 3. SPATIAL INDEX STRUCTURE (`.IDX`)

The `.idx` file organizes map objects by Levels of Detail (LOD) and clusters them for fast BBox culling.

PurrGO currently defines three LOD levels:

```text
LOD 0
LOD 1
LOD 2
```

The spatial index uses a hierarchical R-tree structure based on Sort-Tile-Recursive (STR) bulk loading.

### 3.1. Unified 28-Byte Node

Both Navigation Nodes and Data Nodes occupy exactly **28 bytes**.

The last 8 bytes have a fixed interpretation depending on node type and allow the parser to traverse the tree.

```c
struct UnifiedNode {
    /*
     * First 20 bytes have different meanings for Data and Nav nodes.
     */
    uint32_t payload[5];

    uint32_t v1;  // offset 20
    uint32_t v2;  // offset 24
};
```

Coordinates in all nodes are stored as signed 32-bit integers using the `10⁻⁷` degree coordinate representation.

---

## 3.2. Data Node

A Data Node represents one cartographic feature.

Binary unpacking format:

```text
<iiiiIII
```

| Offset        | Size | Type       | Description                                   |
| :------------ | :--- | :--------- | :-------------------------------------------- |
| `0x00 - 0x0F` | 16   | `int32[4]` | BBox `[xmin, ymin, xmax, ymax]`               |
| `0x10 - 0x13` | 4    | `uint32`   | **Type** — PurrGO feature code                |
| `0x14 - 0x17` | 4    | `uint32`   | **v1** — pointer to geometry record in `.mlp` |
| `0x18 - 0x1B` | 4    | `uint32`   | **v2** — record index in `.db`                |

### 3.2.1. Feature Code

The `Type` field is the numeric `Code` from `features.csv`.

The compiler converts OSM objects into PurrGO feature definitions before writing the binary map.

The firmware does **not** need to know the original OSM tag.

For example:

```text
features.csv:

2;ROAD_NORMAL;DARK_GRAY_SEMITHICK_LINE;1;roads;highway=primary;Главная дорога;1;
```

A resulting Data Node contains:

```text
Type = 2
```

The semantic interpretation is therefore:

```text
Code 2 → ROAD_NORMAL
        → DARK_GRAY_SEMITHICK_LINE
        → LOD 1
        → roads
```

The mapping between `Code`, `PG_class`, `STYLE`, `LOD` and other feature metadata is defined by the compiler's `features.csv`.

---

# 4. FEATURE CLASSIFICATION (`features.csv`)

`features.csv` is the source definition used by the PurrGO map compiler.

It replaces the previous `features_dtg1.csv` classification/remapping system.

The new format is:

```text
Code;PG_class;STYLE;LOD;Layer;OSM_Tags;Description;Enabled;Icon
```

## 4.1. CSV Fields

| Field         | Type       | Description                                                  |
| :------------ | :--------- | :----------------------------------------------------------- |
| `Code`        | integer    | Numeric PurrGO feature code stored in `.idx` Data Nodes      |
| `PG_class`    | identifier | Semantic PurrGO feature class                                |
| `STYLE`       | identifier | Rendering style assigned to the feature                      |
| `LOD`         | integer    | LOD level at which the feature is stored                     |
| `Layer`       | identifier | Target map layer (`roads`, `landuse`, `water`, `pois`, etc.) |
| `OSM_Tags`    | expression | OSM tag expression used to identify the feature              |
| `Description` | text       | Human-readable description                                   |
| `Enabled`     | `0/1`      | Whether the rule is enabled for compilation                  |
| `Icon`        | identifier | POI icon identifier; currently informational                 |

### Example

```text
Code;PG_class;STYLE;LOD;Layer;OSM_Tags;Description;Enabled;Icon

1;ROAD_MAJOR;DARK_GRAY_THICK_LINE;2;roads;highway=motorway;Автомагистраль;1;
2;ROAD_NORMAL;DARK_GRAY_SEMITHICK_LINE;1;roads;highway=primary;Главная дорога;1;
3;ROAD_MINOR;DARK_GRAY_LINE;0;roads;highway=residential;Жилая улица;1;
10;WATER;DARK_GRAY_FILL;1;water;natural=water;Водоем;1;
12;POI_SMALL;DARK_GRAY_CIRCLE;0;pois;amenity=pharmacy;Аптека;1;cross
```

---

## 4.2. OSM Rule Matching

Feature rules are processed **strictly from top to bottom**.

The first matching enabled rule determines the resulting PurrGO feature.

For example, a specific rule must precede a more general rule:

```text
11;POI_BIG;...;place=city, capital=yes;Столица;1;circle
11;POI_BIG;...;place=city;Город;1;circle
```

An object matching both expressions is classified by the first rule.

The same rule applies when an OSM object can belong to different logical map categories.

For example, if an object can be interpreted both as `landuse` and as a POI, its classification is determined by whichever matching rule occurs first in `features.csv`.

There is no separate `Remap_Code`, `Remap_Color` or `Remap_LOD` stage.

---

## 4.3. Disabled Features

If:

```text
Enabled = 0
```

the corresponding rule is ignored by the compiler.

The feature is not written to the binary map as a result of that rule.

---

## 4.4. PurrGO Feature Classes

`PG_class` provides the semantic classification used by PurrGO.

Examples include:

```text
NO_CLASS
ROAD_MAJOR
ROAD_NORMAL
ROAD_MINOR
ROAD_UNPAVED
ROAD_PATH
RAILWAY
LANDUSE_NATURAL
LANDUSE_HUMAN
WATER
POI_BIG
POI_SMALL
```

The exact set of classes is defined by the current `features.csv`.

The binary `.idx` format stores the numeric `Code`, not the textual `PG_class`.

---

## 4.5. Rendering Styles

`STYLE` identifies the rendering style associated with a feature.

Examples:

```text
DARK_GRAY_THICK_LINE
DARK_GRAY_SEMITHICK_LINE
DARK_GRAY_LINE
DARK_GRAY_DOTTED_LINE
DARK_GRAY_DASHED_LINE
RAILWAY_LINE
LIGHT_GRAY_FILL
DARK_GRAY_FILL
DARK_GRAY_CIRCLE
DARK_GRAY_BIG_CIRCLE
```

The style identifier is a compiler/rendering definition and is not stored as a string in `.idx`.

---

## 4.6. POI Icons

The `Icon` field is used only for POI feature definitions.

Examples:

```text
circle
cross
shop
cup
house
fuel
transport
airplane
attraction
```

At the current stage of PurrGO development, no bitmap/icon assets are embedded into the map.

Therefore all POIs are currently rendered using the native POI representation, typically a circle.

The `Icon` field is retained in `features.csv` for future native icon support.

---

# 5. NAVIGATION NODE

A Navigation Node represents a spatial cluster containing child nodes.

Binary unpacking format:

```text
<IiiiiII
```

| Offset        | Size | Type       | Description                                    |
| :------------ | :--- | :--------- | :--------------------------------------------- |
| `0x00 - 0x03` | 4    | `uint32`   | `v3_jump` — jump over the entire child subtree |
| `0x04 - 0x13` | 16   | `int32[4]` | Cluster BBox `[xmin, ymin, xmax, ymax]`        |
| `0x14 - 0x17` | 4    | `uint32`   | `v1` — tree depth                              |
| `0x18 - 0x1B` | 4    | `uint32`   | `v2` — number of child nodes                   |

### Tree Depth

```text
v1 = 0
```

Children are Data Nodes.

```text
v1 > 0
```

Children are Navigation Nodes at depth:

```text
v1 - 1
```

The Navigation Node itself does not contain a cartographic feature code.

---

## 5.1. `v3_jump`

`v3_jump` is used for fast subtree skipping during BBox culling.

`v3_jump` includes an 8-byte hardware prefetch compensation.

Therefore, when the current Navigation Node is culled, the parser advances by:

`v3_jump - 8`

bytes relative to the position immediately after the node.

When a Navigation Node is invisible, the parser performs the corresponding jump over the entire child subtree.

The exact compensation must remain consistent between the map compiler and firmware parser.

---

# 6. LOD SECTION STRUCTURE

Each `.idx` file contains sections for:

```text
LOD 0
LOD 1
LOD 2
```

Each section begins with a 16-byte SQT header.

The section header contains:

| Field                      | Size | Description            |
| :------------------------- | :--- | :--------------------- |
| Magic                      | 4    | `SQT\x01`              |
| Reserved / topology marker | 4    | Current section marker |
| Depth                      | 4    | R-tree depth           |
| Root count                 | 4    | Number of root nodes   |

Conceptually:

```text
+-----------------------+
| SQT Header (16 bytes) |
+-----------------------+
| Root Node             |
| Root Node             |
| ...                   |
+-----------------------+
```

An empty LOD still contains its complete 16-byte SQT header.

---

# 7. SPATIAL INDEX COMPILATION

The compiler builds the `.idx` spatial index using the Sort-Tile-Recursive (STR) bulk-loading algorithm.

### Input

For every compiled feature:

```text
Code
LOD
BBox
v1
v2
```

### Processing

1. Group features by LOD.
2. Sort features by X coordinate.
3. Divide them into spatial slices.
4. Sort each slice by Y coordinate.
5. Pack objects into clusters.
6. Generate Navigation Nodes.
7. Calculate enclosing BBoxes.
8. Generate `v3_jump` values.
9. Write the LOD sections sequentially.

The binary index contains only information necessary for spatial traversal and object retrieval.

OSM tags and textual feature descriptions are **not stored in `.idx`**.

---

# 8. ATTRIBUTE DATABASE STRUCTURE (`.DB`)

The `.db` file stores textual attributes such as object names.

The format is a standard **dBase III (DBF)** database encapsulated inside a YZL container.

## 8.1. DBF Header

Starts at offset `0x20`, immediately after the YZL header.

* dBase III Magic Byte: `0x03`
* Number of Records: 4-byte little-endian value at offset `0x24`
* Database Header Size: `129` (`0x81 0x00`) at offset `0x28`
* Record Size: `117` (`0x75 0x00`) at offset `0x2A`

---

## 8.2. Record Fields

The DBF header contains exactly three 32-byte field descriptors:

1. `osm_id`
2. `code`
3. `name`

The header terminator `0x0D` follows the third field descriptor.
---

## 8.3. Dummy Record

The dummy record is a special exception and is filled entirely with zero bytes.

Unnamed objects reference this record using:

```text
v2 = 1
```

Named records follow:

```text
v2 = 2
v2 = 3
...
```

All subsequent normal DBF records begin with the standard dBase validity indicator byte (`0x20`).

---

## 8.4. Layers Without Names

If a standard layer contains no named objects, `.db` creation may be skipped.

In that case:

```text
v2 = 0
```

for all features in that layer.

---

# 9. POI LAYER

The POI layer therefore does not require a `pois.mlp` geometry file.

A POI Data Node contains its geographic point directly in its BBox:

```text
xmin == xmax
ymin == ymax
```

The coordinates use the same fixed-point `10⁻⁷` degree representation as all other map coordinates.

### Example

```text
xmin = xmax = longitude × 10⁷
ymin = ymax = latitude × 10⁷
```

The `v1` geometry pointer is unused for native POIs.

The feature `Code` identifies the PurrGO POI class.

The `Icon` information originates from `features.csv`, but at the current implementation stage POIs are rendered using a native circle representation.

### 9.1. POI Attribute Database

If named POIs are stored in `pois.db`, the POI database uses the same three-field DBF structure as standard layers.

Unlike standard map layers, the POI database does not contain the zero dummy record.

The first physical POI record has index:

`v2 = 1`

Unnamed POIs use:

`v2 = 0`

---

# 10. OPERATION ALGORITHMS

## 10.1. Map Compilation Pipeline

The PurrGO map compiler performs the following conceptual pipeline:

```text
OpenStreetMap data
        │
        ▼
   OSM objects
        │
        ▼
  features.csv
        │
        │  top-to-bottom matching
        ▼
PurrGO feature definition
        │
        ├── Code
        ├── PG_class
        ├── STYLE
        ├── LOD
        └── Layer
        │
        ▼
   Geometry / POI
        │
        ├───────────────┐
        ▼               ▼
      .mlp             .db
        │
        └───────┐
                ▼
              .idx
                │
                ▼
          YZL containers
```

---

## 10.2. Data Node Generation

For each enabled OSM feature selected by `features.csv`:

1. Determine its PurrGO `Code`.
2. Determine its target `LOD`.
3. Determine its target `Layer`.
4. Calculate its integer BBox using `10⁻⁷` coordinate precision.
5. If it is a linear/polygonal feature, write its geometry to `.mlp`.
6. If it is a native POI, store its point directly in `.idx`.
7. If it has a name, allocate a record in `.db`.
8. Create a Data Node referencing the resulting payload.

---

## 10.3. Hardware Parsing Algorithm

Conceptually:

```python
def parse_node(file, is_nav_node, current_level):
    if not is_nav_node:
        data_bytes = file.read(28)

        xmin, ymin, xmax, ymax, code, v1, v2 = \
            struct.unpack("<iiiiIII", data_bytes)

        render_object(
            xmin,
            ymin,
            xmax,
            ymax,
            code,
            v1,
            v2
        )

        return

    nav_bytes = file.read(28)

    v3_jump, xmin, ymin, xmax, ymax, level, count = \
        struct.unpack("<IiiiiII", nav_bytes)

    if not is_in_screen(
        xmin, ymin,
        xmax, ymax
    ):
        file.seek(v3_jump - 8, 1)
        return

    child_is_nav = level > 0

    for _ in range(count):
        parse_node(
            file,
            child_is_nav,
            level - 1 if child_is_nav else 0
        )
```

The parser converts fixed-point coordinates to display coordinates only when required by the renderer.

---

# 11. INTERNAL RENDERING MODEL

The binary map does not contain textual OSM feature definitions.

The firmware works with compact numeric PurrGO feature codes.

Conceptually:

```text
.idx
 │
 └── Code
      │
      ▼
 PurrGO feature table
      │
      ├── PG_class
      ├── STYLE
      └── rendering parameters
```

The map compiler is responsible for converting the much larger OSM tag space into this compact PurrGO-specific classification.

This prevents the firmware from having to process arbitrary OSM tags.

---

# 12. KNOWN HARDWARE QUIRKS & EDGE CASES

## 12.1. `v3_jump` Prefetch Compensation

The `v3_jump` field requires the currently defined hardware prefetch compensation.

The compiler and firmware must use exactly the same interpretation of this value.

---

## 12.2. Empty LOD Sections

Every LOD section must contain its 16-byte SQT header, including empty sections.

---

## 12.3. Fixed-Point Coordinates

All map coordinates use:

```text
int32 = degrees × 10⁷
```

No floating-point coordinates are stored in `.mlp` or `.idx`.

This applies to:

* geometry coordinates;
* geometry BBoxes;
* Data Node BBoxes;
* Navigation Node BBoxes;
* native POI coordinates.

---

## 12.4. Native POI Representation

POIs do not use geometry records in `.mlp`.

Their point coordinates are stored directly in the `.idx` Data Node BBox:

```text
xmin == xmax
ymin == ymax
```

---

## 12.5. OSM Rule Conflicts

An OSM object may match more than one rule in `features.csv`.

The compiler resolves this deterministically:

> **Rules are processed from top to bottom. The first matching enabled rule wins.**

This includes conflicts between different logical layers, such as:

```text
landuse
POI
```

No secondary remapping stage is used.

---

# 13. SUMMARY

The PurrGO map format consists of three primary binary components:

1. **`.idx`** — spatial index containing LOD sections, Data Nodes and Navigation Nodes.
2. **`.mlp`** — vector geometry storage for linear and polygonal features.
3. **`.db`** — dBase III-compatible attribute database containing textual object names.

Feature classification is defined by:

```text
features.csv
```

with the structure:

```text
Code;PG_class;STYLE;LOD;Layer;OSM_Tags;Description;Enabled;Icon
```

The compiler processes `features.csv` from top to bottom and uses the first matching enabled rule.

The resulting binary map stores compact PurrGO feature codes rather than OSM classification strings.

Geographic coordinates are represented as signed 32-bit integers with a precision of:

```text
10⁻⁷ degrees
```

POIs are represented natively in the spatial index rather than being converted into geometry stored in `.mlp`.

The format is designed for sequential parsing, spatial BBox culling and efficient rendering on resource-constrained PurrGO navigation hardware.
