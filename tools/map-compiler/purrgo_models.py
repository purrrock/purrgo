#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import struct
from dataclasses import dataclass, field
from typing import List, Tuple, Any, Sequence

# Глобальная инициализация таблицы символов (выполняется один раз)
_PGO_CHAR_MAP = {
     
     # Замена отсутствующих символов на ASCII аналоги
    '№': 0x4E,  # Заменяем знак номера на латинскую 'N'
    'Ĺ': 0x4C,  # Заменяем на латинскую 'L'
    'ĺ': 0x6C,  # Заменяем на латинскую 'l'
    
    'А': 0x80, 'Б': 0x81, 'В': 0x82, 'Г': 0x83, 'Д': 0x84, 'Е': 0x85, 'Ж': 0x86, 'З': 0x87,
    'И': 0x88, 'Й': 0x89, 'К': 0x8A, 'Л': 0x8B, 'М': 0x8C, 'Н': 0x8D, 'О': 0x8E, 'П': 0x8F,
    'Р': 0x90, 'С': 0x91, 'Т': 0x92, 'У': 0x93, 'Ф': 0x94, 'Х': 0x95, 'Ц': 0x96, 'Ч': 0x97,
    'Ш': 0x98, 'Щ': 0x99, 'Ъ': 0x9A, 'Ы': 0x9B, 'Ь': 0x9C, 'Э': 0x9D, 'Ю': 0x9E, 'Я': 0x9F,
    'а': 0xA0, 'б': 0xA1, 'в': 0xA2, 'г': 0xA3, 'д': 0xA4, 'е': 0xA5, 'ж': 0xA6, 'з': 0xA7,
    'и': 0xA8, 'й': 0xA9, 'к': 0xAA, 'л': 0xAB, 'м': 0xAC, 'н': 0xAD, 'о': 0xAE, 'п': 0xAF,
    'р': 0xB0, 'с': 0xB1, 'т': 0xB2, 'у': 0xB3, 'ф': 0xB4, 'х': 0xB5, 'ц': 0xB6, 'ч': 0xB7,
    'ш': 0xB8, 'щ': 0xB9, 'ъ': 0xBA, 'ы': 0xBB, 'ь': 0xBC, 'э': 0xBD, 'ю': 0xBE, 'я': 0xBF,
    'Ґ': 0xC0, 'Є': 0xC1, 'І': 0xC2, 'Ї': 0xC3, 'Ў': 0xC4, 'Ђ': 0xC5, 'Ј': 0xC6, 'Љ': 0xC7,
    'Њ': 0xC8, 'Џ': 0xC9, 'Ѓ': 0xCA, 'Ѕ': 0xCB, 'Ғ': 0xCC, 'Қ': 0xCD, 'Ң': 0xCE, 'Ұ': 0xCF,
    'ґ': 0xD0, 'є': 0xD1, 'і': 0xD2, 'ї': 0xD3, 'ў': 0xD4, 'ђ': 0xD5, 'ј': 0xD6, 'љ': 0xD7,
    'њ': 0xD8, 'џ': 0xD9, 'ѓ': 0xDA, 'ѕ': 0xDB, 'ғ': 0xDC, 'қ': 0xDD, 'ң': 0xDE, 'ұ': 0xDF,
    'Ё': 0xF0, 'ё': 0xF1
}

_LATIN_EXT = {
    0xE0: 'ÄäÀàÂâÃãÅå', 0xE1: 'ÖöÒòÔôÕõØø', 0xE2: 'ŬŭÜüÙùÛû', 0xE3: 'ß',
    0xE4: 'ÇçĆćČč', 0xE5: 'ĞğĜĝ', 0xE6: 'İ', 0xE7: 'ı',
    0xE8: 'ŠšŞşȘșŚś', 0xE9: 'Ąą', 0xEA: 'Ęę', 0xEB: 'Łł',
    0xEC: 'ŃńŇňÑñ', 0xED: 'Óó', 0xEE: 'ŽžŹźŻż', 0xEF: 'Řř'
}

for _pgo_code, _unicode_chars in _LATIN_EXT.items():
    for _char in _unicode_chars:
        _PGO_CHAR_MAP[_char] = _pgo_code

class HWConfig:
    PGO_HEADER_SIZE = 32
    DATA_NODE_SIZE = 25      # Data Node size
    NAV_NODE_SIZE = 28       # Nav Node size
    CHUNK_SIZE = 14          # Maximum number of objects in a cluster
    DBF_HEADER_LEN = 129     # dBase III header
    DBF_RECORD_LEN = 117     # Fixed-length dBase III record

    RAM_LOAD_TYPE = 0x04000000

