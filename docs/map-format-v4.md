# PurrGO Map Format — Technical Specification V4

**Status:** normative specification, **draft** (compiler and firmware are still on V3; see Section 0.3 and Appendix A)

This document defines the binary PurrGO map format V4 and the rules required for a compatible map compiler and firmware parser.

The format consists of three PGO-wrapped files per layer:

* `.idx` — spatial index (block layout, **new in V4**);
* `.mlp` — vector geometry;
* `.db` — object attributes;

and one optional package-level file:

* `search.idx` — search index for POI search (**new in V4**, Section 21).

Feature classification is defined by `features.csv`; the subtype dictionary for search is defined by `search_categories.csv` (Section 21.8).

All multi-byte integer fields use **Little-Endian** byte order unless explicitly stated otherwise.

The document is based on [`map-format-v3.md`](map-format-v3.md). Sections whose text did not change keep their V3 numbers, so references such as "Section 10" or "Section 8.1.1" stay valid.

Design rationale: [`design/pgo-v4.md`](design/pgo-v4.md) (block index layout) and [`design/poi-search.md`](design/poi-search.md) (POI search).

---

# 0. SCOPE AND CHANGES FROM V3

## 0.1. What changed

| Area | V3 | V4 |
| ---- | -- | -- |
| `.idx` tree layout | depth-first, children interleaved with their subtrees | **block layout**: all children of a node form one 512-byte block, blocks are ordered by level (Sections 5–7) |
| Subtree access | relative skip `v3_jump` | absolute offset of the child block `child_block` |
| SQT magic | `SQT\x01` | `SQT\x02` |
| SQT header | 16 bytes, followed directly by nodes | one 512-byte header sector (Section 5.1) |
| Fan-out | 14 children per node | 18 Navigation Nodes or 20 Data Nodes per block (Section 6.1) |
| Alignment | none | LOD sections and blocks start at file offsets that are multiples of 512 |
| POI `.db` | text said "unnamed POI → `v2 = 0`" | every POI has a record, `v2 ≥ 1` (Section 10.5; the V3 compiler already did this) |
| Name field | not specified | name selection and encoding rules written down (Section 10.6) |
| Search | none | optional `search.idx` (Section 21), file type `4` |

## 0.2. What did not change

* the PGO container header (Section 1), except the new file type `4` and the alignment rules for `.idx`;
* coordinates (Section 2);
* `.mlp` geometry records and the meaning of `v1` (Section 3);
* the Navigation Node size (28 bytes) and the Data Node layout and size (25 bytes);
* `features.csv`, feature codes and their ranges (Sections 8.1 and 9);
* the `.db` DBF layout (Sections 10.1–10.4);
* `map.name` (Section 11);
* the layer and geometry invariants (Section 13).

## 0.3. Compatibility

* A V3 reader rejects a V4 `.idx` at the SQT magic check and must not try to parse it.
* The version of an `.idx` file is identified by the magic of its SQT sections: `SQT\x01` is V3, `SQT\x02` is V4. All three sections of one file have the same magic. A transitional firmware may support both and choose the parser per file by the magic of LOD 0.
* A map package is compiled completely as V3 or completely as V4. Mixing V3 and V4 layer files in one package is not supported.
* `.mlp`, `.db` and `map.name` are byte-identical in V3 and V4 packages built from the same source data.
* `search.idx` is optional. A reader that does not know it ignores it (Section 11.4.1).

---

# 1. PGO CONTAINER

Every `.idx`, `.mlp`, `.db` and `search.idx` file begins with a fixed-size **32-byte PGO header**.

## 1.1. Header layout

| Offset | Size | Type      | Field              | Description                                 |
| -----: | ---: | --------- | ------------------ | ------------------------------------------- |
| `0x00` |    3 | `char[3]` | Magic              | Strictly `PGO` (`50 47 4F`)                 |
| `0x03` |    1 | `uint8`   | File Type          | `1` = `.idx`, `2` = `.mlp`, `3` = `.db`, `4` = `search.idx` |
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
4 = search.idx
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

In V4 the LOD offsets are multiples of 512, the first one is `0x200`, and the sections follow each other in the order LOD 0, LOD 1, LOD 2 without gaps (Section 4.1). A LOD section extends to the next LOD offset; the last one extends to the end of the file.

For `.mlp`, `.db` and `search.idx`, these fields are currently unused and must be zero.

## 1.4. Reserved fields

Offsets `0x14`–`0x1F` are reserved for future extensions.

The current compiler writes zero to these fields.

Current firmware must treat them as opaque and assign them no additional meaning.

## 1.5. Header validation

A parser must:

1. verify the `PGO` magic;
2. verify that `File Type` is `1`, `2`, `3` or `4`;
3. read `Payload Size`;
4. verify that the declared payload fits inside the file;
5. for `.idx`, verify that every LOD offset is inside the file, is a multiple of 512, is at least `0x200`, and that LOD 0 < LOD 1 < LOD 2;
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

Rings are filled with the even-odd rule, so an inner ring is a hole (for example an island in a lake).

### 3.4.1. Limits

One geometry record holds at most `2000` points in total over all parts and at most `32` parts. The firmware rejects a larger record as a whole, and the object is not drawn. The compiler skips ways and multipolygons above `1999` points, so every object it writes fits the limit.

### 3.4.2. Multipolygon construction in the compiler

* the rings of a relation are assembled from member ways: closed ways are taken as they are, open ways are chained by shared end points;
* a member with an empty role is an outer ring;
* a ring that cannot be closed (incomplete data at the edge of the extract) is dropped;
* an inner ring is kept only if it lies inside one of the assembled outer rings; a relation without a closed outer ring produces nothing, so islands are never written without their lake;
* for water, a multipolygon replaces the standalone polygon of its outer way, whether the relation duplicates the tags of that way or carries none (old-style tagging). Otherwise the solid polygon would cover the holes.

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

The spatial index is a hierarchical tree generated using the **Sort-Tile-Recursive (STR)** bulk-loading algorithm. In V4 the tree is stored as **blocks**: the children of one Navigation Node are stored together in one block of 512 bytes, so a reader obtains all children of a node with a single sector read and decides from their BBoxes which subtrees to descend into.

