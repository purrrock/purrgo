# PurrGo Map Format

**Status:** Working format specification for the PurrGo map subsystem  
**Reference:** `docs/dtg1_map_specification.md`  
**Reference implementation:** `tools/map-parser/dtmap-parser.py`  
**Current C implementation:** `src/core/map.c` / `include/purrgo/map.h`

---

## 1. Purpose and status

This document defines the map format as it is currently understood and used by the PurrGo map subsystem.

The normative reverse-engineering specification of the original DT G1 format is kept separately in:

```text
docs/dtg1_map_specification.md
```

That file is the immutable reference for the original format.

This file is intentionally different. It describes the format **as PurrGo currently understands it** and is the document that may be changed later when the map format is adapted for the STM32 target.

The current PurrGo format is therefore divided conceptually into:

```text
Original DT G1 format
        |
        | documented by
        v
docs/dtg1_map_specification.md
        |
        | interpreted by PurrGo
        v
PurrGo map format
        |
        | future STM32 adaptation
        v
PurrGo STM32 map format
```

No STM32-specific changes are defined by this document yet.

---

## 2. Sources of information

The current format description is based on three repository sources:

1. `docs/dtg1_map_specification.md`
   - normative description of the reverse-engineered DT G1 binary format;
2. `tools/map-parser/dtmap-parser.py`
   - reference Python parser/renderer;
3. `src/core/map.c` and `include/purrgo/map.h`
   - current PurrGo C implementation and public API.

Where the sources differ, this document records the difference explicitly instead of silently choosing one interpretation.

---

# 3. File types

The map system currently uses the following files:

| File | Purpose |
|---|---|
| `.idx` | Spatial index and object nodes |
| `.mlp` | Geometry records |
| `.db` | Attribute/name database |
| `pois.idx` | POI spatial index with special behavior |

The current Python renderer directly consumes `.idx` and `.mlp`.

The `.db` structure is specified by the DT G1 reference document, but the current PurrGo map renderer does not yet use it.

---

# 4. Global YZL header

Every binary map layer begins with a fixed 32-byte YZL header.

```text
Offset   Size   Type       Description
------   ----   --------   ------------------------------
0x00     3      char[3]    Magic: "YZL"
0x03     1      uint8      File magic extension
0x04     4      uint32     Payload size, little-endian
0x08     4      uint32     RAM load type
0x0C     4      uint32     LOD 2 section size, big-endian
0x10     16     byte[16]   MD5 checksum
```

Total size:

```text
32 bytes
```

## 4.1 Magic

The first three bytes must be:

```text
YZL
```

Hexadecimal:

```text
59 5A 4C
```

The Python parser validates this field.

## 4.2 Payload size

Offset:

```text
0x04
```

Size:

```text
uint32
```

Endian:

```text
little-endian
```

The reference specification defines this as:

```text
file size - 32
```

The current Python parser reads the field but does not use it to drive parsing.

## 4.3 Remaining YZL fields

The original DT G1 specification defines:

- file magic extension at `0x03`;
- RAM load type at `0x08`;
- LOD 2 section size at `0x0C`;
- MD5 checksum at `0x10`.

The current PurrGo Python parser only validates the YZL magic and reads the payload size.

Therefore these fields must not currently be treated as required by the PurrGo renderer unless the relevant functionality is explicitly implemented.

---

# 5. `.idx` structure

The `.idx` file contains spatial indexing information.

After the 32-byte YZL header, the file contains one or more SQT sections.

Conceptually:

```text
+------------------------+
| YZL header (32 bytes)  |
+------------------------+
| SQT header             |
+------------------------+
| root nodes             |
+------------------------+
| SQT header             |
+------------------------+
| root nodes             |
+------------------------+
| ...                    |
+------------------------+
```

The current Python parser processes SQT sections until EOF.

---

# 6. SQT section header

Each SQT section has a fixed 16-byte header.

Binary layout:

```text
<4sIII
```

Therefore:

```text
Offset   Size   Type       Description
------   ----   --------   ------------------------------
0x00     4      char[4]    SQT magic
0x04     4      uint32     Topology marker
0x08     4      uint32     Mode
0x0C     4      uint32     Root node count
```