def pgo_encode(text: Any, max_len: int) -> bytes:
    """
    Конвертирует строку в однобайтовую кодировку PurrGO-256.
    Обрезает до max_len и дополняет нулями.
    """
    # Сразу обрезаем исходную строку, чтобы не обрабатывать лишние символы
    source = str(text or "")[:max_len]
    result = bytearray()
    
    for char in source:
        code = ord(char)
        if code <= 0x7F:
            result.append(code)
        else:
            result.append(_PGO_CHAR_MAP.get(char, 0x3F))
            
    return bytes(result).ljust(max_len, b'\x00')

# [MEMORY OPTIMIZATION 1]: Указание slots=True предотвращает создание __dict__ и __weakref__
# Это экономит ~56-64 байта чистой памяти на каждом экземпляре класса.
@dataclass(slots=True)
class MapFeature:
    """Represents a single map primitive (Road, Polygon, POI)"""
    osm_id: str
    code: int
    name: str
    points: bytes
    lod: int

    # [MEMORY OPTIMIZATION 2]: Используем иммутабельный (неизменяемый) кортеж по умолчанию.
    # Это позволяет всем миллионам объектов без multipolygon ссылаться на один и тот же (0,) в памяти.
    parts: Sequence[int] = (0,)

    bbox: Tuple[int, int, int, int] = (0, 0, 0, 0)
    v1: int = 0        # Absolute geometry offset in the .mlp file
    v2: int = 0        # Row index in the attribute DB .db
    mlp_size: int = 0  # Binary body size in the .mlp file

    def calculate_bbox(self) -> None:
        """Direct bounding box calculation (optimized O(N) single pass)."""
        if not self.points:
            return

        # [MEMORY OPTIMIZATION 3]: Отказ от List Comprehension.
        # Проходим по массиву координат ровно один раз (O(N)), не выделяя память под временные списки.
        iterator = struct.iter_unpack("<ii", self.points)
        try:
            p0 = next(iterator)
        except StopIteration:
            return

        minx, miny = p0[0], p0[1]
        maxx, maxy = p0[0], p0[1]

        for x, y in iterator:
            if x < minx: minx = x
            elif x > maxx: maxx = x

            if y < miny: miny = y
            elif y > maxy: maxy = y

        self.bbox = (minx, miny, maxx, maxy)

    def pack_data_node(self) -> bytes:
        """
        Packing a Data Node (strictly 25 bytes).
        Format: [BBox 16b] [Type 1b] [v1 4b] [v2 4b]
        """
        return struct.pack(
            "<iiiiBII",
            self.bbox[0], self.bbox[1],
            self.bbox[2], self.bbox[3],
            self.code, self.v1, self.v2
        )


# Распространяем паттерн __slots__ на узлы R-Дерева
@dataclass(slots=True)
class RTreeNode:
    """
    Represents a Hierarchical Macro-Node (Nav Node) for the Spatial Quadrant Tree.
    Calculates boundaries and pre-fetches byte-offsets for hardware Z-Culling.
    """
    level: int
    children: List[Any]  # List of MapFeature (if level 0) or RTreeNode (if level > 0)
    bbox: Tuple[int, int, int, int] = field(init=False)
    v3_jump: int = field(init=False)
    bin_size: int = field(init=False)

    def __post_init__(self):
        # [MEMORY OPTIMIZATION 4]: Аналогичный O(N) проход для Macro-Nodes
        if not self.children:
            self.bbox = (0, 0, 0, 0)
            self.v3_jump = 0
            self.bin_size = HWConfig.NAV_NODE_SIZE
            return

        c0_bbox = self.children[0].bbox
        minx, miny, maxx, maxy = c0_bbox[0], c0_bbox[1], c0_bbox[2], c0_bbox[3]

        for c in self.children[1:]:
            cb = c.bbox
            if cb[0] < minx: minx = cb[0]
            if cb[1] < miny: miny = cb[1]
            if cb[2] > maxx: maxx = cb[2]
            if cb[3] > maxy: maxy = cb[3]

        self.bbox = (minx, miny, maxx, maxy)

        # 2. Calculating the size of the child subtree in bytes
        if self.level == 0:
            # Level 0 (Bottom of the tree): Children are raw geometry (Data Nodes)
            child_payload_size = len(self.children) * HWConfig.DATA_NODE_SIZE
        else:
            # Level > 0 (Macro-nodes): Children are other RTreeNodes
            child_payload_size = sum(c.bin_size for c in self.children)

        # Hardware jump = size of the entire tree under this node
        self.v3_jump = child_payload_size
        # Own size in binary = 28 bytes (the Nav Node itself) + the whole subtree
        self.bin_size = HWConfig.NAV_NODE_SIZE + child_payload_size

    def pack(self) -> bytes:
        """
        Recursive packing of C-Union tree structures into a binary stream.
        """
        data = bytearray(struct.pack(
            "<IiiiiII",
            self.v3_jump,
            self.bbox[0], self.bbox[1],
            self.bbox[2], self.bbox[3],
            self.level,
            len(self.children)
        ))

        for child in self.children:
            if self.level == 0:
                data.extend(child.pack_data_node())
            else:
                data.extend(child.pack())

        return bytes(data)