The index contains only information required for spatial traversal and feature retrieval. OSM tags and textual descriptions are not stored in `.idx`.

## 4.1. File layout

```text
+-----------------------------+ 0x000
| PGO Header (32 bytes)       |
+-----------------------------+ 0x020
| zero padding                |
+-----------------------------+ 0x200   ← LOD 0 Offset
| LOD 0 section               |
+-----------------------------+         ← LOD 1 Offset
| LOD 1 section               |
+-----------------------------+         ← LOD 2 Offset
| LOD 2 section               |
+-----------------------------+ end of file
```

Rules:

* bytes `0x20..0x1FF` are zero;
* the LOD 0 offset is `0x200`; LOD 1 and LOD 2 follow without gaps, in this order;
* every LOD offset is a multiple of 512;
* the size of every LOD section is a multiple of 512; therefore the file size is a multiple of 512;
* `Payload Size` keeps its V3 definition: file size minus 32.

The 512-byte alignment is measured from the beginning of the file, not from the beginning of the payload. Files are read from a FAT volume whose clusters are sector-aligned, so a 512-byte block at a file offset that is a multiple of 512 occupies exactly one SD sector.

---

# 5. SQT LOD SECTIONS

A LOD section consists of one **header sector** followed by **blocks**. Both are 512 bytes. A block address is an absolute file offset that is a multiple of 512.

```text
section start S
+---------------------------+ S
| Header sector (512 bytes) |
+---------------------------+ S + 512
| Root block                |
+---------------------------+ S + 1024
| Blocks of level depth-2   |   (in the order defined in Section 7.3)
| ...                       |
| Leaf blocks               |
+---------------------------+ S + section_size
```

## 5.1. SQT header sector

| Offset | Size | Type      | Description                                                      |
| -----: | ---: | --------- | ---------------------------------------------------------------- |
| `0x00` |    4 | `char[4]` | Magic: `SQT\x02`                                                 |
| `0x04` |    4 | `uint32`  | Topology: `2` (block tree)                                       |
| `0x08` |    4 | `uint32`  | Tree depth: number of node levels; `0` for an empty LOD          |
| `0x0C` |    4 | `uint32`  | Number of root nodes (`root_count`)                              |
| `0x10` |    4 | `uint32`  | Block size in bytes: `512`                                       |
| `0x14` |    4 | `uint32`  | Section size in bytes: header sector + all blocks                |
| `0x18` |    2 | `uint16`  | Maximum Navigation Nodes per block (`nav_fanout`): `18`          |
| `0x1A` |    2 | `uint16`  | Maximum Data Nodes per block (`leaf_fanout`): `20`               |
| `0x1C` |  484 | `uint8[]` | Reserved; must be zero                                           |

A V4 reader requires `Block size = 512`, `nav_fanout ≤ 18` and `leaf_fanout ≤ 20`. The fields exist so that a later revision can change the block size without changing the header layout; V4 does not allow other values.

`Section size` must be a multiple of 512 and must equal the distance to the next LOD offset (for LOD 2: to the end of the file).

## 5.2. Empty LOD

An empty LOD consists of the header sector only:

```text
depth = 0
root_count = 0
section_size = 512
```

It has no root block.

## 5.3. Root block

For a non-empty LOD the root block starts at `S + 512`. It contains `root_count` Navigation Nodes (`1 ≤ root_count ≤ 18`) of level `depth - 1`.

`depth` is the number of Navigation Node levels. The root nodes have level `depth - 1`; the nodes at the bottom have level `0` and their children are Data Nodes (Section 6.3).

---

# 6. BLOCKS AND NODES

## 6.1. Block

A block is exactly 512 bytes. It holds a contiguous array of entries starting at byte 0 of the block; the bytes after the last entry are zero.

There are two kinds of block:

| Kind             | Entries                 | Entry size | Max entries | Bytes used |
| ---------------- | ----------------------- | ---------: | ----------: | ---------: |
| Navigation block | Navigation Nodes        |   28 bytes |          18 |        504 |
| Leaf block       | Data Nodes              |   25 bytes |          20 |        500 |

The kind and the number of entries of a block are not stored in the block. They are given by the node that points to it: the Navigation Node's `v1` (level) selects the kind and `v2` (child count) gives the number of entries (Section 6.3). The kind and count of the root block follow from the SQT header: it is a Navigation block with `root_count` entries.

Every block holds at least one entry.

## 6.2. Navigation Node

A Navigation Node represents a spatial cluster and points to the block with its children.

Its binary size is **28 bytes** (unchanged from V3).

Binary layout:

```text
<IiiiiII
```

| Offset | Size | Type       | Description                                         |
| -----: | ---: | ---------- | --------------------------------------------------- |
| `0x00` |    4 | `uint32`   | `child_block` — absolute file offset of the block with the children |
| `0x04` |   16 | `int32[4]` | Cluster BBox `[xmin,ymin,xmax,ymax]`                |
| `0x14` |    4 | `uint32`   | `v1` — tree level                                   |
| `0x18` |    4 | `uint32`   | `v2` — number of children                           |

All BBox coordinates use the `10⁻⁷` degree representation.

The first field is the only difference from the V3 node: `v3_jump` (the byte size of the child subtree) is replaced by `child_block` (see Section 7).

## 6.3. Tree level and child count

For:

```text
v1 = 0
```

the node points to a **leaf block** with `v2` Data Nodes, `1 ≤ v2 ≤ 20`.

For:

```text
v1 > 0
```

the node points to a **Navigation block** with `v2` Navigation Nodes, `1 ≤ v2 ≤ 18`, each of level `v1 - 1`.

Thus the tree is homogeneous at every level: all entries of one block have the same kind, and all Navigation Nodes of one block have the same level.

The nodes of the root block have level `depth - 1`. Their `v1` therefore equals `depth - 1` for all of them.

---

# 7. `child_block`

`child_block` is the absolute file offset of the block that contains the children of the Navigation Node.

It is not relative to anything: the reader seeks to this offset and reads 512 bytes.

