# PurrGO Map Format — Technical Specification V3

**Status:** normative specification

This document defines the binary PurrGO map format V3 and the rules required for a compatible map compiler and firmware parser.

The format consists of three PGO-wrapped files:

* `.idx` — spatial index;
* `.mlp` — vector geometry;
* `.db` — object attributes.

Feature classification is defined by `features.csv`.

All multi-byte integer fields use **Little-Endian** byte order unless explicitly stated otherwise.

---

# 1. PGO CONTAINER

Every `.idx`, `.mlp` and `.db` file begins with a fixed-size **32-byte PGO header**.

## 1.1. Header layout

| Offset | Size | Type      | Field              | Description                                 |
| -----: | ---: | --------- | ------------------ | ------------------------------------------- |
| `0x00` |    3 | `char[3]` | Magic              | Strictly `PGO` (`50 47 4F`)                 |
| `0x03` |    1 | `uint8`   | File Type          | `1` = `.idx`, `2` = `.mlp`, `3` = `.db`     |
| `0x04` |    4 | `uint32`  | Payload Size       | File size minus 32                          |
| `0x08` |    4 | `uint32`  | LOD 0 Offset       | Absolute file offset of LOD 0 SQT in `.idx` |
| `0x0C` |    4 | `uint32`  | LOD 1 Offset       | Absolute file offset of LOD 1 SQT in `.idx` |
| `0x10` |    4 | `uint32`  | LOD 2 Offset       | Absolute file offset of LOD 2 SQT in `.idx` |
| `0x14` |    4 | `uint32`  | Future Extension 1 | Reserved; must be zero                      |
| `0x18` |    4 | `uint32`  | Future Extension 2 | Reserved; must be zero                      |
| `0x1C` |    4 | `uint32`  | Future Extension 3 | Reserved; must be zero                      |

The payload begins at:

```text
0x20
```

The payload size is:

```text
Payload Size = File Size - 32
```

`uint32` therefore permits files of up to approximately 4 GiB; the current implementation limits payload size to `0xFFFFFFFF`.

## 1.2. File type

```text
1 = .idx
2 = .mlp
3 = .db
```

Other values are reserved for future extensions.

## 1.3. LOD offsets

For `.idx`, the three LOD fields contain **absolute byte offsets from the beginning of the file**:

```text
LOD 0 Offset → beginning of LOD 0 SQT block
LOD 1 Offset → beginning of LOD 1 SQT block
LOD 2 Offset → beginning of LOD 2 SQT block
```

They allow the firmware to seek directly to a selected LOD.

For `.mlp` and `.db`, these fields are currently unused and must be zero.

## 1.4. Reserved fields

Offsets `0x14`–`0x1F` are reserved for future extensions.

The current compiler writes zero to these fields.

Current firmware must treat them as opaque and assign them no additional meaning.

## 1.5. Header validation

A parser must:

1. verify the `PGO` magic;
2. verify that `File Type` is `1`, `2` or `3`;
3. read `Payload Size`;
4. verify that the declared payload fits inside the file;
5. for `.idx`, verify that every LOD offset is inside the file;
6. reject malformed or unsupported headers before parsing the payload.

---

# 2. COORDINATE REPRESENTATION

All map coordinates use signed 32-bit fixed-point integers with a precision of:

```text
10⁻⁷ degrees
```

Conversion from geographic coordinates:

```text
integer_coordinate = geographic_coordinate × 10⁷
```

Example:

```text
55.7558000° → 557558000
37.6173000° → 376173000
```

Coordinate order:

```text
X = longitude
Y = latitude
```

No floating-point coordinates are stored in `.idx` or `.mlp`.

The STM32 map/navigation path operates on these integer coordinates. Floating-point arithmetic is not required for binary map parsing or spatial traversal.

---

# 3. GEOMETRY FILE (`.MLP`)

The `.mlp` file stores ordered coordinates for linear and polygonal map features.

The file consists of a PGO header followed by geometry records.

```text
+-------------------------+
| PGO Header (32 bytes)   |
+-------------------------+
| Geometry Record         |
+-------------------------+
| Geometry Record         |
+-------------------------+
| ...                     |
+-------------------------+
```

## 3.1. Geometry Record Local Header