All four fields are read by the Python parser as little-endian.

## 6.1 Magic

The expected value is:

```text
SQT\x01
```

Hexadecimal:

```text
53 51 54 01
```

The Python parser rejects a section with another magic value.

## 6.2 Topology marker

The Python parser reads this field but does not currently use it for normal layer rendering.

The DT G1 reference specification defines special POI behavior associated with topology marker `0x00000001`.

PurrGo should therefore preserve this field in the format description even though the current generic renderer does not interpret it.

## 6.3 Mode

The Python parser uses `mode` to determine whether root nodes are navigation nodes.

Current interpretation:

```text
mode == 0
    root nodes are Data Nodes

mode > 0
    root nodes are Nav Nodes
```

For navigation mode:

```text
initial_level = mode - 1
```

## 6.4 Root node count

`count` specifies the number of root nodes immediately following the SQT header.

The parser processes exactly that many nodes.

---

# 7. Unified 28-byte node

Both Data Nodes and Navigation Nodes occupy exactly 28 bytes.

The node is a union-like structure.

```text
+----------------------+ 0x00
| payload              | 20 bytes
+----------------------+ 0x14
| v1                   | 4 bytes
+----------------------+ 0x18
| v2                   | 4 bytes
+----------------------+ 0x1C
```

Total:

```text
28 bytes
```

The meaning of the first 20 bytes depends on node type.

---

# 8. Data Node

A Data Node contains a cartographic object reference.

Python unpacking format:

```text
<ffffIII
```

Layout:

```text
Offset   Size   Type       Description
------   ----   --------   ------------------------------
0x00     4      float      xmin
0x04     4      float      ymin
0x08     4      float      xmax
0x0C     4      float      ymax
0x10     4      uint32     object type
0x14     4      uint32     v1
0x18     4      uint32     v2
```

## 8.1 Bounding box

The first four fields form the object bounding box:

```text
xmin
ymin
xmax
ymax
```

They are IEEE-754 single-precision floating-point values stored little-endian.

The Python renderer performs AABB intersection against the camera bounding box.

## 8.2 Object type

Offset:

```text
0x10
```

The DT G1 specification identifies this as the cartographic/object class code.

The current renderer reads the value but does not use it to select a visual style.

Therefore PurrGo currently preserves the value but does not define a rendering table for object types.

## 8.3 `v1` — MLP geometry offset

For ordinary vector layers, `v1` references geometry in the corresponding `.mlp` file.

The current Python parser interprets it as an offset relative to the beginning of the payload after the 32-byte YZL header:

```text
absolute_body_offset = 32 + v1
```

The reference specification states that `v1` points to the geometry body and skips the 8-byte local MLP record header.

This interpretation is important and must be preserved when implementing the C reader.

## 8.4 `v2` — database record index

The DT G1 specification defines `v2` as an index into the `.db` attribute database.

The current map renderer does not use it.

For PurrGo, `v2` is therefore currently metadata associated with the object and not part of geometry rendering.

---

# 9. Navigation Node

A Navigation Node represents a spatial cluster / hierarchical R-tree node.

Python unpacking format:

```text
<IffffII
```

Layout:

```text
Offset   Size   Type       Description
------   ----   --------   ------------------------------
0x00     4      uint32     v3_jump
0x04     4      float      xmin
0x08     4      float      ymin
0x0C     4      float      xmax
0x10     4      float      ymax
0x14     4      uint32     nav_level / tree depth
0x18     4      uint32     child count
```

Total:

```text
28 bytes
```

## 9.1 `v3_jump`

`v3_jump` is an early-exit jump used when the entire subtree is outside the current camera BBox.

The original format specifies that the jump includes an 8-byte prefetch compensation.

The Python parser therefore performs:

```text
seek(v3_jump - 8, SEEK_CUR)
```

after reading the current 28-byte node.

This is a critical part of the spatial traversal algorithm.

### Required behavior

When a Nav Node is culled:

```text
1. Read the 28-byte node.
2. Determine that its BBox does not intersect the camera BBox.
3. Advance over the complete child subtree.
4. Apply the required -8-byte compensation.
5. Continue with the next node after the subtree.
```

The current C implementation does not yet perform this seek.

---

