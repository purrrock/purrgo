# PurrGO Map Format V3 — Binary Format Conformance

Документ предназначен для **Binary Format Conformance Audit** формата карт PurrGO V3.

Цель аудита — доказать соответствие трёх компонентов:

```text
PurrGO Map Format V3
        │
        ├── Python map compiler
        │       └── writes binary
        │
        └── C firmware parser
                └── reads binary
```

Аудит должен проверять не только физический layout, но и:

- типы данных;
- byte order;
- размеры;
- offsets;
- границы файлов и секций;
- семантику полей;
- взаимосвязь `.idx`, `.mlp` и `.db`;
- инварианты геометрии;
- malformed input;
- соответствие Python writer ↔ C reader.

---

# 1. Binary Layout

## 1.1. PGO Global Header

Общий заголовок для `.idx`, `.mlp` и `.db`.
Все multi-byte integer fields — Little-Endian.

| File Offset | Size | Type | Endianness | Field | Description |
|---:|---:|---|---|---|---|
| `0x00` | 3 | `char[3]` | — | Magic | `PGO` (`50 47 4F`) |
| `0x03` | 1 | `uint8` | — | File Type | `1=.idx`, `2=.mlp`, `3=.db` |
| `0x04` | 4 | `uint32` | LE | Payload Size | `File Size - 32` |
| `0x08` | 4 | `uint32` | LE | LOD 0 Offset | Absolute file offset to LOD 0 SQT; `.mlp/.db`: unused |
| `0x0C` | 4 | `uint32` | LE | LOD 1 Offset | Absolute file offset to LOD 1 SQT; `.mlp/.db`: unused |
| `0x10` | 4 | `uint32` | LE | LOD 2 Offset | Absolute file offset to LOD 2 SQT; `.mlp/.db`: unused |
| `0x14` | 4 | `uint32` | LE | Future Extension 1 | Current writer = `0` |
| `0x18` | 4 | `uint32` | LE | Future Extension 2 | Current writer = `0` |
| `0x1C` | 4 | `uint32` | LE | Future Extension 3 | Current writer = `0` |

Total: **32 bytes**.

Источник формата определяет `Payload Size` как число байт после 32-байтового заголовка. Для `.idx` LOD offsets являются абсолютными file offsets; для `.mlp` и `.db` эти поля пока зарезервированы. Future Extension Fields не имеют текущей семантики и должны записываться нулём. 
### PGO Header invariants

```text
Magic == "PGO"

File Type ∈ {1, 2, 3}

Payload Size == File Size - 32

For .idx:
    LOD offsets point to LOD SQT headers

For .mlp/.db:
    LOD offsets are reserved

Future Extension Fields == 0
```

---

# 2. `.mlp` Geometry Format

После 32-байтового PGO Header `.mlp` содержит последовательность geometry records.

```text
+-------------------------+
| PGO Header (32 bytes)   |
+-------------------------+
| Geometry Record         |
|   Local Header (8)      |
|   Body (variable)       |
+-------------------------+
| Geometry Record         |
|   Local Header (8)      |
|   Body (variable)       |
+-------------------------+
| ...                     |
+-------------------------+
```

---

## 2.1. `.mlp` Geometry Record — Local Header

Offset является относительным началу Geometry Record.

| Offset | Size | Type | Endianness | Field | Description |
|---:|---:|---|---|---|---|
| `0x00` | 4 | `uint32` | BE | Sequence Number | Starts at `1`, incremented for every geometry record |
| `0x04` | 4 | `uint32` | LE | Content Length | Exact size of Geometry Body in bytes |

Total: **8 bytes**.

---

## 2.2. `.mlp` Geometry Record — Body

Offset является относительным началу Body.

| Offset | Size | Type | Endianness | Field | Description |
|---:|---:|---|---|---|---|
| `0x00` | 16 | `int32[4]` | LE | BBox | `[minx, miny, maxx, maxy]` |
| `0x10` | 4 | `uint32` | LE | `num_parts` | Number of parts/rings |
| `0x14` | 4 | `uint32` | LE | `num_points` | Total number of points |
| `0x18` | `num_parts × 4` | `uint32[]` | LE | `parts[]` | Start index of each part |
| `0x18 + num_parts×4` | `num_points × 8` | `int32[2][]` | LE | Points | `[X,Y]`, fixed-point `10⁷` degrees |