Each geometry record begins with an 8-byte local header.

| Offset | Size | Type     | Endianness    | Description                                    |
| -----: | ---: | -------- | ------------- | ---------------------------------------------- |
| `0x00` |    4 | `uint32` | Big-Endian    | Geometry record sequence number, starting at 1 |
| `0x04` |    4 | `uint32` | Little-Endian | Geometry record body length in bytes           |

The record body immediately follows this local header.

## 3.2. Geometry Record Body

The body length is exactly the value stored in the local header.

|   Offset |             Size | Type         | Description                     |
| -------: | ---------------: | ------------ | ------------------------------- |
|   `0x00` |               16 | `int32[4]`   | BBox `[minx, miny, maxx, maxy]` |
|   `0x10` |                4 | `int32`      | `num_parts`                     |
|   `0x14` |                4 | `int32`      | `num_points`                    |
|   `0x18` |  `num_parts × 4` | `uint32[]`   | `parts` array                   |
| variable | `num_points × 8` | `int32[2][]` | Points `[X,Y]`                  |

All coordinates use the `10⁻⁷` degree representation.

`parts` contains **point indices**, not contour lengths.

For a geometry with `num_parts` parts, each `parts[i]` identifies the first point of part `i`.

## 3.3. Geometry pointer (`v1`)

For a normal Data Node, `v1` identifies the corresponding geometry inside `.mlp`.

`v1` is an offset **relative to the beginning of the `.mlp` payload**, i.e. relative to file offset `0x20`.

It points to the beginning of the **Geometry Record Body**, not to the 8-byte Local Header.

Therefore:

```text
absolute .mlp file offset = 0x20 + v1
```

For example, if:

```text
v1 = 8
```

the pointer refers to the first byte of the first Geometry Record Body.

The 8-byte Local Header is therefore not included in `v1`.

The current compiler implements this as:

```text
v1 = geometry_record_offset_in_payload + 8
```

## 3.4. Polygon topology

Multipolygon geometry uses the same representation as all other geometry.

* all rings are flattened into one `points` array;
* `num_parts` specifies the number of rings;
* `parts` contains the starting point index of each ring;
* rings must be closed;
* the first and last point of a ring must be identical.

Winding rules:

```text
Outer rings → Clockwise (CW)
Inner rings → Counter-Clockwise (CCW)
```

A simple polygon consists of one closed part.

---

# 4. SPATIAL INDEX (`.IDX`)

The `.idx` file provides spatial lookup and BBox culling.

PurrGO currently defines:

```text
LOD 0
LOD 1
LOD 2
```

Each LOD is stored as an independent SQT section.

The spatial index uses a hierarchical tree generated using the **Sort-Tile-Recursive (STR)** bulk-loading algorithm.

The index contains only information required for spatial traversal and feature retrieval. OSM tags and textual descriptions are not stored in `.idx`.

---

# 5. SQT LOD SECTIONS

Each LOD section begins with a fixed 16-byte SQT header.

## 5.1. SQT header

| Offset | Size | Type      | Description          |
| -----: | ---: | --------- | -------------------- |
| `0x00` |    4 | `char[4]` | Magic: `SQT\x01`     |
| `0x04` |    4 | `uint32`  | Topology: `1`        |
| `0x08` |    4 | `uint32`  | Tree depth           |
| `0x0C` |    4 | `uint32`  | Number of root nodes |

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

For an empty LOD:

```text
depth = 0
root_count = 0
```

---

# 6. NAVIGATION NODE

A Navigation Node represents a spatial cluster containing child nodes.

Its binary size is **28 bytes**.

Binary layout:

```text
<IiiiiII
```

| Offset | Size | Type       | Description                          |
| -----: | ---: | ---------- | ------------------------------------ |
| `0x00` |    4 | `uint32`   | `v3_jump`                            |
| `0x04` |   16 | `int32[4]` | Cluster BBox `[xmin,ymin,xmax,ymax]` |
| `0x14` |    4 | `uint32`   | `v1` — tree level                    |
| `0x18` |    4 | `uint32`   | `v2` — number of children            |

All BBox coordinates use the `10⁻⁷` degree representation.

## 6.1. Tree level

For:

```text
v1 = 0
```