# 10. Navigation node BBox

The four float fields at offsets `0x04..0x13` form the bounding box of the complete child cluster.

They are used for AABB culling before descending into the subtree.

Intersection rule used by the Python parser:

```text
if xmax < camera_min_x
    outside

if xmin > camera_max_x
    outside

if ymax < camera_min_y
    outside

if ymin > camera_max_y
    outside
```

Otherwise the node is considered visible and its children are traversed.

---

# 11. Navigation tree depth

The field at offset `0x14` is the navigation/tree level.

Current Python interpretation:

```text
nav_level > 0
    children are Navigation Nodes

nav_level == 0
    children are Data Nodes
```

When descending:

```text
child_level = nav_level - 1
```

for a navigation node.

This corresponds to the `v1` field of the unified node layout.

---

# 12. Navigation child count

The field at offset `0x18` specifies the number of immediate child nodes contained by the current navigation node.

The parser loops exactly `obj_count` times.

No assumption should be made that the number of children is always a particular value.

The reference specification notes a maximum cluster size of 14 data objects, but the parser itself treats the stored count as authoritative.

---

# 13. Spatial traversal algorithm

The current reference algorithm is:

```text
read SQT header

determine root node type from mode

for every root node:
    parse node

Data Node:
    test object BBox
    if visible:
        read geometry from MLP

Nav Node:
    test cluster BBox
    if outside:
        skip subtree using v3_jump - 8
    else:
        determine child node type from nav_level
        recursively parse obj_count children
```

This is the current PurrGo reference traversal model.

---

# 14. `.mlp` geometry

The `.mlp` file stores the geometry referenced by Data Nodes.

After the 32-byte YZL header, geometry records are stored sequentially.

Each geometry record has an 8-byte local header:

```text
Offset   Size   Type       Description
------   ----   --------   ------------------------------
0x00     4      uint32     Sequence number, big-endian
0x04     4      uint32     Body size, little-endian
```

The geometry body follows immediately after this header.

However, a Data Node `v1` points directly to the geometry body, not to this 8-byte local header.

---

# 15. MLP geometry body

The fixed part of a geometry body is 24 bytes.

Python unpacking:

```text
<iiiiii
```

Layout:

```text
Offset   Size   Type       Description
------   ----   --------   ------------------------------
0x00     4      int32      minx
0x04     4      int32      miny
0x08     4      int32      maxx
0x0C     4      int32      maxy
0x10     4      int32      num_parts
0x14     4      int32      num_points
```

The geometry body then contains:

```text
parts[]
points[]
```

---

# 16. MLP geometry BBox

The first four `int32` values form the geometry bounding box.

The DT G1 reference specifies these as integer coordinates.

The current Python renderer reads them but does not use them for projection/culling after the Data Node has already passed the AABB test.

---

# 17. `num_parts`

`num_parts` is the number of segments/rings stored in the geometry object.

It determines the size of the `parts` array:

```text
parts_size = num_parts * 4
```

A negative value is rejected by the current Python implementation.

---

# 18. `num_points`

`num_points` is the total number of vertices stored in the geometry.

Each point contains two signed 32-bit integers:

```text
X
Y
```

Therefore:

```text
points_size = num_points * 8
```

The current Python parser additionally rejects values above 50000 as a corruption/memory-safety guard.

That `50000` limit is a parser implementation limit, not a statement that the original file format defines a maximum of 50000 points.

---

# 19. `parts[]`

The `parts` array contains point indices.

Each value identifies the index of the first point belonging to that part/ring.

Example:

```text
num_parts = 2
parts = [0, 41]
```

means:

```text
part 0: points 0 .. 40
part 1: points 41 .. end
```

The values are **start indices**, not lengths.

This distinction must be preserved in all future PurrGo implementations.

---

# 20. MLP points

Each point is:

```text
int32 x
int32 y
```

Little-endian.

The current reference parser converts them to geographic coordinates using:

```text
longitude = x / 1000000.0
latitude  = y / 1000000.0
```

Therefore the source MLP coordinate scale is:

```text
1 000 000 integer units per degree
```

This is a property of the original map format.

---

# 21. PurrGo internal coordinate representation