Content Length:

`24 + num_parts × 4 + num_points × 8`


### Geometry size formula

Для любого корректного Geometry Record:

```text
Content Length =
    24
    + num_parts  × 4
    + num_points × 8
```

Следовательно:

```text
Record Size =
    8 + Content Length
```

Это должно проверяться parser-ом до чтения массивов.

---

# 3. Coordinate Representation

В формате PurrGO V3 координаты `.mlp` и `.idx` представлены как signed `int32` с точностью:

```text
10⁻⁷ degree
```

Формула:

```text
integer_coordinate = geographic_coordinate × 10⁷
```

Например:

```text
55.7558000° → 557558000
37.6173000° → 376173000
```

Ось:

```text
X = longitude
Y = latitude
```

Координаты хранятся без floating-point представления.

### Coordinate invariants

| Проверка | Требование |
|---|---|
| Type | `int32` |
| Byte order | Little-Endian |
| X | Longitude |
| Y | Latitude |
| Scale | `10⁷` units/degree |
| Floating-point | Не используется в binary representation |

---

# 4. `.idx` Spatial Index

`.idx` содержит три LOD:

```text
LOD 0
LOD 1
LOD 2
```

Структура файла:

```text
+-------------------------+
| PGO Header (32 bytes)   |
+-------------------------+
| LOD 0 SQT Header        |
| LOD 0 Nodes             |
+-------------------------+
| LOD 1 SQT Header        |
| LOD 1 Nodes             |
+-------------------------+
| LOD 2 SQT Header        |
| LOD 2 Nodes             |
+-------------------------+
```

Каждый LOD начинается с SQT Header. Даже пустой LOD должен содержать полный SQT Header.

---

# 5. SQT Header

Offset указан **от начала SQT Header**.

Каждый LOD начинается с 16-byte SQT Header.

| Offset | Size | Type | Endianness | Field | Description |
|---:|---:|---|---|---|---|
| `0x00` | 4 | `char[4]` | — | Magic | `SQT\x01` |
| `0x04` | 4 | `uint32` | LE | Topology | Always `1` in current writer |
| `0x08` | 4 | `uint32` | LE | Mode / Depth | Tree depth |
| `0x0C` | 4 | `uint32` | LE | Root Count | Number of root Navigation Nodes |

Total: **16 bytes**.


### Mode

Текущая V3 модель:

```text
mode == 0
    root nodes are Data Nodes

mode > 0
    root nodes are Navigation Nodes
```

Для navigation mode начальная глубина определяется из значения `mode`.

### SQT invariants

```text
Magic == "SQT\x01"

Topology == 1

Root Nodes Count
    determines exactly how many root nodes follow

Empty LOD
    still contains 16-byte SQT Header
```

---

# 6. Navigation Node

Navigation Node представляет пространственный кластер.

Offset указан от начала Navigation Node.

Binary format: `<IiiiiII`

| Offset | Size | Type | Endianness | Field | Description |
|---:|---:|---|---|---|---|
| `0x00` | 4 | `uint32` | LE | `v3_jump` | Exact physical size of the complete child subtree |
| `0x04` | 4 | `int32` | LE | `xmin` | Child subtree BBox |
| `0x08` | 4 | `int32` | LE | `ymin` | Child subtree BBox |
| `0x0C` | 4 | `int32` | LE | `xmax` | Child subtree BBox |
| `0x10` | 4 | `int32` | LE | `ymax` | Child subtree BBox |
| `0x14` | 4 | `uint32` | LE | `v1` / level | Navigation tree level |
| `0x18` | 4 | `uint32` | LE | `v2` / count | Number of immediate children |

Total: **28 bytes**.

Navigation Node не содержит feature code.

---

## 6.1. Navigation Tree Depth

```text
v1 == 0
    children are Data Nodes

v1 > 0
    children are Navigation Nodes
    child depth = v1 - 1
```

---

## 6.2. Navigation Child Count

`v2` содержит количество непосредственных дочерних nodes.

Parser должен обработать **ровно `v2` children**.

Нельзя полагаться на фиксированный размер cluster.

