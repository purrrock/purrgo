# PurrGO MAP FORMAT TECHNICAL SPECIFICATION — V3

---
# 1. GLOBAL LAYER HEADER (PGO HEADER)

Each binary map layer file (`.idx`, `.mlp`, `.db`) must begin with a global PurrGO header with a fixed length of **32 bytes**.
The header is identical for all three file types.
All multi-byte integer fields use **Little-Endian** byte order.

### 1.1. PGO Header Byte Structure

| Offset | Size | Data Type | Field Description            | Value / Format                                                                                                  |
| :----- | :--- | :-------- | :--------------------------- | :-------------------------------------------------------------------------------------------------------------- |
| `0x00` | 3    | `char[3]` | **Magic Signature**          | Strictly `b'PGO'` (`50 47 4F`)                                                                                  |
| `0x03` | 1    | `uint8`   | **File Type**                | `.idx` = `1`, `.mlp` = `2`, `.db` = `3`                                                                         |
| `0x04` | 4    | `uint32`  | **Payload Size**             | Payload size in bytes, Little-Endian. Formula: `File Size - 32`                                                 |
| `0x08` | 4    | `uint32`  | **LOD 0 Offset**             | Absolute byte offset of the LOD 0 SQT block in the `.idx` file. For `.mlp` and `.db`: **for future extensions** |
| `0x0C` | 4    | `uint32`  | **LOD 1 Offset**             | Absolute byte offset of the LOD 1 SQT block in the `.idx` file. For `.mlp` and `.db`: **for future extensions** |
| `0x10` | 4    | `uint32`  | **LOD 2 Offset**             | Absolute byte offset of the LOD 2 SQT block in the `.idx` file. For `.mlp` and `.db`: **for future extensions** |
| `0x14` | 4    | `uint32`  | **Future Extension Field 1** | For future extensions                                                                                           |
| `0x18` | 4    | `uint32`  | **Future Extension Field 2** | For future extensions                                                                                           |
| `0x1C` | 4    | `uint32`  | **Future Extension Field 3** | For future extensions                                                                                           |

### File Type

The `File Type` field identifies the binary layer represented by the file:

```text
1 = .idx
2 = .mlp
3 = .db
```

Other values are currently undefined and are reserved for future extensions.

### Payload Size

`Payload Size` specifies the number of bytes following the 32-byte PGO header.

```text
Payload Size = File Size - 32
```

The payload therefore begins at offset:

```text
0x20
```

For files up to approximately **3.5 GiB**, `uint32` is sufficient for the current PurrGO map format.

### LOD Offsets

For `.idx` files, the three LOD offset fields contain absolute byte offsets from the beginning of the file:

```text
LOD 0 Offset → beginning of LOD 0 SQT block
LOD 1 Offset → beginning of LOD 1 SQT block
LOD 2 Offset → beginning of LOD 2 SQT block
```

This allows the firmware to seek directly to the SQT corresponding to the selected LOD without sequentially traversing preceding LOD sections.

For `.mlp` and `.db` files these fields are currently unused and are reserved **for future extensions**.

### Future Extension Fields

Offsets `0x14`–`0x1F` are currently undefined.
They are explicitly reserved **for future extensions** and must be written as zero by the current compiler.
The current firmware must not assign any additional semantics to these fields.

### 1.2. Header Validation

The C parser must:

1. Verify the `PGO` magic signature.
2. Verify that `File Type` is one of the currently supported values (`1`, `2`, `3`).
3. Read `Payload Size` as Little-Endian `uint32`.
4. Calculate the payload boundary from the 32-byte header.
5. For `.idx`, validate that each LOD offset is within the file payload boundary.
6. Reject malformed or unsupported headers before parsing the payload.
7. Treat the future-extension fields as opaque and ignore their contents.

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

Immediately following the 32-byte global PGO header is an array of geometry records.

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

### 3.1. Nodes

Navigation Node занимает 28 байт, а Data Node занимает 25 байт.

The last 8 bytes have a fixed interpretation depending on node type and allow the parser to traverse the tree.

Coordinates in all nodes are stored as signed 32-bit integers using the `10⁻⁷` degree coordinate representation.

---

## 3.2. Data Node

A Data Node represents one cartographic feature.
Размер 25 байт. Распаковка `<iiiiBII`.
| Offset        | Size | Type       | Description                                   |
| :------------ | :--- | :--------- | :-------------------------------------------- |
| `0x00 - 0x0F` | 16   | `int32[4]` | BBox `[xmin, ymin, xmax, ymax]` |
| `0x10`        | 1    | `uint8`    | **Type** — PurrGO feature code |
| `0x11 - 0x14` | 4    | `uint32`   | **v1** — pointer to geometry record in `.mlp` |
| `0x15 - 0x18` | 4    | `uint32`   | **v2** — record index in `.db` |

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

`v3_jump` is the exact physical size of the skipped child subtree in bytes. Therefore, when the current Navigation Node is culled, the parser advances by exactly: `v3_jump` bytes.

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

| Offset | Size | Поле         | Значение                                       |
| ------ | ---: | ------------ | ---------------------------------------------- |
| `0x00` |    4 | Magic        | `SQT\x01`                                      |
| `0x04` |    4 | Topology     | `1` — PurrGO SQT topology                      |
| `0x08` |    4 | Mode / Depth | `0` = flat, `1` = one-level, `>1` = tree depth |
| `0x0C` |    4 | Root count   | Количество root nodes                          |

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

The format is a standard **dBase III (DBF)** database encapsulated inside a PGO container.

## 8.1. DBF Header

Starts at offset `0x20`, immediately after the PGO header.

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
          PGO containers
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
        # Узел данных теперь занимает 25 байт (код уменьшен до 1 байта)
        data_bytes = file.read(25)
        
        xmin, ymin, xmax, ymax, code, v1, v2 = \
            struct.unpack("<iiiiBII", data_bytes)
        
        render_object(xmin, ymin, xmax, ymax, code, v1, v2)
        return

    nav_bytes = file.read(28)
    v3_jump, xmin, ymin, xmax, ymax, level, count = \
        struct.unpack("<IiiiiII", nav_bytes)

    if not is_in_screen(xmin, ymin, xmax, ymax):
        # Точный прыжок без компенсации
        file.seek(v3_jump, 1)
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

## Geometry type by layer

The map compiler guarantees that geometry types are consistent with the target layer. This is a format-level invariant and may therefore be relied upon by the PurrGO navigator renderer:

* `roads` — **line geometry only**;
* `landuse` — **polygon geometry only**;
* `water` — **polygon geometry only**;
* `pois` — **point geometry only**.

The navigator does not need to detect or validate geometry type at runtime. A violation of this invariant indicates an invalid map produced by the map compiler and is outside the normal runtime data contract.

## Rendering model

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

## 12.1. `v3_jump` Prefetch Compensation not need now

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
* POI coordinates.

---

## 12.4. POI Representation

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
