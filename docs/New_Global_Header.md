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