all children are Data Nodes.

For:

```text
v1 > 0
```

all children are Navigation Nodes with:

```text
child level = v1 - 1
```

Thus the tree is homogeneous at every Navigation Node level.

---

# 7. `v3_jump`

`v3_jump` is the exact physical size, in bytes, of the complete child subtree belonging to the Navigation Node.

It does **not** include the 28-byte Navigation Node itself.

Examples:

For a level-0 Navigation Node with 14 Data Nodes:

```text
v3_jump = 14 × 25
        = 350 bytes
```

For a higher-level Navigation Node:

```text
v3_jump = sum(child.bin_size)
```

where each child is another Navigation Node.

## 7.1. Subtree skipping

After the Navigation Node header has been read, the parser position is immediately at the first child.

If the Navigation Node BBox does not intersect the current viewport, the entire child subtree can be skipped:

```text
file_position += v3_jump
```

No compensation is applied.

There is no prefetch compensation or additional adjustment associated with `v3_jump`.

The compiler and firmware must use the same definition.

---

# 8. DATA NODE

A Data Node represents one cartographic feature.

Its binary size is **25 bytes**.

Binary format:

```text
<iiiiBII
```

| Offset | Size | Type       | Description                  |
| -----: | ---: | ---------- | ---------------------------- |
| `0x00` |   16 | `int32[4]` | BBox `[xmin,ymin,xmax,ymax]` |
| `0x10` |    1 | `uint8`    | `Type` — PurrGO feature code |
| `0x11` |    4 | `uint32`   | `v1` — geometry pointer      |
| `0x15` |    4 | `uint32`   | `v2` — DB record index       |

## 8.1. Feature code

`Type` is the numeric `Code` from `features.csv`.

The firmware uses this numeric code to identify the PurrGO feature class and rendering style.

The original OSM tags are not required by the firmware.

For example:

```text
features.csv

2;ROAD_NORMAL;DARK_GRAY_SEMITHICK_LINE;1;roads;highway=primary;Главная дорога;1;
```

produces:

```text
Type = 2
```

The meaning of the code is defined by the corresponding `features.csv` entry.

The textual fields `PG_class`, `STYLE`, `LOD`, `Layer` and `OSM_Tags` are not stored in the Data Node.

## 8.2. `v1`

For linear and polygonal features:

```text
v1 = offset of Geometry Body in .mlp payload
```

For native POIs:

```text
v1 = 0
```

