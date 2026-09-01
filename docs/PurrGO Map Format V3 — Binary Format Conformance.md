# PurrGO Map Format V3 — Binary Format Conformance

**Status:** Binary Format Conformance Audit

This document defines the conformance requirements for implementations of **PurrGO Map Format V3**.

The normative binary format is defined exclusively in:

```text
docs/purrgo_map_specification_v3.md
```

This document does **not** duplicate the binary layout.

---

# 1. Conformance Scope

A V3 implementation is conformant when the following components agree with the normative specification:

```text
                 PurrGO Map Format V3
                         │
              ┌──────────┴──────────┐
              │                     │
       Python map compiler      C firmware parser
              │                     │
           writes V3              reads V3
```

The audit covers:

- binary layout;
- field types and sizes;
- byte order;
- offsets;
- file and section boundaries;
- field semantics;
- `.idx` ↔ `.mlp` ↔ `.db` references;
- geometry invariants;
- LOD structure;
- POI representation;
- malformed input handling;
- compiler ↔ parser compatibility.

---

# 2. Normative Reference

All binary-format questions must be resolved against:

```text
docs/purrgo_map_specification_v3.md
```

In particular, the following definitions are normative:

- PGO header;
- SQT header;
- Navigation Node;
- Data Node;
- `v1`;
- `v2`;
- `v3_jump`;
- `.mlp` Geometry Record;
- `.db` structure;
- coordinate representation;
- POI representation;
- `features.csv` semantics.

If another document or implementation contradicts the specification, the implementation is non-conformant until the discrepancy is resolved.

---

# 3. Required Conformance Checks

## 3.1. PGO Containers

For every `.idx`, `.mlp` and `.db` file verify:

```text
Magic == "PGO"
File Type is valid
Payload Size == File Size - 32
reserved fields are zero
```

For `.idx` additionally verify that every LOD offset points to the corresponding SQT header.

---

## 3.2. `.mlp`

For every Geometry Record verify:

```text
Local Header size == 8
Content Length matches actual body size

Content Length =
    24
    + num_parts  × 4
    + num_points × 8
```

Verify:

```text
parts[] contains point start indices
parts[0] == 0 when num_parts > 0
parts[i] < num_points
```

Verify that all stored coordinates use the V3 fixed-point representation.

---

## 3.3. `.idx`

Verify:

```text
LOD 0, LOD 1 and LOD 2
```

and that every LOD contains a valid SQT header, including empty LODs.

Verify:

```text
Navigation Node = 28 bytes
Data Node       = 25 bytes
```

Verify that the parser consumes exactly the number of children specified by each Navigation Node.

---

## 3.4. `v1`

For a geometry Data Node:

```text
v1
```

must point to the **Geometry Body**, not to the 8-byte Geometry Local Header.

The offset is relative to the beginning of the `.mlp` payload:

```text
absolute_mlp_offset = 0x20 + v1
```

For a native POI:

```text
v1 = 0
```

---

## 3.5. `v2`

Verify that `v2` is interpreted as a **DB record index**, not a byte offset.

Standard layers:

```text
v2 = 0     → no .db
v2 = 1     → dummy record
v2 >= 2    → normal DBF record
```

POI database:

```text
v2 = 0     → unnamed POI
v2 >= 1    → physical POI record
```

---

## 3.6. `v3_jump`

For every Navigation Node:

```text
v3_jump =
    exact byte size of the complete child subtree
```

When a node is culled, the parser must skip exactly:

```text
current_position_after_nav_node + v3_jump
```

No prefetch compensation is applied.

This is a critical V3 conformance requirement.

---

## 3.7. Cross-file References

For every Data Node verify:

```text
Type
v1
v2
```

against the corresponding files and feature definitions.

For geometry features:

```text
.idx v1 → valid .mlp Geometry Body
```

For named features:

```text
.idx v2 → valid .db record
```

For POIs:

```text
.idx BBox → point
v1 == 0
```

No reference may point outside the corresponding file or payload.

---

# 4. Feature Classification

The compiler must implement the `features.csv` rules exactly as specified:

```text
first matching enabled rule wins
```

An object matching multiple rules must therefore produce the feature selected by the first matching enabled rule.

Disabled rules must not produce features.

The binary `.idx` stores the numeric `Code`, not the original OSM tags.

---

# 5. Geometry and Layer Invariants

The compiler must produce geometry compatible with the target layer:

| Layer | Geometry |
|---|---|
| `roads` | line |
| `landuse` | polygon |
| `water` | polygon |
| `pois` | point |

POIs must not require `.mlp` geometry.

Polygon rings must satisfy the topology rules defined by the normative specification.

---

# 6. Map Package

A complete V3 map package must contain:

```text
map.name
```

and its generated layer files.

`map.name` is package metadata, not a PGO binary container.

Its JSON structure is defined by the normative map specification.

Conformance must verify:

- valid JSON;
- required `mapName`;
- required `centerLat`;
- required `centerLon`.

---

# 7. Malformed Input

The firmware parser must reject or safely terminate parsing of malformed data.

At minimum, test:

- invalid PGO magic;
- unsupported file type;
- payload extending beyond the physical file;
- invalid LOD offset;
- invalid SQT header;
- truncated Navigation Node;
- truncated Data Node;
- `v3_jump` beyond the valid payload;
- invalid child count;
- invalid `.mlp` content length;
- invalid `num_parts`;
- invalid `num_points`;
- invalid `parts[]`;
- invalid `.db` record reference.

Malformed input must never result in an out-of-bounds file access or memory access.

---

# 8. Compiler ↔ Parser Test

A V3 conformance test should use a map containing at least:

- multiple LODs;
- empty and non-empty LODs;
- nested Navigation Nodes;
- line geometry;
- polygon geometry;
- multipart polygon geometry;
- named and unnamed objects;
- POIs;
- objects sharing the same feature area;
- objects requiring different `features.csv` rules.

The test procedure is:

```text
OSM/test data
     │
     ▼
Python compiler
     │
     ▼
.idx + .mlp + .db + map.name
     │
     ▼
C parser
     │
     ▼
decoded features
```

The C parser must recover the same feature relationships written by the compiler:

```text
Code
BBox
geometry reference
attribute reference
LOD
layer
```

---

# 9. Conformance Result

An implementation is:

### CONFORMANT

when all mandatory checks pass and compiler output can be consumed by the parser according to the V3 specification.

### NON-CONFORMANT

when any mandatory binary layout, offset, field-semantic, reference, geometry or traversal rule differs from the normative specification.

Implementation-specific optimizations are permitted provided that they do not change the V3 binary representation or its semantics.

---

# 10. Change Control

Any change to the V3 binary representation requires simultaneous review of:

```text
docs/purrgo_map_specification_v3.md
tools/map-compiler/
STM32 map parser
conformance tests
```

The normative specification must be updated before an intentional binary-format change is considered complete.