---

# 7. `v3_jump`

`v3_jump` предназначен для быстрого пропуска дочернего subtree при BBox culling.

Актуальная V3 спецификация определяет:

> `v3_jump` — точный физический размер пропускаемого child subtree в байтах.

При отсечении Navigation Node parser должен перейти вперёд ровно на `v3_jump` байт относительно текущей позиции после чтения Navigation Node.

### Обязательный invariant

```text
skip_end =
    current_position_after_nav_node
    + v3_jump
```

При этом:

```text
skip_end <= end_of_valid_idx_payload
```

---

# 8. Data Node

Data Node представляет одну cartographic feature.

Binary format:

```text
<iiiiBII
```

Offset указан от начала Data Node.

Binary format: `<iiiiBII`

| Offset | Size | Type | Endianness | Field | Description |
|---:|---:|---|---|---|---|
| `0x00` | 4 | `int32` | LE | `xmin` | Feature BBox |
| `0x04` | 4 | `int32` | LE | `ymin` | Feature BBox |
| `0x08` | 4 | `int32` | LE | `xmax` | Feature BBox |
| `0x0C` | 4 | `int32` | LE | `ymax` | Feature BBox |
| `0x10` | 1 | `uint8` | — | `Type` | PurrGO feature code |
| `0x11` | 4 | `uint32` | LE | `v1` | MLP geometry-body offset relative to `.mlp` payload |
| `0x15` | 4 | `uint32` | LE | `v2` | DBF record index |

Total: **25 bytes**.

---

# 9. Feature Code

`Type` — это числовой `Code` из `features.csv`.

Например:

```text
features.csv

2;ROAD_NORMAL;...;roads;...
```

даёт:

```text
Data Node:
    Type = 2
```

Firmware работает с компактным PurrGO Code, а не с исходными OSM tags.

### Feature Code invariant

```text
Type == Code from features.csv
```

Оригинальные OSM tags не записываются в `.idx`.

---

# 10. Data Node `v1`

For a geometry feature:
```text
absolute `.mlp` file offset = 0x20 + v1

v1 points to the beginning of the Geometry Body, not to the 8-byte Local Header.

Therefore:
v1 = 0
is reserved/unused for POIs.```

---

# 11. Data Node `v2`

`v2` — **индекс записи в `.db`**, а не byte offset.

Для стандартных слоёв:

```text
v2 = 0
    .db отсутствует

v2 = 1
    Dummy Record

v2 >= 2
    normal DBF record
```

Для POI:

```text
v2 = 0
    unnamed POI

v2 >= 1
    physical POI DBF record
```

Эти правила различаются для standard layers и POI и должны быть проверены отдельно. 
---

# 12. Geometry ↔ Layer invariant

Compiler гарантирует соответствие geometry type целевому layer.

| Layer | Allowed geometry |
|---|---|
| `roads` | Line |
| `landuse` | Polygon |
| `water` | Polygon |
| `pois` | Point |

Firmware renderer не обязан определять geometry type динамически.

Нарушение этого правила означает некорректный map artifact, созданный compiler-ом.

---

# 13. `.mlp` Geometry Invariants

## 13.1. Counts

```text
num_parts >= 0
num_points >= 0
```

Для runtime parser должны дополнительно проверяться ограничения выделяемой памяти.

---

## 13.2. `parts[]`

`parts[]` содержит **start indices**.

Например:

```text
num_parts = 2
parts = [0, 41]
```

означает:

```text
part 0 → points [0 .. 40]
part 1 → points [41 .. num_points-1]
```

Обязательные проверки:

```text
if num_parts > 0:
    parts[0] == 0

for i:
    parts[i] < num_points

for i > 0:
    parts[i] > parts[i-1]
```

---

## 13.3. Polygon Rings

Для polygon:

```text
first point == last point
```

Outer rings:

```text
CW
```

Inner rings / holes:

```text
CCW
```

Все rings хранятся в едином flattened `points[]` массиве, а `parts[]` определяет начало каждого ring.

---

# 14. POI Representation

POI не требует `.mlp` geometry record.

Координата хранится непосредственно в Data Node BBox:

```text
xmin == xmax
ymin == ymax
```

Координаты используют ту же fixed-point representation:

