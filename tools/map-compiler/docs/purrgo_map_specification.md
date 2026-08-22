# PurrGO MAP FORMAT TECHNICAL SPECIFICATION

---

## 1. GLOBAL LAYER HEADER (YZL HEADER)

Each binary map layer file (`.idx`, `.mlp`, `.db`) must begin with a global system header with a fixed length of **32 bytes**. The header is used by the firmware bootloader for memory mapping and integrity checking via the MD5 algorithm.

### 1.1. YZL Header Byte Structure

| Offset | Size | Data Type | Field Description | Value / Format |
| :--- | :--- | :--- | :--- | :--- |
| `0x00` | 3 | `char[3]` | **Magic Signature (Magic)** | Strictly `b'YZL'` (`59 5A 4C`) |
| `0x03` | 1 | `uint8` | **File Magic Extension** | For `.mlp` and `.db` = `0x00`. For `.idx` variable (`0x08`, `0x10`, `0x0C`, etc.) |
| `0x04` | 4 | `uint32` | **Payload Size** | In bytes (Little-Endian). Formula: `File Size - 32` |
| `0x08` | 4 | `uint32` | **RAM Load Type** | For `.mlp`/`.db`/`pois.idx` = `0x04000000` (LE: `b'\x00\x00\x00\x04'`). For `.idx` = `0x04000002` (LE: `b'\x02\x00\x00\x04'`) |
| `0x0C` | 4 | `uint32` | **LOD 2 Section Size** | In bytes (Big-Endian). For `fseek` from EOF (fast pointer calculation). For `.mlp` and `.db` = `0x00000000` |
| `0x10` | 16 | `byte[16]`| **MD5 Checksum** | Computed in hardware from the payload body (from `0x20` to EOF) |

> **Exception for the POI layer (`pois.idx`):**
> * **Offset `0x03` (Magic Extension):** Value is strictly `0x00`.
> * **Offset `0x08` (RAM Load Type):** Takes the value `0x04000000` (Little-Endian: `b'\x00\x00\x00\x04'`), identical to `.mlp` and `.db` headers.

---

## 2. GEOMETRY FILE STRUCTURE (.MLP)

The `.mlp` file contains raw ordered coordinates of vertices for linear and polygonal objects (unified polyline/polygon storage). Coordinates are represented in a fixed-precision integer format.

### 2.1. Local Geometry Header (Geometry Record Header)

Immediately following the 32 bytes of the global YZL header is an array of geometry records. Each record is prefaced by its own 8-byte mini-header.

| Offset | Size | Data Type | Endianness | Field Description |
| :--- | :--- | :--- | :--- | :--- |
| `0x00` | 4 | `uint32` | **Big-Endian** (`>I`) | Geometry record sequence number (starting from 1) |
| `0x04` | 4 | `uint32` | **Little-Endian** (`<I`) | Geometry record body length in bytes (Content Length) |

### 2.2. Geometry Record Body

The record body size in bytes is strictly equal to the `Content Length` field value.

| Offset | Size | Data Type | Field Description / Specifics |
| :--- | :--- | :--- | :--- |
| `0x00` | 16 | `int32[4]` | `bbox_int` = `[minx, miny, maxx, maxy]` |
| `0x10` | 4 | `int32` | `num_parts` — total number of segments/rings in the object |
| `0x14` | 4 | `int32` | `num_points` — total number of points (vertices) in the object |
| `0x18` | `num_parts * 4` | `uint32[]` | `parts` array. Point indices (offsets) where each segment begins (starting from 0). *These are indices, not contour lengths.* |
| `0x18 + (num_parts * 4)` | `num_points * 8` | `int32[2][]` | `points` array (pairs of `[X, Y]`). Conversion to Float: `Coordinate = Value / 1000000.0` |

### 2.3. Polygon Topology (Multipolygons)

The hardware triangulator supports complex polygon structures (e.g., lakes with islands), similar to the ESRI Shapefile format:

* All points of all rings (both outer contours and inner "holes") are flattened into a single `points` array.
* The `num_parts` field indicates the total number of rings in the multipolygon.
* The `parts` array contains the start indices of each ring (e.g., `[0, 41]`).
* Rings must be closed (the first and last coordinate of the contour must match).
* Single closed polygons are always classified as Outer and are wound clockwise.

**Winding Rules:**

* **Outer (External contours):** Strictly **Clockwise (CW)**.
* **Inner (Internal holes):** Strictly **Counter-Clockwise (CCW)**.

---

## 3. SPATIAL INDEX STRUCTURE (.IDX)

The `.idx` file organizes data by Levels of Detail (LOD) and clusters them for fast invisible geometry discarding (BBox Culling).

### 3.1. Flat List Architecture and Hardware State Machine

Within a LOD, objects are packed into flat groups (clusters) of a fixed size (maximum of 14 data objects). The engine uses a unified 28-byte frame for both Navigation Nodes (Nav Node) and Data Nodes (Data Node).

Fundamental rule: the last 8 bytes of any node (offset `+20` and `+24`) are always `v1` and `v2` pointers. This allows the hardware parser to traverse the memory graph without analyzing the node type.