## 7.1. Constraints

For a Navigation Node stored in a block at offset `B`, in the section `[S, S + section_size)`:

1. `child_block` is a multiple of 512;
2. `S + 1024 ≤ child_block` and `child_block + 512 ≤ S + section_size`;
3. `child_block > B` — a child block always lies after the block that references it.

Constraint 3 makes the tree acyclic by construction: a parser that follows `child_block` pointers cannot loop, and the number of nested reads is bounded by `depth`.

A parser must reject a node that violates these constraints, or whose `v1` or `v2` is outside the limits of Section 6.3, or whose `v1` is not equal to the expected level.

## 7.2. Traversal

Culling needs no subtree skipping. A subtree is skipped by not reading its block:

```text
visit_block(offset, level, count):        # level = level of the entries (Nav) or -1 (leaf)
    read 512 bytes at offset
    for each entry e in the first count entries:
        if e.bbox does not intersect the viewport:
            continue                       # nothing is read for the subtree
        if leaf block:
            pass e to the renderer
        else:
            visit_block(e.child_block, e.v1 - 1, e.v2)    # e.v1 = 0 → leaf block with e.v2 entries
```

The root call is `visit_block(S + 512, depth - 1, root_count)`.

The reader needs no `seek` between the children of one block: after the block has been read, all BBoxes of its entries are in memory.

## 7.3. Physical order of blocks

The order of blocks inside a section is normative, so that parsers and tools can rely on it.

Blocks are grouped by the level of the nodes they contain:

```text
group 0: the root block                          (nodes of level depth-1)
group 1: Navigation blocks of nodes of level depth-2
...
group depth-1: Navigation blocks of nodes of level 0
group depth:   leaf blocks                       (Data Nodes)
```

Rules:

1. Groups appear in increasing order of the index above, one after another, without gaps. The groups of a tree with `depth = 1` are the root block and the leaf blocks.
2. Every block except the root block is referenced by exactly one Navigation Node, and every Navigation Node references exactly one block.
3. The blocks of a group are in the order of the nodes that reference them: the k-th block of group `g + 1` is referenced by the k-th Navigation Node of group `g`, where nodes are counted in the order of their blocks and, inside a block, in the order of their entries.
4. As a consequence of rule 3, the child blocks of the nodes of one block are consecutive: for entries `i` and `i + 1` of the same block, `child_block[i + 1] = child_block[i] + 512`. A reader may read the children of a block with one multi-sector request.
5. The order of the nodes inside a block is the order produced by the STR algorithm (Section 14). It has no meaning for the reader.
6. All bytes outside entries and headers (block tails, header sector tail) are zero.

The top levels of the tree are therefore at the start of the section, close to each other. The blocks of one level that belong to one subtree are adjacent.

## 7.4. `v3_jump` is removed

V4 does not contain `v3_jump`. There is no relative subtree skip, no subtree size and no jump compensation. Everything V3 said about "subtree skipping" is replaced by Section 7.2.

## 7.5. Example (informative)

A LOD with 50 Data Nodes (one layer of three clusters of 20, 20 and 10) and a section start `S = 0x200`:

```text
0x200  header sector   depth=1, root_count=3, block_size=512, section_size=0xA00
0x400  root block      3 Navigation Nodes, level 0:
                         child_block=0x600  v2=20
                         child_block=0x800  v2=20
                         child_block=0xA00  v2=10
0x600  leaf block      20 Data Nodes (500 bytes) + 12 zero bytes
0x800  leaf block      20 Data Nodes (500 bytes) + 12 zero bytes
0xA00  leaf block      10 Data Nodes (250 bytes) + 262 zero bytes
0xC00  end of the section (= next LOD offset or end of the file)
```

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

### 8.1.1. Code ranges

The `Type` field is `uint8`. The code space is split into fixed ranges, one per layer:

| Range       | Layer / purpose | Classes                                                       |
| ----------: | --------------- | ------------------------------------------------------------- |
|    `0..31`  | `roads`         | `NO_CLASS` (0), `ROAD_*`, `RAILWAY`                           |
|   `32..63`  | `waterways`     | `WATERWAY_MAJOR` (32), `WATERWAY_MINOR` (33)                  |
|   `64..95`  | `water`         | `WATER` (64), `WETLAND` (65), `GLACIER` (66)                  |
|  `96..127`  | `landuse`       | `LANDUSE_NATURAL` (96), `LANDUSE_HUMAN` (97), `SAND` (98)     |
| `128..191`  | `pois`          | `POI_BIG` (128), `POI_SMALL` (129), other POI classes (130..) |
| `192..223`  | UI icons        | icon numbers of `purrgo_poi_icons`, never stored in maps      |
| `224..255`  | reserved        | not used by any layer                                         |

Rules:

- A rule in `features.csv` must use a code from the range of its own `Layer`. The map compiler checks this (also for disabled rules) and stops with an error otherwise.
- A new class is added at the end of the range of its layer: the next free code after the highest one in use. A code that has appeared in compiled maps is never reassigned to another class.
- Several rules may share one code (one class, many OSM tags). Code `4` is an existing exception: it is used by both `ROAD_NORMAL` and `ROAD_MINOR` rules.
- Numbers `192..223` are used by the firmware for UI icons (target, waypoint, crosshair; later battery, antenna and similar). They are not feature codes: a map must not contain them, and the compiler never assigns them. A new UI icon takes the next free number in this range.
- Codes `224..255` are reserved for future needs (for example new layers). The firmware treats a code without a style as "do not draw".
- POI icons are indexed by the code itself, see 9.6.