```text
degrees × 10⁷
```

Для native POI:

```text
v1 = unused
```

Feature `Type` определяет PurrGO POI class.

---

## 14.1. POI `.db`

POI DB отличается от стандартного layer DB:

```text
Standard layer:
    v2 = 1 → dummy record

POI:
    v2 = 1 → first physical record
    v2 = 0 → unnamed POI
```

POI `.db` **не содержит dummy record**.

---

# 15. `.db` Attribute Database

`.db` — DBF database внутри PGO container.

PGO Header:

```text
0x00 .. 0x1F
```

DBF payload начинается:

```text
0x20
```

---

## 15.1. DBF Header

The DBF payload starts at absolute file offset `0x20`.

| File Offset | Size | Type | Endianness | Field | Current Value |
|---:|---:|---|---|---|---|
| `0x20` | 1 | `uint8` | — | DBF Magic | `0x03` |
| `0x21` | 3 | — | — | Header bytes | Current writer = `00 00 00` |
| `0x24` | 4 | `uint32` | LE | Number of Records | `total_records` |
| `0x28` | 2 | `uint16` | LE | Header Size | `129` |
| `0x2A` | 2 | `uint16` | LE | Record Size | `117` |
| `0x2C` | 20 | — | — | Reserved/Header bytes | All zero in current writer |

---

## 15.2. Field Descriptors

Each descriptor is exactly 32 bytes:

| Relative Descriptor Offset | Size | Field |
|---:|---:|---|
| `0x00` | 11 | Field name, ASCII, NUL padded |
| `0x0B` | 1 | Type = ASCII `C` |
| `0x0C` | 4 | Reserved = zero |
| `0x10` | 1 | Field length |
| `0x11` | 15 | Reserved = zero |

Descriptors:

| Descriptor | Field Name | Type | Length |
|---:|---|---|---:|
| 1 | `osm_id` | `C` | 12 |
| 2 | `code` | `C` | 4 |
| 3 | `name` | `C` | 100 |

After the third descriptor:

```text
0x0D```

---

Total DBF header size:

32 PGO bytes
+
97 DBF bytes
=
129 bytes

---

# 16. DBF Records

Для standard layer:

```text
v2 = 1
    Dummy Record

v2 = 2
    first named/normal record

v2 = 3
    next record
```

Dummy Record:

```text
117 bytes
all bytes == 0
```

Normal records начинаются стандартным DBF validity indicator:

```text
0x20
```

Если layer не содержит имён:

```text
.db may be omitted
v2 = 0
```

`.db` — DBF Record

Each record is exactly 117 bytes.

| Offset | Size | Type | Description |
|---:|---:|---|---|
| `0x00` | 1 | `char` | dBase record validity marker; normal record = `0x20` |
| `0x01` | 12 | `char[12]` | `osm_id`, zero-padded ASCII |
| `0x0D` | 4 | `char[4]` | `code`, zero-padded ASCII representation |
| `0x11` | 100 | `char[100]` | `name`, zero-padded ASCII/PurrGO 8-бит encoding |

Total:

```text
1 + 12 + 4 + 100 = 117 bytes```

---

# 17. LOD / `.idx` Structural Invariants

Для каждого `.idx`:

```text
PGO Header
    ↓
LOD 0 Offset → SQT Header
    ↓
LOD 0 nodes

LOD 1 Offset → SQT Header
    ↓
LOD 1 nodes

LOD 2 Offset → SQT Header
    ↓
LOD 2 nodes
```

Проверить:

```text
LOD offset >= 32

LOD offset < file_size

LOD offset points to valid SQT Header
```

Каждая LOD section должна иметь корректный SQT Header, включая empty LOD.

---

# 18. Binary Size Invariants

## PGO Header

```text
sizeof(PGO Header) == 32
```

## SQT Header

```text
sizeof(SQT Header) == 16
```

## Navigation Node

```text
sizeof(Navigation Node) == 28
```

## Data Node

```text
sizeof(Data Node) == 25
```

## Geometry Local Header

```text
sizeof(Local Header) == 8
```

## Geometry Body

```text
size =
    24
    + num_parts * 4
    + num_points * 8
```

## DBF Header

```text
Header Size == 129
```