### 3.2. Node Type Specification (C-Union Pattern)

```c
struct UnifiedNode {
    union {
        struct { float xmin, ymin, xmax, ymax; uint32_t type; } data; // 20 bytes
        struct { uint32_t v3_jump; float xmin, ymin, xmax, ymax; } nav;  // 20 bytes
    } payload;
    uint32_t v1;  // offset 20
    uint32_t v2;  // offset 24
};
````

#### 1. Data Node

Contains the cartographic primitive itself. Unpacking format (Python): `<ffffIII`.

| Offset        | Size | Type       | Description                                                                 |
| :------------ | :--- | :--------- | :-------------------------------------------------------------------------- |
| `0x00 - 0x0F` | 16   | `float[4]` | BBox (xmin, ymin, xmax, ymax)                                               |
| `0x10 - 0x13` | 4    | `uint32`   | **Type** (OSM object class code, e.g., `5114` — secondary road)             |
| `0x14 - 0x17` | 4    | `uint32`   | **v1** (Pointer to Payload in `.mlp`). Must skip the 8-byte geometry header |
| `0x18 - 0x1B` | 4    | `uint32`   | **v2** (Index in `.db`). `1` — empty record (unnamed), `>=2` — unique names |

> **POI Layer:**
>
> * The `pois.mlp` file does not exist.
> * Closed polygons (e.g. buildings, shops) are dynamically converted to points during compilation by calculating their mathematical centroid.
> * **Offset `0x00 - 0x0F` (Point Injection):** 32-bit Float coordinates are written directly into the BBox. Strict duplication is required: `minX == maxX` and `minY == maxY`.
> * **Offset `0x14 - 0x17` (v1):** Ignored by the graphics parser when the topology marker is `0x00000001` (`b'\x01\x00\x00\x00'`). In the current implementation, `v1` is forcefully set to `0`.
> * **R-Tree Compression:** Large POI arrays now also utilize hierarchical R-Tree (STR) compression, similar to standard vector layers.

#### 2. Nav Node (Navigation Node / Macro Node)

Opens geometry (Mode 0x00) or a branch of the hierarchical R-tree (Mode > 0x01).

Unpacking format (Python): `<IffffII`.

| Offset        | Size | Type     | Description                                                                                                                              |
| ------------- | ---- | -------- | ---------------------------------------------------------------------------------------------------------------------------------------- |
| `0x00 - 0x03` | 4    | uint32   | `v3_jump` (Early Exit jump). Size of the entire child subtree in bytes + 8 bytes of prefetch compensation.                               |
| `0x04 - 0x13` | 16   | float[4] | Cluster BBox (covers all nested child BBoxes)                                                                                            |
| `0x14 - 0x17` | 4    | uint32   | `v1` (Tree Depth / Height of subtree). `0` = children are `Data Node`. `>0` = children are macro-nodes (`Nav Node` with level `v1 - 1`). |
| `0x18 - 0x1B` | 4    | uint32   | `v2` (Number of child nodes directly inside the current cluster).                                                                        |

> **Warning:** The `v3_jump` field requires mandatory hardware prefetch compensation (`+8` bytes to the jump length).

---

## 4. ATTRIBUTE DATABASE STRUCTURE (.DB)

The `.db` file stores text names (`name` tags). The format is a standard **dBase III (DBF)** DBMS encapsulated within a YZL container.

### 4.1. DBF Header Specification (Size: 161 bytes)

Starts at offset `0x20` (immediately after the YZL header).

* **dBase III Magic Byte:** `0x03` (Offset `0x20`).
* **Number of Records:** 4 bytes (LE) at offset `0x24`. `(Named objects + 1)`.
* **Database Header Size:** Strictly `161` (`0xA1 0x00`) at offset `0x28`.
* **Record Size:** Strictly `145` (`0x91 0x00`) at offset `0x2A`.

### 4.2. Record Fields Mapping

Each field description length is 32 bytes. Field description terminator is `0x0D`.

1. `osm_id` (Type `C`, 12 bytes).
2. `code` (Type `C`, 4 bytes).
3. `fclass` (Type `C`, 28 bytes).
4. `name` (Type `C`, 100 bytes).

### 4.3. Importance of Record 0

The first physical data record in `.db` (145 bytes) **must strictly be filled with zeros** (`\x00`).

* Unnamed map objects point to it (pointer `v2 = 1`).
* Named records are packed subsequently (`v2 = 2`, `v2 = 3`, etc.).
* Each record begins with a `0x20` byte (dBase validity indicator).

> **Skipping .db Creation:**
> If a layer contains no named objects (and is not the POI layer), `.db` file creation is skipped entirely, and all features receive `v2 = 0`.

> **Exception for the POI layer:**
> In `pois.db`, the zero dummy record is absent. The first data record starts immediately after the `0x0D` terminator. 1-based indexing is applied: `v2 = 1` points to the first physical record. Unnamed POIs receive `v2 = 0`.

---

## 5. OPERATION ALGORITHMS (PSEUDOCODE)

### 5.1. Spatial Index (IDX) Compilation Algorithm

**INPUT:** List of objects with computed `v1` (from MLP), `v2` (from DB), BBox, and Code.
**OUTPUT:** Binary map layer file `.idx`.

1. Initialize an empty byte buffer `idx_body`.
2. Apply Sort-Tile-Recursive (STR) Bulk Loading algorithm for hardware Z-Culling:

   * Sort objects by X axis (Centroid longitude).
   * Calculate mathematical slice limits (chunks of up to 14 elements).
   * Cut vertical slices and sort them by Y axis (Centroid latitude).
   * Pack into C-Union clusters (14 elements each) to form hierarchical R-tree macro-nodes (Nav Nodes).
3. For each node (recursive):

   * Calculate the bounding rectangle (Enveloping BBox) for all children.
   * Compute hardware jump `v3_jump` = size of the entire tree under this node + 8 bytes of prefetch compensation.
4. Compile LOD 0, LOD 1, and LOD 2 sections.

   * Prepend a 16-byte header: `b'SQT\x01\x01\x00\x00\x00'` + `pack("<II", depth, root_nodes_count)`.
   * Nodes are always arranged consecutively.
   * If a LOD is empty, the section consists of exactly 16 bytes: `[SQT\x01] [01 00 00 00] [00 00 00 00] [00 00 00 00]`.
5. Calculate `lod2_size` (exact size of the LOD 2 section).
6. Generate YZL Header and append `idx_body` to the file `LAYER_NAME.idx`.

### 5.2. Hardware Parsing Algorithm via State Machine

```python
def parse_node(file, is_nav_node, current_level):
    if not is_nav_node:
        # Read Data Node
        data_bytes = file.read(28)
        xmin, ymin, xmax, ymax, obj_type, v1, v2 = struct.unpack("<ffffIII", data_bytes)
        render_object(xmin, ymin, xmax, ymax, obj_type, v1, v2)
        return

    # Read Nav Node
    nav_bytes = file.read(28)
    v3_jump, c_xmin, c_ymin, c_xmax, c_ymax, nav_level, obj_count = struct.unpack("<IffffII", nav_bytes)

    if not is_in_screen(c_xmin, c_ymin, c_xmax, c_ymax):
        # Culling: Jump to the next Nav Node
        # v3_jump ALREADY includes the +8 bytes of hardware pipeline prefetch compensation!
        file.seek(v3_jump - 8, 1)
        return

    # Cluster is visible, recursively read nested nodes
    for _ in range(obj_count):
        parse_node(file, nav_level > 0, nav_level - 1 if nav_level > 0 else 0)

