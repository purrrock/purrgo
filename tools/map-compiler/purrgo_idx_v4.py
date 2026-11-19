#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
Block-layout spatial index (.idx) of PurrGO map format V4
(docs/map-format-v4.md, sections 4-7 and 14).

A leaf is any object with `bbox` and `pack_data_node()` (MapFeature or
VariantLeaf). Nodes are packed with STR; the tree is serialised in 512-byte
blocks in breadth-first (level by level) order.
"""

import math
import struct
from typing import Any, List, Sequence, Tuple

from purrgo_models import HWConfig

BLOCK = HWConfig.BLOCK_SIZE
NAV_FANOUT = HWConfig.NAV_FANOUT
LEAF_FANOUT = HWConfig.LEAF_FANOUT
SQT_MAGIC = b"SQT\x02"
SQT_TOPOLOGY_BLOCKS = 2


class VariantLeaf:
    """Data node of a LOD 2/3 simplified geometry variant."""
    __slots__ = ("bbox", "code", "v1", "v2")

    def __init__(self, bbox, code: int, v1: int, v2: int):
        self.bbox = bbox
        self.code = code
        self.v1 = v1
        self.v2 = v2

    def pack_data_node(self) -> bytes:
        return struct.pack("<iiiiBII", *self.bbox, self.code, self.v1, self.v2)


class NavNode:
    __slots__ = ("level", "children", "bbox")

    def __init__(self, level: int, children: List[Any]):
        self.level = level
        self.children = children
        c0 = children[0].bbox
        minx, miny, maxx, maxy = c0[0], c0[1], c0[2], c0[3]
        for c in children[1:]:
            cb = c.bbox
            if cb[0] < minx:
                minx = cb[0]
            if cb[1] < miny:
                miny = cb[1]
            if cb[2] > maxx:
                maxx = cb[2]
            if cb[3] > maxy:
                maxy = cb[3]
        self.bbox = (minx, miny, maxx, maxy)


def str_pack(items: List[Any], fanout: int, level: int) -> List[NavNode]:
    """Sort-Tile-Recursive bulk loading into nodes of at most `fanout` children."""
    if not items:
        return []
    items.sort(key=lambda it: it.bbox[0] + it.bbox[2])
    num_nodes = math.ceil(len(items) / fanout)
    num_slices = math.ceil(math.sqrt(num_nodes))
    slice_capacity = num_slices * fanout
    nodes: List[NavNode] = []
    for i in range(0, len(items), slice_capacity):
        slc = items[i:i + slice_capacity]
        slc.sort(key=lambda it: it.bbox[1] + it.bbox[3])
        for j in range(0, len(slc), fanout):
            nodes.append(NavNode(level, slc[j:j + fanout]))
    return nodes


def build_tree(leaves: List[Any]) -> Tuple[int, List[NavNode]]:
    """Returns (depth, root nodes). Level 0 nodes hold data nodes (<= 20)."""
    if not leaves:
        return 0, []
    layer = str_pack(leaves, LEAF_FANOUT, 0)
    level = 0
    while len(layer) > NAV_FANOUT:
        level += 1
        layer = str_pack(layer, NAV_FANOUT, level)
    return level + 1, layer


def empty_section() -> bytes:
    return build_section([], 0)


def build_section(leaves: List[Any], section_start: int) -> bytes:
    """One LOD section (header sector + blocks) starting at file offset `section_start`."""
    depth, roots = build_tree(leaves)
    body = bytearray()
    if roots:
        # Block queue in breadth-first order == normative order of 7.3.
        blocks: List[Tuple[bool, List[Any]]] = [(False, roots)]   # (is_leaf_block, entries)
        i = 0
        while i < len(blocks):
            is_leaf, entries = blocks[i]
            buf = bytearray()
            if is_leaf:
                for e in entries:
                    buf += e.pack_data_node()
            else:
                for node in entries:
                    child_index = len(blocks)
                    blocks.append((node.level == 0, node.children))
                    child_off = section_start + BLOCK * (1 + child_index)
                    buf += struct.pack("<IiiiiII", child_off, *node.bbox,
                                       node.level, len(node.children))
            body += buf.ljust(BLOCK, b"\x00")
            i += 1
    section_size = BLOCK + len(body)
    header = struct.pack("<4sIIIIIHH", SQT_MAGIC, SQT_TOPOLOGY_BLOCKS, depth,
                         len(roots), BLOCK, section_size, NAV_FANOUT, LEAF_FANOUT)
    return header.ljust(BLOCK, b"\x00") + bytes(body)