## DBF Record

```text
Record Size == 117
```

---

# 19. Parser Safety Requirements

C parser обязан проверять границы **до каждого чтения переменного размера**.

## 19.1. PGO Header

Reject:

```text
file_size < 32

Magic != "PGO"

File Type not in {1,2,3}

Payload Size != file_size - 32
```

---

## 19.2. LOD Offsets

Reject:

```text
LOD offset < 32

LOD offset >= file_size

LOD offset does not point to valid SQT Header
```

Для `.idx` offsets должны быть интерпретированы как absolute file offsets.

---

## 19.3. SQT Header

Reject:

```text
remaining bytes < 16

Magic != "SQT\x01"

unsupported Topology
```

---

## 19.4. Navigation Node

Before reading:

```text
remaining >= 28
```

After reading:

```text
v3_jump must not move beyond valid payload

v2 child count must be traversable
```

---

## 19.5. Data Node

Before reading:

```text
remaining >= 25
```

---

## 19.6. Geometry

Before allocating/reading:

```text
num_parts >= 0
num_points >= 0

calculated_size does not overflow

calculated_size == Content Length

geometry record lies completely inside .mlp file
```

---

# 20. Integer Overflow Requirements

Особое внимание требуется операциям:

```text
num_parts * 4
num_points * 8
24 + num_parts * 4 + num_points * 8
current_offset + v3_jump
offset + Content Length
```

Нельзя сначала вычислять размер в недостаточно широком signed integer.

Безопасная последовательность:

```text
validate counts
↓
calculate using sufficiently wide unsigned type
↓
check against file boundary / configured memory limit
↓
only then read or allocate
```

---

# 21. MLP Memory Limits

Runtime parser может иметь implementation-specific limits:

```text
PURRGO_MAP_MAX_POINTS
PURRGO_MAP_MAX_PARTS
```

При этом важно различать:

```text
Binary format limit
```

и

```text
Firmware memory limit
```

Например, если firmware ограничивает:

```text
num_points <= PURRGO_MAP_MAX_POINTS
```

это **не означает**, что V3 binary format определяет такое ограничение.

---

# 22. Implementation Conformance Matrix

Эта таблица является главным рабочим checklist аудита.