# 1. Read SQT header (16 bytes)
magic, res, depth, count = struct.unpack("<IIII", file.read(16))

if count == 0:
    return # Empty LOD, proceed to the next

for _ in range(count):
    parse_node(file, depth > 0, depth - 1 if depth > 0 else 0)
```

---

## 6. INTERNAL STYLE TABLE (LUT) AND HARDWARE Z-CULLING

The `code` field is hardcoded to an internal Look-Up Table (LUT) style table corresponding to the **Geofabrik GIS** standard. The navigator does not render all objects constantly, employing **hardware Z-Culling**.

**Hardcoded Visibility Thresholds (Display Scale):**

* **Scale 1000+ m:** Only major highways (`5111`, `5112`, `5113`, `5114`). Polygons (landuse, water) are hardware-blocked by the GPU.
* **Scale 500 m:** Minor roads and slip roads (`5115`, `513x`). Rendering of water bodies and forests is permitted.
* **Scale 100 m:** Local streets (`512x`).
* **Scale 50 m:** Agricultural and technical roads (`514x`).
* **Scale 10 m (Max zoom):** Pedestrian and bicycle paths, steps (`515x`).

> **Engineering Conclusion:** PurrGO uses spatial indexing (Z-Culling) to manage the display of vector data. The map compiler must therefore strictly adhere to the C-Union Node structure and SQT spatial index format.

---

## 7. KNOWN HARDWARE QUIRKS & EDGE CASES

### 7.1. v3_jump +8 Byte Prefetch Anomaly

The `v3_jump` field in Nav Nodes requires a mandatory `+8` byte compensation due to the hardware's speculative memory prefetch behavior.

### 7.2. LOD Section Headers

Each LOD section begins with a 16-byte SQT header. Even empty LODs must contain a complete 16-byte header.

### 7.3. POI Data Insertion

POI coordinates are stored directly in the `.idx` BBox as floating-point values. The `v1` pointer is unused for POI records.

### 7.4. Empty DB Layers

If a standard layer contains no named objects, the `.db` file may be omitted and all `v2` values must be zero.

---

## 8. SUMMARY

The PurrGO map format consists of three primary binary layers:

1. **`.idx`** — spatial index containing LOD sections, C-Union nodes and R-tree hierarchy.
2. **`.mlp`** — vector geometry storage for linear and polygonal objects.
3. **`.db`** — dBase III-compatible attribute database containing object names.

The format is designed for efficient spatial culling and sequential geometry access on resource-constrained navigation hardware.

```