A map compiled with the previous code assignment (`WATER` = 10, `LANDUSE_*` = 8/9, `POI_*` = 11..36, `WATERWAY_*` = 37/38, `WETLAND` = 39, `GLACIER` = 40, `SAND` = 41) is not compatible with the current firmware: water, landuse and POI would not be drawn. Such maps must be recompiled. Road and railway codes (`1..7`) did not change.

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
64;WATER;WATER_FILL;1;water;natural=water;Водоем;1;
32;WATERWAY_MAJOR;WATER_THICK_LINE;2;waterways;waterway=river;Река;1;
129;POI_SMALL;DARK_GRAY_CIRCLE;0;pois;amenity=pharmacy;Аптека;1;cross
```

The exact set of feature classes and styles is defined by the current `features.csv`.

## 9.2. Rule matching

Rules are evaluated strictly from top to bottom.

The **first matching enabled rule wins**.

Therefore more specific rules must precede more general rules.

Example:

```text
128;POI_BIG;...;place=city, capital=yes;Столица;1;circle
128;POI_BIG;...;place=city;Город;1;circle
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
WATERWAY_MAJOR
WATERWAY_MINOR
POI_BIG
POI_SMALL
```

The classes of the `waterways` layer (`WATERWAY_MAJOR`, `WATERWAY_MINOR`) are linear water objects. `WETLAND` and `GLACIER` are polygons of the `water` layer that the firmware draws with horizontal hatching (wetland) and a small snowflake pattern (glacier) instead of the solid black water fill. `SAND` is a polygon of the `landuse` layer (beach, sand, dune, shingle, scree) that the firmware draws as a dot pattern instead of the light gray fill. Their codes are listed in 8.1.1.

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
WATER_THICK_LINE
WATER_LINE
LIGHT_GRAY_FILL
DARK_GRAY_FILL
WATER_FILL
DARK_GRAY_CIRCLE
DARK_GRAY_BIG_CIRCLE
```

Style names are not stored as strings in `.idx`.

Water styles are black: `WATER_FILL` is a black polygon fill with a 1 px dark-gray contour; `WATER_THICK_LINE` is a plain 3 px black line (rivers, canals); `WATER_LINE` is a plain 1 px black line (streams). Water lines have no outline.

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

The firmware draws a POI with the 11x11 icon `purrgo_poi_icons[Type]` (`src/core/map/purrgo_poi_icons.c`): the icon number is the feature code, so POI icons occupy numbers `128..191`. UI icons (navigation target `192`, waypoint `193`, map centre crosshair `194`) use the range `192..223`. A new POI class gets the next free code in `128..191` and its icon is drawn under the same number.

In the circle mode the firmware draws `POI_BIG` as a large circle and every other POI code as a small one.

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

**Every POI Data Node has `v2 ≥ 1`.** The compiler writes one record per POI, including POIs without a name of their own (Section 10.6). `v2 = 0` is not produced for POIs; a reader must treat `v2 = 0` as "no record".

The V3 text said that unnamed POIs use `v2 = 0`. The V3 compiler never did that: it assigns a record to every POI. V4 aligns the text with the actual behaviour.

The `.db` has no coordinates and no back reference to the `.idx` node. The link in the opposite direction (record → position) exists only in `search.idx` (Section 21).

## 10.6. Name field

The `name` field of a record holds the object name in PurrGO-256 (`docs/font-encoding.md`), one byte per character, padded to 100 bytes. The compiler produces it as follows; the rules are normative for the compiler and are relied on by `search.idx`.

1. **Choice.** The first non-empty value of the tags `short_name:ru`, `name:ru`, `short_name:be`, `name:be`, `int_name`, `short_name:en`, `name:en`, `short_name`, `name` (in this order).
2. **Stop words.** If the name starts with a stop word (street, square, café, hotel and similar words in Russian, Belarusian and English; the list is in `purrgo_osmparser.py`), the word is moved to the end: `Кафе Пушкин` becomes `Пушкин кафе`. The first letter of the remaining part is upper-case, the moved word is lower-case.
3. **Spaces.** Every space is replaced by `_`.
4. **Length.** A name longer than 22 characters is cut to the first 20 characters (trailing `_` removed) followed by `..`. A name is never longer than 100 bytes.
5. **POI without a name.** A POI that has no name tag gets the value of the first tag of its `features.csv` rule as the name (`camp_site`, `clothes`), cut to 22 characters in the same way, without the stop-word rule.
6. **Barriers.** A POI of a rule with the tag `barrier=…` that has no name keeps an empty name.

Consequences that other parts of the format depend on:

* a stop word is usually at the **end** of the name, not at the beginning;
* a POI named by rule 5 cannot be told from a POI that really has this name by looking at the `.db`; `search.idx` marks such POIs with the flag `UNNAMED` (Section 21.4);
* names of the `roads`, `landuse` and `water` layers follow rules 1–4.

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
├── waterways.idx
├── waterways.mlp
├── waterways.db
├── roads.idx
├── roads.mlp
├── roads.db
├── pois.idx
├── pois.db
└── search.idx        (optional, Section 21)
```

Layer files that contain no data may be omitted according to the layer rules defined elsewhere in this specification. The `map.name` file itself is mandatory.

### 11.4.1. Optional layers and compatibility

The `waterways` layer is optional. The compiler writes `waterways.idx`, `waterways.mlp` and `waterways.db` only if the source data contains at least one enabled linear water object; no empty dummy files are created for it.

Adding a new layer does not change the binary format: the PGO container, `.idx`, `.mlp` and `.db` structures are the same, and the format version stays V4. A reader opens layer files by name and must ignore layer files it does not use. A package with `waterways.*` is therefore valid for a reader that does not know this layer, and a package without it is valid for a reader that does.

`search.idx` is optional in the same way: the compiler writes it only if the search index is requested, a reader that does not know it ignores it, and a reader that knows it works without it (only the simple search is then available).

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
| `waterways` | line geometry only  |
| `pois`    | point geometry only   |

The firmware may rely on this invariant and does not need to determine geometry type dynamically.

Linear water objects (rivers, streams, canals) are therefore stored in the separate `waterways` layer and never in `water`. `waterways` uses the same geometry record as `roads`: an open polyline with a single part. Ring orientation and closure rules for polygons do not apply to it. Like `roads`, the layer is not labelled, and its `.db` may be omitted if it contains no named objects (Section 10.4).

A violation indicates an invalid map produced by the compiler.

---

# 14. STR INDEX CONSTRUCTION

The `.idx` spatial index is generated using the Sort-Tile-Recursive (STR) bulk-loading algorithm and then serialized into blocks.

Conceptually:

1. group features by LOD;
2. sort features by X centroid;
3. divide them into spatial slices;
4. sort each slice by Y centroid;
5. divide objects into clusters;
6. generate Navigation Nodes;
7. calculate enclosing BBoxes;
8. repeat steps 2–7 for the Navigation Nodes of the previous level until the layer has at most `nav_fanout` nodes; this layer is the root block;
9. assign blocks and compute `child_block` (Section 14.2);
10. serialize the header sector and the blocks of the LOD section.

## 14.1. Fan-out

The fan-out is chosen so that a full block fits one sector:

```text
Navigation block:   18 × 28 bytes = 504 bytes   (nav_fanout  = 18)
Leaf block:         20 × 25 bytes = 500 bytes   (leaf_fanout = 20)
```

A cluster of Data Nodes (level 0) has at most 20 children. A cluster of Navigation Nodes (level ≥ 1) has at most 18 children. The root layer has at most 18 nodes.

In the compiler this replaces the single constant `CHUNK_SIZE = 14` of V3 by two constants.

## 14.2. Block assignment

After the tree is built, the compiler:

1. orders the nodes of every level in the order of the STR slices (the order in which they were created);
2. gives the root block the offset `S + 512`;
3. assigns the offsets of the remaining blocks group by group in the order of Section 7.3, each block 512 bytes;
4. writes `child_block` into every Navigation Node;
5. fills the unused bytes of every block with zeros.

An equal LOD section cannot be copied as bytes from another LOD, as the V3 compiler did for LODs with the same object count, because the offsets inside a section depend on its position in the file. Each LOD is built or relocated separately.

## 14.3. Size (informative)

A block is padded to 512 bytes even if it is not full, so `.idx` grows compared with V3. The design estimate is a factor of 1.3–1.5 (for Belarus about +25–30 MB). The estimate is to be verified with `sim_map_io.py` on the map built in CI.

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

Optionally, after `pois.db` and `pois.idx` have been compiled (so that `v2` of every POI is known), the compiler writes `search.idx` (Section 21): it assigns subtypes by `search_categories.csv`, cuts the names into words, and writes the NAME and CAT sections and the dictionary.

---

# 16. FIRMWARE PARSING MODEL

The firmware does not parse OSM tags.

It operates on:

```text
.idx
.mlp
.db
```

and the corresponding numeric PurrGO feature definitions. If `search.idx` is present, the search code additionally uses it (Section 21).

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

For every block the parser reads 512 bytes with one request and tests the BBoxes of all entries from RAM. A Navigation Node whose BBox does not intersect the current viewport is skipped by not reading its `child_block`. A Data Node that passes the spatial test is passed to the renderer.

A parser should keep one 512-byte buffer per nesting level (at most `depth` buffers) or, when stack space is short, copy only the entries that pass the test before descending.

A transitional firmware reads V3 and V4 `.idx` files and chooses the traversal by the SQT magic (Section 0.3).

---

# 17. BINARY FORMAT INVARIANTS

The following properties are normative for V4:

1. All PGO containers have a 32-byte header.
2. The PGO magic is exactly `PGO`.
3. `.idx`, `.mlp`, `.db` and `search.idx` use file types `1`, `2`, `3` and `4`.
4. All multi-byte integers are Little-Endian unless explicitly specified otherwise.
5. `.mlp` geometry record sequence numbers are Big-Endian.
6. Coordinates use signed `int32` with `10⁻⁷` degree precision.
7. `.idx` Data Nodes are exactly 25 bytes.
8. `.idx` Navigation Nodes are exactly 28 bytes.
9. `v1` in a normal Data Node points to the Geometry Body in `.mlp`, relative to the beginning of the `.mlp` payload.
10. The `.mlp` Local Header is not included in the `v1` target.
11. SQT sections start with the magic `SQT\x02`, topology `2` and a 512-byte header sector.
12. LOD offsets, section sizes and blocks are multiples of 512; the first LOD offset is `0x200`.
13. A block holds at most 18 Navigation Nodes or 20 Data Nodes; the unused bytes of a block are zero.
14. `child_block` is the absolute offset of the block with the children, and `child_block` is greater than the offset of the block that holds the node.
15. Blocks are stored in the order of Section 7.3; the child blocks of the nodes of one block are consecutive.
16. Empty LODs still contain their 512-byte SQT header sector.
17. POIs do not use `.mlp` geometry records.
18. POI coordinates are represented by a zero-area Data Node BBox.
19. Every POI Data Node has `v2 ≥ 1`.
20. OSM rule matching uses first-match-wins semantics.
21. Disabled `features.csv` rules are ignored.
22. OSM tags and textual feature definitions are not stored in `.idx`.
23. Geometry type is determined by the target layer.
24. Reserved PGO header fields and reserved SQT header bytes have no current runtime semantics and are zero.

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

For the optional search index:

```text
search.idx
 │
 ├── record.db_idx ───────→ pois.db record (= v2 of the POI Data Node in pois.idx)
 ├── record.lon, lat ─────→ POI point (= BBox of the POI Data Node)
 └── record.code ─────────→ POI feature definition (= Type of the Data Node)