| ID | Format Item | Python Writer | C Reader | Required Test | Status |
|---|---|---|---|---|---|
| PGO-01 | PGO Header size = 32 | Verify | Verify | byte-size test | `TODO` |
| PGO-02 | Magic `PGO` | Verify | Verify | valid/invalid magic | `TODO` |
| PGO-03 | File Type | Verify | Verify | types 1/2/3 + invalid | `TODO` |
| PGO-04 | Payload Size | Verify | Verify | exact file-size calculation | `TODO` |
| PGO-05 | LOD offsets | Verify | Verify | valid/out-of-range | `TODO` |
| PGO-06 | Future fields | Verify zero | Ignore | non-zero input | `TODO` |
| MLP-01 | Local Header = 8 | Verify | Verify | size test | `TODO` |
| MLP-02 | Sequence number BE | Verify | Verify | byte-order test | `TODO` |
| MLP-03 | Content Length LE | Verify | Verify | byte-order test | `TODO` |
| MLP-04 | Geometry body = Content Length | Verify | Verify | calculated-size test | `TODO` |
| MLP-05 | BBox int32 | Verify | Verify | known coordinates | `TODO` |
| MLP-06 | `num_parts` | Verify | Verify | zero/positive/invalid | `TODO` |
| MLP-07 | `num_points` | Verify | Verify | zero/positive/invalid | `TODO` |
| MLP-08 | `parts[]` start indices | Verify | Verify | multipart geometry | `TODO` |
| MLP-09 | Points int32 pairs | Verify | Verify | known coordinates | `TODO` |
| MLP-10 | Coordinate scale `10⁷` | Verify | Verify | exact round-trip | `TODO` |
| IDX-01 | SQT Header = 16 | Verify | Verify | size test | `TODO` |
| IDX-02 | SQT Magic | Verify | Verify | valid/invalid magic | `TODO` |
| IDX-03 | SQT Topology | Verify | Verify | value validation | `TODO` |
| IDX-04 | SQT Mode | Verify | Verify | flat/tree cases | `TODO` |
| IDX-05 | Root count | Verify | Verify | exact node count | `TODO` |
| NAV-01 | Navigation Node = 28 | Verify | Verify | size test | `TODO` |
| NAV-02 | `v3_jump` | Verify | Verify | subtree skip | `TODO` |
| NAV-03 | Navigation BBox | Verify | Verify | known BBox | `TODO` |
| NAV-04 | Navigation `v1` | Verify | Verify | depth cases | `TODO` |
| NAV-05 | Navigation `v2` | Verify | Verify | child-count cases | `TODO` |
| DATA-01 | Data Node = 25 | Verify | Verify | size test | `TODO` |
| DATA-02 | Data BBox | Verify | Verify | known BBox | `TODO` |
| DATA-03 | Feature Code | Verify | Verify | known `features.csv` mapping | `TODO` |
| DATA-04 | `v1` geometry reference | Verify | Verify | exact offset test | `TODO` |
| DATA-05 | `v2` DB record index | Verify | Verify | DBF index test | `TODO` |
| POI-01 | POI BBox point | Verify | Verify | `xmin==xmax`, `ymin==ymax` | `TODO` |
| POI-02 | POI `v1` unused | Verify | Ignore | POI test | `TODO` |
| POI-03 | POI `.db` indexing | Verify | Verify | first record = 1 | `TODO` |
| DB-01 | DBF Magic | Verify | Verify | `0x03` test | `TODO` |
| DB-02 | DBF record count | Verify | Verify | count consistency | `TODO` |
| DB-03 | DBF header size = 129 | Verify | Verify | exact value | `TODO` |
| DB-04 | DBF record size = 117 | Verify | Verify | exact value | `TODO` |
| DB-05 | 3 field descriptors | Verify | Verify | descriptor count | `TODO` |
| DB-06 | Dummy record | Verify | Verify | all-zero 117 bytes | `TODO` |
| DB-07 | Normal DBF records | Verify | Verify | validity byte | `TODO` |

---

# 23. Semantic Conformance Matrix

Следующие semantic contracts также должны быть проверены.

| ID | Contract |
|---|---|
| `SEM-01` | Coordinates are `int32`, fixed-point `degrees × 10⁷` |
| `SEM-02` | X = longitude, Y = latitude |
| `SEM-03` | `.mlp` `num_parts` and `num_points` are written as `uint32` |
| `SEM-04` | `parts[]` contains start indices, not lengths |
| `SEM-05` | `v1` points to Geometry Body relative to `.mlp` payload start |
| `SEM-06` | Absolute MLP body offset = `32 + v1` |
| `SEM-07` | `v1 = 0` for native POI |
| `SEM-08` | `v3_jump` equals exact byte size of complete child subtree |
| `SEM-09` | `v3_jump` excludes the current Navigation Node itself |
| `SEM-10` | Navigation BBox is stored as `int32 × 4` |
| `SEM-11` | Data BBox is stored as `int32 × 4` |
| `SEM-12` | Navigation `v1 > 0` means child nodes are Navigation Nodes |
| `SEM-13` | Navigation `v1 == 0` means child nodes are Data Nodes |
| `SEM-14` | Navigation `v2` is immediate child count |
| `SEM-15` | Data `Type` is PurrGO feature Code |
| `SEM-16` | Standard unnamed feature → `v2 = 1` |
| `SEM-17` | Standard named features → `v2 >= 2` |
| `SEM-18` | Standard layer with no names → `.db` may be omitted and `v2 = 0` |
| `SEM-19` | POI unnamed → `v2 = 0` |
| `SEM-20` | POI first physical DBF record → `v2 = 1` |
| `SEM-21` | Standard layer DBF record 1 is the zero-filled dummy record |
| `SEM-22` | POI DBF has no dummy record |
| `SEM-23` | Empty LOD still contains a 16-byte SQT Header |

---

# 24. Negative / Malformed Input Tests

Минимальный набор malformed files:

| ID | Corruption | Expected result |
|---|---|---|
| NEG-01 | File shorter than 32 bytes | Reject |
| NEG-02 | Invalid PGO magic | Reject |
| NEG-03 | Invalid File Type | Reject |
| NEG-04 | Incorrect Payload Size | Reject |
| NEG-05 | LOD offset before payload | Reject |
| NEG-06 | LOD offset outside file | Reject |
| NEG-07 | Invalid SQT magic | Reject |
| NEG-08 | Truncated SQT Header | Reject |
| NEG-09 | Truncated Navigation Node | Reject |
| NEG-10 | Truncated Data Node | Reject |
| NEG-11 | `v3_jump` outside file | Reject |
| NEG-12 | `v2` child count exceeds available nodes | Reject |
| NEG-13 | Truncated MLP Local Header | Reject |
| NEG-14 | Content Length exceeds file | Reject |
| NEG-15 | Content Length calculation mismatch | Reject |
| NEG-16 | Negative `num_parts` | Reject |
| NEG-17 | Negative `num_points` | Reject |
| NEG-18 | `parts[i] >= num_points` | Reject |
| NEG-19 | Non-monotonic `parts[]` | Reject |
| NEG-20 | Geometry size integer overflow | Reject |
| NEG-21 | DBF header truncated | Reject |
| NEG-22 | Invalid DBF magic | Reject |
| NEG-23 | Invalid DBF header size | Reject |
| NEG-24 | Invalid DBF record size | Reject |
| NEG-25 | `v2` outside DBF record range | Reject |

---

# 25. Golden Binary Tests

Для каждого фундаментального binary structure должен существовать минимальный golden fixture.

Минимальный набор:

```text
golden/
    pgo_idx_minimal.bin
    pgo_mlp_minimal.bin
    pgo_db_minimal.bin

    sqt_empty.bin
    sqt_flat.bin
    sqt_tree.bin

    data_node.bin
    nav_node.bin

    mlp_line.bin
    mlp_polygon.bin
    mlp_multipart.bin
    mlp_polygon_hole.bin

    poi_idx.bin
    db_standard.bin
    db_poi.bin
```

Каждый fixture должен проверяться:

```text
Python writer
    ↓
binary output
    ↓
byte-exact comparison
    ↓
golden fixture
```

---

# 26. Python ↔ C Round-Trip Tests

Для каждого fixture должна выполняться проверка:

```text
Python compiler
      ↓
     binary
      ↓
C parser
      ↓
runtime representation
```

и значения должны совпадать:

```text
BBox
Type
v1
v2
LOD
SQT parameters
Navigation depth
Child count
v3_jump
num_parts
num_points
parts[]
points[]
```

---

# 27. Binary Format vs Runtime Representation

Следует строго разделять:

```text
ON-DISK FORMAT
```

и:

```text
RUNTIME REPRESENTATION
```

Например:

```text
.idx Data Node
    int32 coordinates
        ↓
C runtime
    purrgo_bbox_t
```

или:

```text
.mlp coordinate
    int32 × 10⁷
        ↓
screen projection
        ↓
integer pixel coordinates
```

Изменение runtime representation **не является изменением binary format**, пока значения и их интерпретация на диске остаются прежними.

---

# 28. STM32 Constraints

При переносе на STM32 должны сохраняться:

- PGO Header layout;
- SQT Header layout;
- Navigation Node layout;
- Data Node layout;
- MLP Local Header;
- MLP Body;
- coordinate scale;
- `parts[]` semantics;
- polygon topology;
- `v3_jump`;
- LOD structure;
- `.db` record semantics.

Можно менять:

- buffering;
- I/O strategy;
- memory allocation;
- streaming;
- parser implementation;
- fixed-point arithmetic;
- geometry clipping;
- rendering;
- cache strategy.

Это implementation details, а не изменения binary format.

---

# 29. Open Issues — требуют отдельной проверки

Эти пункты **нельзя считать установленными только на основании текущей таблицы**.

## CON-01 — `Data Node v1` target

Необходимо точно установить:

```text
v1
 ↓
offset in .mlp
 ↓
Local Header?
или
Geometry Body?
```

Текущая спецификация говорит о geometry reference, но для conformance audit необходимо зафиксировать точную адресацию.

---

## CON-02 — `.db` field descriptor layout

Текущая V3 specification определяет:

```text
3 descriptors × 32 bytes

osm_id
code
name
```