The current `include/purrgo/map.h` defines the PurrGo map BBox coordinate representation using:

```text
degrees × 10^7
```

The current C parser therefore converts the MLP integer coordinate representation by multiplying by 10:

```text
MLP coordinate / 10^6 degrees
        |
        v
PurrGo coordinate = MLP integer × 10
        |
        v
degrees × 10^7
```

Example:

```text
MLP raw coordinate:
55,755,800

PurrGo internal coordinate:
557,558,000

which represents:
55.7558000 degrees
```

This conversion is an internal PurrGo representation choice. It does not modify the original `.mlp` binary format.

---

# 22. Geographic axis convention

The current reference renderer treats:

```text
X = longitude
Y = latitude
```

The Python projection converts:

```text
longitude -> screen X
latitude  -> screen Y
```

and then inverts the screen Y axis because graphics coordinates have their origin at the top.

The exact camera model is not part of the binary map format.

---

# 23. Basic geometry rendering behavior

The current Python renderer has two basic rendering paths.

## 23.1. Line geometry

If the geometry is not recognized as a closed polygon, it draws the complete point sequence as a polyline.

## 23.2. Closed polygon

The current Python renderer recognizes a polygon when:

```text
number of points > 2
first point == last point
parts[0] == 0
```

For a geometry with multiple parts it uses:

```text
parts[1]
```

as the end of the first contour.

Therefore the current renderer draws only the first/external contour for a multipart geometry.

This is a limitation of the current renderer, not a limitation of the binary format.

---

# 24. Polygon topology

The reference DT G1 specification defines:

```text
Outer rings:
    clockwise (CW)

Inner rings:
    counter-clockwise (CCW)
```

Rings are expected to be closed.

A multipart polygon may therefore contain:

```text
outer ring
inner ring
inner ring
...
```

with all points flattened into one points array and `parts[]` identifying the ring starts.

The current PurrGo renderer does not yet implement complete hole/multipolygon rendering.

---

# 25. POI layer

`pois.idx` has special behavior in the original DT G1 format.

The reference specification states:

- `pois.mlp` does not exist;
- closed polygon objects may be converted to centroid points;
- point coordinates are stored directly in the Data Node BBox;
- for injected points:

```text
minX == maxX
minY == maxY
```

- with topology marker `0x00000001`, `v1` is ignored by the graphics parser;
- large POI collections may use hierarchical R-tree compression.

The current generic PurrGo Python renderer does not implement this POI-specific path.

Therefore POI support is **not yet part of the generic PurrGo geometry renderer contract**.

---

# 26. `.db` attribute database

The original DT G1 specification defines `.db` as a dBase III-compatible database encapsulated in a YZL container.

The database stores object attributes such as names.

The current map renderer does not read `.db`.

Consequently:

```text
geometry rendering
    does not currently depend on .db
```

The Data Node `v2` field is retained as the database record index for future attribute lookup.

The complete `.db` binary specification remains in:

```text
docs/dtg1_map_specification.md
```

and is not duplicated here unless required by the PurrGo map subsystem.

---

# 27. Camera and viewport

The binary map format does not contain a PurrGo display viewport.

The current PurrGo map API uses a geographic bounding box:

```c
typedef struct {
    int32_t min_x;
    int32_t min_y;
    int32_t max_x;
    int32_t max_y;
} purrgo_bbox_t;
```

with coordinates represented as degrees × 10^7.

A viewport contains pixel dimensions.

The conversion from geographic coordinates to screen pixels is therefore a PurrGo rendering concern, not a file-format field.

---

# 28. Current PurrGo implementation status

The following table records the current state.

| Feature | Reference format | Python parser | Current C |
|---|---|---|---|
| YZL header | Defined | Magic validated | Not fully parsed |
| Payload size | Defined | Read | Not central to parser |
| SQT header | Defined | Implemented | Partial |
| Data Node | Defined | Implemented | Implemented internally |
| Nav Node | Defined | Implemented | Implemented internally |
| AABB culling | Defined/used | Implemented | Implemented |
| `v3_jump - 8` | Required | Implemented | **Not implemented** |
| Navigation depth | Defined | Implemented | Uses recursive level |
| Child count | Defined | Implemented | Implemented internally |
| MLP geometry | Defined | Implemented | Partially implemented |
| MLP coordinate scale | 10^6 | Implemented | Converted to 10^7 |
| `parts[]` | Start indices | Used | Parsed |
| Polygon holes | Format supports | Renderer partial | Not complete |
| Object type rendering | Metadata | Read, ignored | Not used |
| `.db` | Defined | Not used | Not implemented |
| POI special path | Defined | Not implemented | Not implemented |
| Public map render API | PurrGo API | N/A | **Empty** |
| GFX integration | PurrGo API | Pygame | **Needs integration** |