```

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

This document defines **PurrGO Map Format V4**.

The previous version, V3, is defined in [`map-format-v3.md`](map-format-v3.md).

Changes to any of the following constitute a binary-format change and require corresponding updates to both compiler and firmware:

* PGO header;
* SQT header;
* block layout, block order and fan-out;
* Navigation Node layout;
* Data Node layout;
* Geometry Record layout;
* `.db` layout;
* meaning of `v1`, `v2` or `child_block`;
* coordinate representation;
* LOD representation;
* feature-code semantics;
* `search.idx` header, record layout, key normalization or section order.

## 20.1. Version identification

The `.idx` version is identified by the SQT magic: `SQT\x01` is V3, `SQT\x02` is V4. The PGO header carries no version.

`search.idx` has its own version field (Section 21.3). A new `search.idx` version does not change the V4 version of the `.idx` files and the other layer files.

Adding a layer does not change the version (Section 11.4.1).

---

# 21. SEARCH INDEX (`search.idx`)

## 21.1. Purpose and status

`search.idx` makes it possible to find POIs by a part of the name and by a category word ("кафе", "hotel") without reading the whole `pois.db`. Without it the name and the position of a POI cannot be linked cheaply: `pois.db` has no coordinates and `pois.idx` has no names.

The file is **optional**:

* no file → only the simple search (category by feature code, nearest objects) is available; it needs only `pois.idx` and `pois.db`;
* a file that fails validation (Section 21.10) is ignored as if it did not exist; this is never a fatal error of the map.

`search.idx` is a package-level file next to `pois.idx` and `pois.db` (Section 11.4). It is written by the compiler in the same run as `pois.*`.

## 21.2. File layout

```text
+------------------------------+ 0x000
| PGO Header (32 bytes)        |   File Type = 4
+------------------------------+ 0x020
| SRCH header (64 bytes)       |
+------------------------------+ 0x060   ← dict_offset
| Dictionary                   |
+------------------------------+
| zero padding to 512          |
+------------------------------+         ← name_offset (multiple of 512)
| NAME section                 |
| (zero padding to 512)        |
+------------------------------+         ← cat_offset  (multiple of 512)
| CAT section                  |
| (zero padding to 512)        |
+------------------------------+ end of file
```

In the PGO header `File Type` is `4`; the three LOD offset fields and the reserved fields are zero. `Payload Size` is the file size minus 32.

Both sections start at a multiple of 512 and are padded with zeros to a multiple of 512, so a binary search touches whole SD sectors (16 records per sector). A section with zero records has `count = 0` and its offset points to the end of the preceding data (still a multiple of 512, not beyond the end of the file).

## 21.3. SRCH header

The header begins at file offset `0x20`. Offsets in the table are relative to this position.

| Offset | Size | Type      | Description                                                                 |
| -----: | ---: | --------- | --------------------------------------------------------------------------- |
| `0x00` |    4 | `char[4]` | Magic: `SRCH`                                                               |
| `0x04` |    2 | `uint16`  | Version: `1`                                                                |
| `0x06` |    2 | `uint16`  | Record size: `32`                                                           |
| `0x08` |    4 | `uint32`  | Flags: `0`                                                                  |
| `0x0C` |    4 | `uint32`  | `db_records` — number of records in `pois.db` this index was built for     |
| `0x10` |    4 | `uint32`  | `name_offset` — absolute file offset of the NAME section                    |
| `0x14` |    4 | `uint32`  | `name_count` — number of records in NAME                                    |
| `0x18` |    4 | `uint32`  | `cat_offset` — absolute file offset of the CAT section                      |
| `0x1C` |    4 | `uint32`  | `cat_count` — number of records in CAT                                      |
| `0x20` |    4 | `uint32`  | `dict_offset` — absolute file offset of the dictionary: `0x60`              |
| `0x24` |    4 | `uint32`  | `dict_size` — size of the dictionary in bytes                               |
| `0x28` |    4 | `uint32`  | `tile_offset` — reserved for the tile catalog; `0` in version 1             |
| `0x2C` |    4 | `uint32`  | `tile_count` — reserved for the tile catalog; `0` in version 1              |
| `0x30` |   16 | `uint8[]` | Reserved; must be zero                                                      |

The size of the header is 64 bytes. A reader that finds `Version ≠ 1` or `Record size ≠ 32` ignores the file.

## 21.4. Record

Both sections use the same 32-byte record. Binary layout:

```text
<iiIBBBB16s
```

| Offset | Size | Type       | Description                                                       |
| -----: | ---: | ---------- | ----------------------------------------------------------------- |
| `0x00` |    4 | `int32`    | `lon` — longitude, `10⁻⁷` degrees                                  |
| `0x04` |    4 | `int32`    | `lat` — latitude, `10⁻⁷` degrees                                   |
| `0x08` |    4 | `uint32`   | `db_idx` — POI record number in `pois.db` (the `v2` of the POI Data Node) |
| `0x0C` |    1 | `uint8`    | `code` — feature code `Type` of the POI (Section 8.1.1, range `128..191`) |
| `0x0D` |    1 | `uint8`    | `subtype` — search subtype (Section 21.8); `0` = none                |
| `0x0E` |    1 | `uint8`    | `flags`                                                            |
| `0x0F` |    1 | `uint8`    | `key_len` — number of significant bytes of `key`, `0..16`           |
| `0x10` |   16 | `uint8[16]`| `key` — normalized key (Section 21.7), padded with zero bytes       |

`lon`, `lat`, `db_idx` and `code` are copies of the BBox, `v2` and `Type` of the POI Data Node in `pois.idx`.

`flags`:

| Bit | Name        | Meaning                                                                    |
| --: | ----------- | -------------------------------------------------------------------------- |
|   0 | `SECONDARY` | NAME only: the key is a word of the name other than the first one          |
|   1 | `UNNAMED`   | CAT only: the POI has no name of its own (Section 10.6, rules 5 and 6)     |
|   2 | `KEY_TRUNC` | the normalized word was longer than 16 bytes; `key` holds the first 16     |
| 3–7 | —           | reserved; must be zero                                                     |

The key comparison used for ordering and for searching is a bytewise unsigned comparison of the 16 `key` bytes (`memcmp`), regardless of `key_len`. Because keys contain no zero byte (Section 21.7), a shorter key sorts before every longer key that starts with it.

## 21.5. NAME section

NAME is the index of POI names by word.

**Content.** For every named POI (a name that is not substituted by rule 5 of Section 10.6 and not empty) the compiler writes:

* one **primary** record for the first word of the name (`SECONDARY = 0`);
* one **secondary** record (`SECONDARY = 1`) for every further word, up to 3 secondary records per POI.

Words are cut out of the name as described in Section 21.7. A secondary word shorter than 2 bytes after normalization is not indexed. A word whose key equals the key of an earlier word of the same POI is not indexed twice.

All records of one POI have the same `lon`, `lat`, `db_idx`, `code` and `subtype`. `subtype` is `0` if no rule of `search_categories.csv` matches the POI.

**Order.** Records are sorted by `key` (Section 21.4), then by `db_idx`, then primary before secondary.

**Use.** All names that contain a word starting with a prefix `p` (`1 ≤ |p| ≤ 16`) form one contiguous range of records: the first record with `key ≥ p` padded with `0x00` up to the first record with `key > p` padded with `0xFF`. A POI may occur in the range twice (two words with the same prefix); the reader merges duplicates by `db_idx`.

Because the stop word of a name is stored at its end (Section 10.6, rule 2), it is normally a secondary record: the name `Пушкин кафе` is found by `пуш` (primary record) and by `кафе` (secondary record).

The compiler may omit POI classes that are not meaningful search targets (for example barriers, code `152`). A reader must not assume that every POI of `pois.idx` occurs in `search.idx`.

## 21.6. CAT section

CAT is the index of POIs by category (subtype).

**Content.** One record for every searchable POI with `subtype ≠ 0`, including unnamed POIs (`UNNAMED = 1`, `key_len = 0`). For a named POI `key` is the key of its first word (Section 21.7); `SECONDARY` is `0`.

**Order.** Records are sorted by `subtype`, then by the Morton code of the point, then by `db_idx`.

The Morton code is a 64-bit value computed from the point as:

```text
X = lon + 1 800 000 000      (as uint32)
Y = lat +   900 000 000      (as uint32)
morton = interleave bits of X and Y: bit i of X → bit 2·i, bit i of Y → bit 2·i + 1
```

The code is not stored; the reader computes it from `lon` and `lat` of a record when it needs it.

**Use.** The records of one subtype are contiguous, so a category query reads one run of records. Inside the run the Morton order keeps objects that are close to each other close in the file, so a query for the objects near a point can restrict the run by a binary search on the Morton code of a bounding square and then filter by distance. Whether a tile catalog (`tile_offset`) is needed on top of this is decided after measuring on a real map (Appendix A).

## 21.7. Key normalization

The key is computed from bytes in PurrGO-256 (`docs/font-encoding.md`), the same encoding as the `name` field of `pois.db`. The compiler and the firmware must use the same function; it is defined by a 256-byte table generated from the font table and checked in the same way as `tests/check_pgo_text_table.py`.

**Folding.** Every byte `b` of the name is replaced:

| Input bytes     | Result                  | Meaning                                    |
| --------------- | ----------------------- | ------------------------------------------ |
| `0x41..0x5A`    | `b + 0x20`              | Latin upper case → lower case              |
| `0x80..0x9F`    | `b + 0x20`              | Cyrillic upper case → lower case           |
| `0xC0..0xCF`    | `b + 0x10`              | extended Cyrillic upper case → lower case  |
| `0xF0`, `0xF1`  | `0xA5`                  | `Ё`, `ё` → `е`                             |

**Word bytes.** After folding, a byte is a *word byte* if it is in one of the ranges

```text
0x30..0x39    digits
0x61..0x7A    Latin letters
0xA0..0xBF    Cyrillic letters
0xD0..0xDF    extended Cyrillic letters
0xE0..0xEF    Latin Extended glyph codes
```

Every other byte (control codes, space, `_`, `-`, `(`, `)`, `.`, `,`, quotes, `0xF2..0xFF` and so on) is a **separator**.

**Words.** The name is cut into words at the separators; empty words are dropped. A word is at most 16 bytes long for the key: a longer word is cut to its first 16 bytes and `KEY_TRUNC` is set. The first word is the primary word.

**Alphabet of keys.** A key contains only word bytes, so it never contains `0x00` or `0xFF`.

Latin letters with diacritics are already merged by PurrGO-256 into one glyph code (`Č Ć Ç` → `0xE4`); the key does not distinguish them. The trailing `..` of a shortened name (Section 10.6, rule 4) consists of separators and disappears.

The same normalization is applied to the text typed by the user. `Ё` and `Е` are not distinguished in a query.

## 21.8. Subtypes and the dictionary

A **subtype** is a finer class than the feature code: `tourism=hostel` and `tourism=hotel` have the same code but different subtypes. Subtype numbers are `1..255`; `0` means "none". A subtype number that has appeared in compiled maps is never assigned to another class.

### 21.8.1. `search_categories.csv`

The file is an input of the compiler (like `features.csv`) and is not part of the map package. It is placed in `tools/map-compiler/`.

```text
Group;Subtype;Keywords_RU;Keywords_EN;OSM_Tags
```

| Field         | Description                                                                          |
| ------------- | ------------------------------------------------------------------------------------ |
| `Group`       | identifier of a set of subtypes (for example lodging: hotel, hostel, camp site, guest house, shelter) |
| `Subtype`     | subtype number `1..255`; empty in a group row                                         |
| `Keywords_RU` | category words in Russian, separated by commas                                        |
| `Keywords_EN` | category words in English, separated by commas                                        |
| `OSM_Tags`    | the same expression as `OSM_Tags` in `features.csv` (`tourism=hostel`); empty in a group row |

A row with `Subtype` filled assigns the subtype to POIs and gives the keywords of that subtype. A **group row** (`Subtype` and `OSM_Tags` empty) gives keywords that stand for all subtypes of the group.

The compiler assigns a subtype to a POI by the **first matching row** from top to bottom, evaluated on the OSM tags of the object. A POI that matches no row has `subtype = 0`.

Rules with `Enabled = 0` in `features.csv` produce no POI in the map and therefore cannot be found. Searching objects that are not drawn needs a separate mechanism (Section 22).

### 21.8.2. Dictionary

The dictionary lists the keywords and the subtypes each keyword stands for. It is stored at `dict_offset`, so the vocabulary can be extended by recompiling the map, without changing the firmware.

| Offset | Size | Type     | Description                                |
| -----: | ---: | -------- | ------------------------------------------ |
| `0x00` |    2 | `uint16` | Number of keywords `n`                     |
| `0x02` |    2 | `uint16` | Entry size: `48`                           |
| `0x04` |    4 | `uint32` | Reserved; must be zero                     |

The header is followed by `n` entries of 48 bytes, so `dict_size = 8 + 48 · n`:

| Offset | Size | Type        | Description                                                        |
| -----: | ---: | ----------- | ------------------------------------------------------------------ |
| `0x00` |   16 | `uint8[16]` | normalized keyword (one word, Section 21.7), padded with zero bytes |
| `0x10` |   32 | `uint8[32]` | subtype mask: bit `s mod 8` of byte `s div 8` is set for subtype `s` |

Entries are sorted by keyword (bytewise, as keys) and the keywords are unique: if the same normalized word occurs in several rows (also in Russian and English lists), its masks are combined with OR. The mask of a keyword from a group row contains all subtypes of the group. Bit 0 is zero. A keyword longer than 16 bytes is not allowed.

## 21.9. Query semantics (informative)

The format is designed for the following use; the exact UI is described in `design/poi-search.md`.

1. The text is normalized and cut into words (Section 21.7).
2. A word is a **category word** if it equals a dictionary keyword or is a prefix of a keyword of at least 3 bytes. The subtype masks of all matching keywords are combined with OR. All other words are **name prefixes**.
3. Only category words → read the runs of the CAT section for the subtypes of the mask.
4. At least one name prefix → the first prefix selects a range in NAME (Section 21.5); the records are filtered by the subtype mask if there is one, and the remaining name prefixes are checked against the full name from `pois.db` of the candidates. A query of two parts is an AND: "кафе пуш" is the category "кафе" and a name word starting with "пуш".
5. The result is the nearest objects: duplicates by `db_idx` are merged, and the top N by distance are kept in a fixed-size array.
6. If a new query extends the previous one, the search range of the previous query can be narrowed instead of started again.

A prefix of one byte may select a large part of the NAME section; limiting the radius or requiring two bytes is a firmware decision (Appendix A).

## 21.10. Validation

A reader validates `search.idx` before use and ignores the file if any check fails:

1. the PGO header is valid, `File Type = 4`, and the payload fits into the file;
2. `Magic = SRCH`, `Version = 1`, `Record size = 32`;
3. `name_offset`, `cat_offset` and `dict_offset` are inside the file, `name_offset` and `cat_offset` are multiples of 512, and the sections lie after the dictionary, in the order dictionary, NAME, CAT, without overlap;
4. `name_offset + 32 · name_count` and `cat_offset + 32 · cat_count` do not exceed the file size;
5. `db_records` is equal to the number of records in `pois.db`, so an index built for another `pois.db` is not used;
6. `tile_offset = 0` and `tile_count = 0`;
7. `dict_size = 8 + entry_size · n` and the dictionary fits into the file.

A reader that reads records one by one additionally rejects a record with `db_idx = 0` or `db_idx > db_records`, `key_len > 16`, or reserved flag bits set.

The map audit (`tests/audit_maps.py`) additionally checks the order of both sections, the uniqueness and order of dictionary entries, that every `db_idx` occurs in `pois.idx` with the same coordinates and code, and that the key of every record can be recomputed from its `pois.db` record.

## 21.11. Size (informative)

```text
NAME   = named POI count × 32 bytes × ≈1.7   (one primary and on average ≈0.7 secondary records)
CAT    = POI count with subtype ≠ 0 × 32 bytes
dict   = 8 + 48 × keyword count
```

The sizes for the Belarus map are to be measured on the map built in CI. They decide whether the record is kept at 32 bytes (Appendix A).

---

# 22. EXTENSION POINTS

The following extensions are not part of V4. The format leaves room for them, which means that they can be added without changing the V4 version of the `.idx` files:

* **Tile catalog** in `search.idx`: the header fields `tile_offset` and `tile_count` (Section 21.3). It would list Morton ranges of a subtype, so that a category query reads only the nearby tiles.
* **POI for search only**: POIs that are not drawn on the map but can be found (a `features.csv` column, a separate marker in `search.idx`).
* **Transliteration** (`kafe` → `Кафе`): can be done in the reader by additional keys in NAME, or by a mapping before the query; the key table of Section 21.7 is then extended.
* **Streets from `roads.db`**: a further section in `search.idx` for road names; needs a back reference from a road name to a geometry.
* **Shared LOD sections**: two LOD offsets pointing to the same section when the LODs are equal. A V4 reader that computes the end of a section from the next LOD offset does not support this; the field `section_size` of the header sector is reserved for this purpose.
* **Block packing**: storing several small blocks in one sector. Rule 4 of Section 7.3 (consecutive child blocks) excludes it in V4.

---

# APPENDIX A. OPEN POINTS OF THE DRAFT

This appendix is removed when the points are decided.

1. **SQT header.** `docs/design/pgo-v4.md` changes only the magic. This draft also extends the header to a 512-byte sector with block size, section size and fan-out limits (Section 5.1), so that every block address stays aligned and the header is validated against the real section extent.
2. **Alignment of `.idx`.** The LOD offsets are multiples of 512 and the first LOD is at `0x200`; 480 bytes after the PGO header are padding (Section 4.1).
3. **Consecutive child blocks** (Section 7.3, rule 4) are normative in this draft. They allow a multi-sector read of the children of a block but exclude block packing and sharing.
4. **Fan-out** 18 (Navigation) and 20 (leaf), as in the design document. An underfull block costs a whole sector.
5. **Check in the firmware:** the V3 text said "unnamed POI → `v2 = 0`" while the compiler wrote a record for every POI. V4 follows the compiler (Section 10.5). Check that no firmware code depends on `v2 = 0` for POIs.
6. **Feature codes in `design/poi-search.md`.** The category table of the simple search uses the codes `11..36`, which were replaced by `128..167` in the current `features.csv` (Section 8.1.1). The table must be redone with the current codes before the simple search is implemented. `search.idx` stores the current `Type`.
7. **Search index parameters** decided after measurement: record size 32 or 16 bytes, tile catalog, minimal prefix length of one byte.
8. **`search_categories.csv`:** the group-row convention and the 256-bit subtype mask of a keyword (Section 21.8) are proposals of this draft.
9. **Unnamed POIs.** The flag `UNNAMED` is set for both kinds of unnamed POI (substituted tag value and empty name); the empty ones are barriers and are not searched.