See [Section 3.3](#33-geometry-pointer-v1).

## 8.3. `v2`

For features with an associated attribute record:

```text
v2 = DB record index
```

For layers without a `.db` file:

```text
v2 = 0
```

The exact `.db` indexing rules are defined in Section 10.

---

# 9. FEATURE CLASSIFICATION (`features.csv`)

`features.csv` is the source classification table used by the map compiler.

The format is:

```text
Code;PG_class;STYLE;LOD;Layer;OSM_Tags;Description;Enabled;Icon
```

## 9.1. Fields

| Field         | Type       | Description                                      |
| ------------- | ---------- | ------------------------------------------------ |
| `Code`        | integer    | Numeric PurrGO feature code stored in Data Nodes |
| `PG_class`    | identifier | Semantic PurrGO feature class                    |
| `STYLE`       | identifier | Rendering style                                  |
| `LOD`         | integer    | LOD at which the feature is stored               |
| `Layer`       | identifier | Target layer                                     |
| `OSM_Tags`    | expression | OSM tag expression used for matching             |
| `Description` | text       | Human-readable description                       |
| `Enabled`     | `0/1`      | Whether the rule is active                       |
| `Icon`        | identifier | POI icon identifier                              |

Example:

```text
Code;PG_class;STYLE;LOD;Layer;OSM_Tags;Description;Enabled;Icon

1;ROAD_MAJOR;DARK_GRAY_THICK_LINE;2;roads;highway=motorway;Автомагистраль;1;
2;ROAD_NORMAL;DARK_GRAY_SEMITHICK_LINE;1;roads;highway=primary;Главная дорога;1;
3;ROAD_MINOR;DARK_GRAY_LINE;0;roads;highway=residential;Жилая улица;1;
10;WATER;DARK_GRAY_FILL;1;water;natural=water;Водоем;1;
12;POI_SMALL;DARK_GRAY_CIRCLE;0;pois;amenity=pharmacy;Аптека;1;cross
```

The exact set of feature classes and styles is defined by the current `features.csv`.

## 9.2. Rule matching

Rules are evaluated strictly from top to bottom.

The **first matching enabled rule wins**.

Therefore more specific rules must precede more general rules.

Example:

```text
11;POI_BIG;...;place=city, capital=yes;Столица;1;circle
11;POI_BIG;...;place=city;Город;1;circle
```

An object matching both rules is classified by the first rule.

This rule also resolves conflicts between logical categories such as `landuse` and `POI`.

There is no separate remapping stage such as:

```text
Remap_Code
Remap_Color
Remap_LOD
```

## 9.3. Disabled rules

If:

```text
Enabled = 0
```

the rule is ignored by the compiler.

No feature is generated from that rule.

## 9.4. Feature classes

`PG_class` provides the semantic PurrGO classification.

Examples:

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

The binary map stores only the numeric `Code`.

## 9.5. Rendering styles

`STYLE` identifies the rendering style assigned to the feature.

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

Style names are not stored as strings in `.idx`.

## 9.6. POI icons

`Icon` is currently metadata associated with POI feature definitions.

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

Bitmap/icon assets are not embedded in the map format.

Current PurrGO POIs use a native renderer representation, typically a circle.

The `Icon` field is retained for future native icon support.

---

# 10. ATTRIBUTE DATABASE (`.DB`)

The `.db` file stores textual attributes such as object names.

It contains a **dBase III (DBF)** database encapsulated in a PGO container.

The DBF payload begins at file offset:

```text
0x20
```

## 10.1. DBF header

The current format uses:

```text
Magic byte       = 0x03
Header size      = 129 bytes (0x0081)
Record size      = 117 bytes (0x0075)
```

The number of records is stored as a 4-byte little-endian value.

The DBF header contains exactly three field descriptors:

```text
osm_id
code
name
```

The field descriptor for each field is 32 bytes.

The header terminator is:

```text
0x0D
```

## 10.2. Record layout

Each normal record is 117 bytes:

```text
+----------+------------+----------+----------------------+
| Validity | osm_id     | code     | name                 |
| 1 byte   | 12 bytes   | 4 bytes  | 100 bytes            |
+----------+------------+----------+----------------------+
```

The normal record validity indicator is:

```text
0x20
```

## 10.3. Standard layer dummy record

Standard map layers use a special zero-filled dummy record at DB record index 1.

The dummy record is:

```text
117 bytes of 0x00
```

Named objects use:

```text
v2 = 2
v2 = 3
...
```

Thus:

```text
v2 = 1
```

identifies the dummy record.

## 10.4. Layers without names

If a standard layer contains no named objects, creation of its `.db` file may be skipped.

All features in that layer then use:

```text
v2 = 0
```

## 10.5. POI database

A POI `.db` uses the same three-field DBF structure but does not contain the standard dummy record.

Therefore:

```text
v2 = 1
```

is the first physical POI record.

Unnamed POIs use:

```text
v2 = 0
```

---

# 11. MAP METADATA (`map.name`)

Every PurrGO map package must contain a `map.name` file.

Unlike `.idx`, `.mlp` and `.db`, `map.name` is a text file and is **not wrapped in a PGO binary header**.

The file uses **strict JSON** and contains the map name and the geographic center of the map.

## 11.1. JSON structure

The required structure is:

```json
{"centerLat":55.7558,"centerLon":37.6173,"mapName":"Moscow"}
```

The three required fields are:

| Field       | JSON type | Description                             |
| ----------- | --------- | --------------------------------------- |
| `centerLat` | number    | Map center latitude in decimal degrees  |
| `centerLon` | number    | Map center longitude in decimal degrees |
| `mapName`   | string    | Human-readable map name                 |

The coordinate order is:

```text
centerLat = latitude
centerLon = longitude
```

Unlike coordinates stored in `.idx` and `.mlp`, the coordinates in `map.name` are stored directly as decimal-degree JSON numbers.

## 11.2. Map center

`centerLat` and `centerLon` represent the center calculated from the **global BBox of all map layers** included in the map package.

They are used by the PC map viewer and by the firmware as the initial map/camera center.

The center is metadata only. It does not replace the spatial BBoxes stored in `.idx`.

## 11.3. JSON representation

The current PurrGO map package uses compact JSON without unnecessary whitespace.

The compiler should serialize the object using JSON separators equivalent to:

```python
json.dumps(data, separators=(',', ':'))
```

Therefore the preferred representation is:

```json
{"centerLat":55.7558,"centerLon":37.6173,"mapName":"Moscow"}
```

rather than:

```json
{
    "centerLat": 55.7558,
    "centerLon": 37.6173,
    "mapName": "Moscow"
}
```

The compact representation keeps the file small and is compatible with the current firmware JSON parsing requirements.

## 11.4. Map package structure

A complete map package therefore has the following structure:

```text
<map>/
├── map.name
├── landuse.idx
├── landuse.mlp
├── landuse.db
├── water.idx
├── water.mlp
├── water.db
├── roads.idx
├── roads.mlp
├── roads.db
├── pois.idx
└── pois.db
```

Layer files that contain no data may be omitted according to the layer rules defined elsewhere in this specification. The `map.name` file itself is mandatory.

## 11.5. Relationship to binary map files

`map.name` is package-level metadata:

```text
                    Map Package
                         │
             ┌───────────┴───────────┐
             │                       │
         map.name               Binary layers
             │                       │
       ┌─────┴─────┐          ┌──────┼──────┐
       │           │          │      │      │
    mapName    center       .idx   .mlp    .db
```

`map.name` does not contain feature data, geometry, spatial index data or attribute records.

---


# 12. POI LAYER

POIs are point features and do not require an `.mlp` geometry record.

A POI Data Node stores its point directly in its BBox:

```text
xmin == xmax
ymin == ymax
```

The coordinates use the same fixed-point representation:

```text
coordinate = degrees × 10⁷
```

For a POI:

```text
v1 = 0
```

The feature `Type` identifies the PurrGO POI class.

The `Icon` metadata originates from `features.csv`, but the current renderer uses the native POI representation.

---

# 13. LAYER GEOMETRY INVARIANTS

The compiler guarantees that geometry type is consistent with the target layer.

The current layer contract is:

| Layer     | Geometry              |
| --------- | --------------------- |
| `roads`   | line geometry only    |
| `landuse` | polygon geometry only |
| `water`   | polygon geometry only |
| `pois`    | point geometry only   |

The firmware may rely on this invariant and does not need to determine geometry type dynamically.

A violation indicates an invalid map produced by the compiler.

---

# 14. STR INDEX CONSTRUCTION

The `.idx` spatial index is generated using the Sort-Tile-Recursive (STR) bulk-loading algorithm.

Conceptually:

1. group features by LOD;
2. sort features by X centroid;
3. divide them into spatial slices;
4. sort each slice by Y centroid;
5. divide objects into clusters;
6. generate Navigation Nodes;
7. calculate enclosing BBoxes;
8. calculate `v3_jump`;
9. serialize the resulting tree into the LOD section.

The current implementation uses a maximum cluster size of:

```text
14 children
```

for Navigation Nodes.

A level-0 Navigation Node contains Data Nodes.

Higher-level Navigation Nodes contain lower-level Navigation Nodes.

---

# 15. MAP COMPILATION MODEL

The complete compilation flow is:

```text
OpenStreetMap data
        │
        ▼
   OSM objects
        │
        ▼
   features.csv
        │
        │ first matching enabled rule
        ▼
 PurrGO feature definition
        │
        ├── Code
        ├── PG_class
        ├── STYLE
        ├── LOD
        └── Layer
        │
        ├─────────────────┐
        ▼                 ▼
  Geometry              POI
        │                 │
        ▼                 ▼
      .mlp              .idx
        │                 │
        └──────┐    ┌─────┘
               ▼    ▼
                 .db
```

For normal linear and polygonal features:

1. determine `Code`;
2. determine `LOD`;
3. determine `Layer`;
4. calculate integer BBox;
5. write geometry to `.mlp`;
6. assign `v1` to the Geometry Body;
7. create the corresponding Data Node;
8. assign `v2` if an attribute record exists;
9. insert the Data Node into the LOD spatial index.

For native POIs:

1. determine `Code`;
2. determine `LOD`;
3. determine `Layer = pois`;
4. calculate the point coordinates;
5. create a zero-area Data Node BBox;
6. set `v1 = 0`;
7. assign `v2` if a POI attribute record exists;
8. insert the Data Node into the LOD spatial index.

---

# 16. FIRMWARE PARSING MODEL

The firmware does not parse OSM tags.

It operates on:

```text
.idx
.mlp
.db
```

and the corresponding numeric PurrGO feature definitions.

Conceptually:

```text
.idx
 │
 └── Data Node
      │
      ├── BBox
      ├── Type ───────────────┐
      ├── v1 ──→ .mlp         │
      └── v2 ──→ .db          │
                              ▼
                       Feature definition
                              │
                              ├── PG_class
                              ├── STYLE
                              └── rendering parameters
```

Spatial traversal is performed using integer BBoxes.

A Navigation Node whose BBox does not intersect the current viewport can be skipped using its exact `v3_jump`.

A Data Node that passes the spatial test is passed to the renderer.

---

# 17. BINARY FORMAT INVARIANTS

The following properties are normative for V3:

1. All PGO containers have a 32-byte header.
2. The PGO magic is exactly `PGO`.
3. `.idx`, `.mlp` and `.db` use file types `1`, `2` and `3`.
4. All multi-byte integers are Little-Endian unless explicitly specified otherwise.
5. `.mlp` geometry record sequence numbers are Big-Endian.
6. Coordinates use signed `int32` with `10⁻⁷` degree precision.
7. `.idx` Data Nodes are exactly 25 bytes.
8. `.idx` Navigation Nodes are exactly 28 bytes.
9. `v1` in a normal Data Node points to the Geometry Body in `.mlp`, relative to the beginning of the `.mlp` payload.
10. The `.mlp` Local Header is not included in the `v1` target.
11. `v3_jump` is exactly the byte size of the complete child subtree.
12. No `v3_jump` compensation is applied.
13. Empty LODs still contain their 16-byte SQT headers.
14. POIs do not use `.mlp` geometry records.
15. POI coordinates are represented by a zero-area Data Node BBox.
16. OSM rule matching uses first-match-wins semantics.
17. Disabled `features.csv` rules are ignored.
18. OSM tags and textual feature definitions are not stored in `.idx`.
19. Geometry type is determined by the target layer.
20. Reserved PGO header fields have no current runtime semantics.

---

# 18. FILE RELATIONSHIPS

The relationship between the three binary files is:

```text
.idx
 │
 ├── Data Node.Type ──────→ features.csv / PurrGO feature definition
 │
 ├── Data Node.v1 ────────→ .mlp Geometry Body
 │
 └── Data Node.v2 ────────→ .db record
```

For native POIs:

```text
.idx
 │
 ├── Data Node.Type ──────→ POI feature definition
 ├── Data Node.BBox ──────→ point coordinates
 ├── v1 = 0
 └── v2 ──────────────────→ optional POI .db record
```

This separation keeps the runtime binary representation compact while leaving OSM-specific classification to the PC-side map compiler.

---

# 19. IMPLEMENTATION CONSTRAINTS

The binary map format is designed for resource-constrained firmware.

The STM32 implementation should:

* use fixed-width integer types;
* operate directly on fixed-point coordinates;
* avoid floating-point arithmetic in the production map/navigation path;
* avoid dynamic allocation where practical;
* rely on the compiler's geometry/layer invariants;
* reject malformed binary structures rather than attempting to recover from them.

The PC-side compiler is not subject to the STM32 runtime arithmetic restriction and may use floating-point arithmetic where required by compilation algorithms.

---

# 20. VERSIONING

This document defines **PurrGO Map Format V3**.

Changes to any of the following constitute a binary-format change and require corresponding updates to both compiler and firmware:

* PGO header;
* SQT header;
* Navigation Node layout;
* Data Node layout;
* Geometry Record layout;
* `.db` layout;
* meaning of `v1`, `v2` or `v3_jump`;
* coordinate representation;
* LOD representation;
* feature-code semantics.