---

# 29. Important implementation constraints

The following rules apply to future PurrGo map code.

## 29.1. Do not change the original file interpretation silently

If a future STM32 optimization changes the binary representation, this document must be updated explicitly.

The original DT G1 specification remains unchanged.

## 29.2. Do not infer fields from examples

Every binary field must have a documented interpretation.

If a field is not defined by the reference specification or verified by the parser, it must remain marked as unknown.

## 29.3. Do not introduce floating-point requirements unnecessarily

The original format contains IEEE-754 floating-point BBoxes.

The PurrGo internal geographic representation uses integer coordinates scaled by 10^7.

When adapting the map subsystem for STM32, conversion and comparison should preferably operate using the established fixed-point representation where this can be done without changing the format semantics.

Such an optimization must not be confused with a change to the source map format.

## 29.4. Keep parsing separate from rendering

The map subsystem should conceptually remain:

```text
file / storage
       |
       v
     parser
       |
       v
 spatial traversal
       |
       v
 geometry
       |
       v
 renderer / GFX
```

The parser must not depend on SDL, framebuffer layout, or STM32 display hardware.

---

# 30. Required behavior for the STM32 adaptation

The STM32 version may change the internal implementation, but the following source-format semantics must remain unchanged unless this document is explicitly revised:

- YZL header size and interpretation;
- SQT header structure;
- 28-byte node structure;
- Data Node field positions;
- Navigation Node field positions;
- `v3_jump` semantics;
- `v3_jump - 8` prefetch compensation;
- navigation tree depth;
- child count;
- MLP record structure;
- MLP body structure;
- MLP coordinate scale;
- `parts[]` semantics;
- point ordering;
- polygon ring topology.

The STM32 adaptation may instead change:

- buffering;
- I/O strategy;
- memory allocation;
- streaming behavior;
- fixed-point conversion;
- spatial traversal implementation;
- geometry clipping;
- rendering primitives;
- display-specific coordinate conversion;
- cache management.

These are implementation details and must not alter the meaning of the source map data.

---

# 31. Known current limitations

The following are intentionally left unresolved for the current Stage 2 implementation:

1. Complete `.db` integration.
2. Rendering styles based on object type.
3. Complete multipart polygon rendering.
4. Polygon-hole rendering.
5. POI-specific rendering.
6. Final PurrGo GFX interface for map primitives.
7. Final camera/zoom model.
8. Efficient STM32 streaming/buffering strategy.
9. Final memory limits for map traversal and geometry buffering.

These are implementation tasks, not unknown binary fields.

---

# 32. Stage 2 implementation target

The immediate goal is not to redesign the original map format.

The immediate goal is to produce a correct PurrGo implementation of the currently documented format:

```text
.idx
  |
  +-- YZL
  |
  +-- SQT
        |
        +-- Nav Nodes
        |     |
        |     +-- Nav Nodes
        |     |     |
        |     |     +-- Data Nodes
        |     |
        |     +-- Data Nodes
        |
        +-- Data Nodes
                |
                +-- .mlp geometry
```

with:

```text
AABB culling
      +
correct v3_jump traversal
      +
MLP geometry loading
      +
coordinate conversion
      +
generic GFX rendering
```

Only after this baseline implementation is tested should STM32-specific optimization begin.

---

# 33. Reference files

The following files define the current map subsystem:

```text
docs/dtg1_map_specification.md
tools/map-parser/dtmap-parser.py
include/purrgo/map.h
src/core/map.c
```

`docs/dtg1_map_specification.md` is the immutable original-format reference.

`docs/map-format.md` is the PurrGo working specification and may evolve during STM32 adaptation.