но не предоставляет отдельную полную byte-level таблицу всех полей каждого descriptor.

До дополнительной проверки нельзя придумывать:

- field type;
- field width;
- field decimal count;
- exact descriptor offsets beyond the descriptor boundaries.

---

## CON-03 — DBF record field layout

Аналогично необходимо отдельно проверить фактический layout 117-byte record:

```text
deletion flag
osm_id
code
name
padding / remaining bytes
```

Если compiler уже реализует это, его output должен быть сопоставлен с фактическим DBF layout.

---

## CON-04 — LOD offset ordering

Нужно проверить на реальном compiler output, является ли обязательным:

```text
LOD0 < LOD1 < LOD2
```

или допускаются одинаковые offsets для некоторых пустых/специальных случаев.

Не следует превращать это предположение в binary-format invariant без проверки compiler implementation.

---

## CON-05 — Maximum geometry size

`PURRGO_MAP_MAX_POINTS` и `PURRGO_MAP_MAX_PARTS` являются потенциальными firmware limits.

Необходимо отдельно определить:

```text
format limit
vs
PC parser limit
vs
STM32 RAM limit
```

---

# 30. Audit Completion Criteria

Binary Format Conformance Audit считается завершённым только если:

### Physical layout

- [ ] PGO Header verified
- [ ] `.idx` LOD offsets verified
- [ ] SQT Header verified
- [ ] Navigation Node verified
- [ ] Data Node verified
- [ ] `.mlp` Local Header verified
- [ ] `.mlp` Body verified
- [ ] `.db` PGO container verified
- [ ] DBF Header verified
- [ ] DBF descriptors verified
- [ ] DBF records verified

### Semantics

- [ ] `v1` semantics verified
- [ ] `v2` semantics verified
- [ ] `v3_jump` verified
- [ ] Navigation depth verified
- [ ] child count verified
- [ ] LOD semantics verified
- [ ] POI semantics verified
- [ ] `parts[]` semantics verified
- [ ] polygon topology verified
- [ ] coordinate representation verified
- [ ] layer ↔ geometry invariant verified

### Safety

- [ ] all fixed-size reads bounds-checked
- [ ] all variable-size reads bounds-checked
- [ ] integer overflow checked
- [ ] file offsets checked
- [ ] `v3_jump` checked
- [ ] geometry size checked
- [ ] DBF indices checked
- [ ] malformed input tests implemented

### Cross-implementation

- [ ] Python writer matches specification
- [ ] C reader matches specification
- [ ] Python output passes C parser
- [ ] golden binary tests pass
- [ ] Python/C semantic round-trip tests pass

---

# 31. Audit Status

| Area | Status |
|---|---|
| PGO Header | `TODO — verify implementation` |
| `.idx` layout | `TODO — verify implementation` |
| SQT | `TODO — verify implementation` |
| Navigation Node | `TODO — verify implementation` |
| Data Node | `TODO — verify implementation` |
| `.mlp` layout | `TODO — verify implementation` |
| Coordinate representation | `TODO — verify implementation` |
| `parts[]` | `TODO — verify implementation` |
| Polygon topology | `TODO — verify implementation` |
| `v1` | `OPEN ISSUE` |
| `v2` | `TODO — verify implementation` |
| `v3_jump` | `TODO — verify implementation` |
| POI | `TODO — verify implementation` |
| `.db` container | `TODO — verify implementation` |
| DBF header | `TODO — verify implementation` |
| DBF descriptors | `OPEN ISSUE` |
| DBF records | `OPEN ISSUE` |
| Negative tests | `TODO` |
| Golden binary tests | `TODO` |
| Python ↔ C round-trip | `TODO` |

---

## 32. Reference

Основным источником binary contract является:

```text
purrgo_map_specification_v3.md
```

Все значения в настоящем документе должны быть проверены против текущей V3 specification и фактической реализации compiler/parser.

Если implementation и specification расходятся, это должно быть зарегистрировано как **conformance failure**, а не исправлено молча.

Главный принцип аудита:

```text
SPECIFICATION
     ↓
PYTHON WRITER
     ↓
BINARY FILE
     ↓
C READER
     ↓
RUNTIME OBJECT
```

Каждый переход должен быть доказуемо корректным.