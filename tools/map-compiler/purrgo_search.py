#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
search.idx for PurrGO map format V4 (docs/map-format-v4.md, section 21).

Contains:
  * normalization of names to search keys (21.7),
  * representative point and merging of non-POI objects (21.5, 21.6),
  * search_categories.csv loader and subtype matching (21.8),
  * search.idx writer: SRCH header, dictionary, NAME and CAT sections.
"""

import csv
import math
import struct
import sys
from typing import Dict, List, Optional, Sequence, Tuple

from purrgo_models import pgo_encode

# POI classes that are not a meaningful search target (barriers, code 152).
SEARCH_EXCLUDED_CODES = frozenset({152})

SEARCH_ENTRY_SIZE = 32
SEARCH_KEY_SIZE = 16
SEARCH_HEADER_SIZE = 64
SEARCH_DICT_ENTRY_SIZE = 48
SEARCH_MAX_SECONDARY = 3
SECTION_ALIGN = 512

# Layers of named objects besides POI (21.4): index = position of the layer in the
# header table layer_records[]; the layer is chosen by the feature code range (8.1.1).
SEARCH_LAYERS = ("roads", "waterways", "water", "landuse")
SEARCH_LAYER_CODE_RANGES = ((0, 31), (32, 63), (64, 95), (96, 127))

# Objects of a layer with the same name whose representative points fall into the same
# cell are one entry (21.5): a long street is not thousands of ways. The cell is
# 0.05 degrees (~5.5 km north-south, ~3.3 km east-west at 53 N), in 1e-7 degree units.
SEARCH_MERGE_CELL = 500_000

FLAG_SECONDARY = 0x01
FLAG_UNNAMED = 0x02
FLAG_KEY_TRUNC = 0x04

# poi.flags from purrgo_osmparser (POI_NAME_SUBSTITUTED | POI_NAME_EMPTY)
_POI_UNNAMED_MASK = 0x03


# ---------------------------------------------------------------------------
# Key normalization (21.7)
# ---------------------------------------------------------------------------

def _build_fold_table() -> bytes:
    table = bytearray(range(256))
    for b in range(0x41, 0x5B):
        table[b] = b + 0x20
    for b in range(0x80, 0xA0):
        table[b] = b + 0x20
    for b in range(0xC0, 0xD0):
        table[b] = b + 0x10
    table[0xF0] = 0xA5
    table[0xF1] = 0xA5
    return bytes(table)


def _build_word_table() -> bytes:
    """1 for word bytes (after folding), 0 for separators."""
    t = bytearray(256)
    for lo, hi in ((0x30, 0x39), (0x61, 0x7A), (0xA0, 0xBF),
                   (0xD0, 0xDF), (0xE0, 0xEF)):
        for b in range(lo, hi + 1):
            t[b] = 1
    return bytes(t)


FOLD_TABLE = _build_fold_table()
WORD_TABLE = _build_word_table()


def split_words(raw: bytes) -> List[bytes]:
    """Fold PurrGO-256 bytes and cut them into words (not truncated)."""
    words: List[bytes] = []
    cur = bytearray()
    for b in raw:
        f = FOLD_TABLE[b]
        if WORD_TABLE[f]:
            cur.append(f)
        elif cur:
            words.append(bytes(cur))
            cur = bytearray()
    if cur:
        words.append(bytes(cur))
    return words


def morton(lon: int, lat: int) -> int:
    """64-bit Morton code of a point (21.6)."""
    def spread(v: int) -> int:
        v &= 0xFFFFFFFF
        v = (v | (v << 16)) & 0x0000FFFF0000FFFF
        v = (v | (v << 8)) & 0x00FF00FF00FF00FF
        v = (v | (v << 4)) & 0x0F0F0F0F0F0F0F0F
        v = (v | (v << 2)) & 0x3333333333333333
        v = (v | (v << 1)) & 0x5555555555555555
        return v
    x = lon + 1_800_000_000
    y = lat + 900_000_000
    return spread(x) | (spread(y) << 1)


# ---------------------------------------------------------------------------
# search_categories.csv (21.8.1)
# ---------------------------------------------------------------------------

class SearchCategories:
    """Subtype rules and keyword dictionary. Disabled until load()."""

    # (subtype, [(key, value)]) strictly top-to-bottom
    RULES: List[Tuple[int, List[Tuple[str, str]]]] = []
    # (group, subtype or 0 for a group row, keyword bytes list)
    KEYWORDS: List[Tuple[str, int, List[bytes]]] = []
    loaded: bool = False

    @classmethod
    def reset(cls) -> None:
        cls.RULES = []
        cls.KEYWORDS = []
        cls.loaded = False

    @staticmethod
    def _keyword(text: str, where: str) -> bytes:
        words = split_words(pgo_encode(text, 100).rstrip(b"\x00"))
        if len(words) != 1:
            raise ValueError(f"{where}: keyword '{text}' must be exactly one word")
        if len(words[0]) > SEARCH_KEY_SIZE:
            raise ValueError(f"{where}: keyword '{text}' is longer than "
                             f"{SEARCH_KEY_SIZE} bytes")
        return words[0]

    @classmethod
    def load(cls, filepath: str) -> None:
        print(f"[>] Loading search categories from {filepath}...")
        cls.reset()
        groups_seen = set()
        subtype_group: Dict[int, str] = {}
        try:
            with open(filepath, "r", encoding="utf-8", newline="") as f:
                reader = csv.reader(f, delimiter=";")
                header = next(reader, None)
                if not header or [h.strip() for h in header[:5]] != [
                        "Group", "Subtype", "Keywords_RU", "Keywords_EN", "OSM_Tags"]:
                    raise ValueError("unexpected header, expected "
                                     "Group;Subtype;Keywords_RU;Keywords_EN;OSM_Tags")
                for lineno, row in enumerate(reader, start=2):
                    if not row or all(not c.strip() for c in row):
                        continue
                    if len(row) < 5:
                        raise ValueError(f"line {lineno}: expected 5 columns")
                    where = f"line {lineno}"
                    group = row[0].strip()
                    if not group:
                        raise ValueError(f"{where}: empty Group")
                    sub_s = row[1].strip()
                    tags_s = row[4].strip()
                    kws: List[bytes] = []
                    for col in (row[2], row[3]):
                        for kw in col.split(","):
                            if kw.strip():
                                kws.append(cls._keyword(kw.strip(), where))
                    if not sub_s and not tags_s:
                        cls.KEYWORDS.append((group, 0, kws))  # group row
                        groups_seen.add(group)
                        continue
                    if not sub_s or not tags_s:
                        raise ValueError(f"{where}: Subtype and OSM_Tags must be "
                                         "both set (subtype row) or both empty (group row)")
                    subtype = int(sub_s)
                    if not 1 <= subtype <= 255:
                        raise ValueError(f"{where}: subtype {subtype} outside 1..255")
                    if subtype_group.setdefault(subtype, group) != group:
                        raise ValueError(f"{where}: subtype {subtype} used in two groups")
                    tags: List[Tuple[str, str]] = []
                    for pair in tags_s.split(","):
                        if "=" not in pair:
                            raise ValueError(f"{where}: bad OSM_Tags '{tags_s}'")
                        k, v = pair.split("=", 1)
                        tags.append((k.strip(), v.strip()))
                    cls.RULES.append((subtype, tags))
                    cls.KEYWORDS.append((group, subtype, kws))
        except FileNotFoundError:
            print(f"[-] Error: {filepath} not found.")
            sys.exit(1)
        except ValueError as e:
            print(f"[-] Error parsing {filepath}: {e}")
            sys.exit(1)
        cls.loaded = True
        print(f"    Subtype rules: {len(cls.RULES)}")

    @classmethod
    def subtype_for(cls, tags: Dict[str, str]) -> int:
        """First matching row wins; 0 if none (or categories not loaded)."""
        if not cls.loaded:
            return 0
        for subtype, rule_tags in cls.RULES:
            for k, v in rule_tags:
                tv = tags.get(k)
                if tv is None or (v != "*" and tv != v):
                    break
            else:
                return subtype
        return 0

    @classmethod
    def dictionary(cls) -> List[Tuple[bytes, bytes]]:
        """Sorted unique (keyword padded to 16, 32-byte subtype mask)."""
        group_subtypes: Dict[str, List[int]] = {}
        for group, subtype, _ in cls.KEYWORDS:
            if subtype:
                group_subtypes.setdefault(group, []).append(subtype)
        masks: Dict[bytes, int] = {}
        for group, subtype, kws in cls.KEYWORDS:
            subs = [subtype] if subtype else group_subtypes.get(group, [])
            bits = 0
            for s in subs:
                bits |= 1 << s
            for kw in kws:
                key = kw.ljust(SEARCH_KEY_SIZE, b"\x00")
                masks[key] = masks.get(key, 0) | bits
        out = []
        for key in sorted(masks):
            out.append((key, masks[key].to_bytes(32, "little")))
        return out



# ---------------------------------------------------------------------------
# Objects of the line and polygon layers (21.5, 21.6)
# ---------------------------------------------------------------------------

def layer_index_for_code(code: int) -> int:
    """Layer of a feature code: 0 roads, 1 waterways, 2 water, 3 landuse; -1 otherwise."""
    for i, (lo, hi) in enumerate(SEARCH_LAYER_CODE_RANGES):
        if lo <= code <= hi:
            return i
    return -1


def anchor_point(feature) -> Tuple[int, int]:
    """
    Representative point of a line or polygon object (21.5): the point the map is
    centred on when the user picks the object in the results.

    * line: the middle vertex;
    * polygon: the vertex of the outer ring (the first part) closest to the centre of
      the bounding box (longitudes are scaled by the cosine of the latitude, so that
      "closest" is meant on the ground); the first one wins a tie.
    """
    pts = list(struct.iter_unpack("<ii", feature.points))
    if not pts:
        return feature.bbox[0], feature.bbox[1]
    layer = layer_index_for_code(feature.code)
    if layer in (0, 1):
        return pts[len(pts) // 2]
    parts = list(feature.parts) if feature.parts else [0]
    end = parts[1] if len(parts) > 1 else len(pts)
    ring = pts[parts[0]:end] or pts
    cx = (feature.bbox[0] + feature.bbox[2]) / 2.0
    cy = (feature.bbox[1] + feature.bbox[3]) / 2.0
    k = math.cos(math.radians(cy / 1e7))
    best = ring[0]
    best_d = None
    for x, y in ring:
        d = ((x - cx) * k) ** 2 + (y - cy) ** 2
        if best_d is None or d < best_d:
            best, best_d = (x, y), d
    return best


def _layer_entries(features: Sequence, keywords: frozenset) -> List[Tuple]:
    """
    Search entries of one non-POI layer: [(lon, lat, db_idx, code, subtype, words)].

    Only named objects (v2 >= 2: record 1 of the layer .db is the dummy one) are
    indexed. Objects with the same code range (one layer), the same folded name and
    the representative point in the same cell are merged; the first one in .db
    order is kept.
    """
    seen = set()
    out: List[Tuple] = []
    for f in sorted(features, key=lambda x: x.v2):
        if f.v2 < 2 or not f.name or not f.points:
            continue
        raw = pgo_encode(f.name, 100).rstrip(b"\x00")
        words = split_words(raw)
        if not words:
            continue
        lon, lat = anchor_point(f)
        cell = ((lon + 1_800_000_000) // SEARCH_MERGE_CELL,
                (lat + 900_000_000) // SEARCH_MERGE_CELL)
        key = (b" ".join(words), cell)
        if key in seen:
            continue
        seen.add(key)
        out.append((lon, lat, f.v2, f.code, f.subtype, words))
    return out

# ---------------------------------------------------------------------------
# search.idx writer
# ---------------------------------------------------------------------------

def _align(n: int) -> int:
    return (n + SECTION_ALIGN - 1) // SECTION_ALIGN * SECTION_ALIGN


def _pack_entry(lon: int, lat: int, db_idx: int, code: int, subtype: int,
                flags: int, key: bytes, key_len: int) -> bytes:
    return struct.pack("<iiIBBBB16s", lon, lat, db_idx, code, subtype, flags,
                       key_len, key)


def _name_rows_for(words: List[bytes], lon: int, lat: int, db_idx: int, code: int,
                   subtype: int, skip_secondary: frozenset) -> List[Tuple]:
    """NAME rows of one object: [(key16, db_idx, secondary, packed entry)] (21.5)."""
    rows: List[Tuple] = []
    w = words[0]
    first_key = w[:SEARCH_KEY_SIZE]
    first_flags = FLAG_KEY_TRUNC if len(w) > SEARCH_KEY_SIZE else 0
    kp = first_key.ljust(SEARCH_KEY_SIZE, b"\x00")
    rows.append((kp, db_idx, 0,
                 _pack_entry(lon, lat, db_idx, code, subtype, first_flags, kp,
                             len(first_key))))
    seen = {first_key}
    secondary = 0
    for w in words[1:]:
        if secondary >= SEARCH_MAX_SECONDARY:
            break
        if len(w) < 2:
            continue
        k = w[:SEARCH_KEY_SIZE]
        if k in seen or k in skip_secondary:
            continue
        seen.add(k)
        secondary += 1
        fl = FLAG_SECONDARY | (FLAG_KEY_TRUNC if len(w) > SEARCH_KEY_SIZE else 0)
        kp = k.ljust(SEARCH_KEY_SIZE, b"\x00")
        rows.append((kp, db_idx, 1,
                     _pack_entry(lon, lat, db_idx, code, subtype, fl, kp, len(k))))
    return rows


def build_search_index(pois: Sequence, db_records: int,
                       layers: Sequence[Tuple[Sequence, int]] = ()) -> bytes:
    """
    Whole search.idx file content (including the PGO header).

    `pois` are PoiFeature objects after compile_db (v2 = record number in
    pois.db); `db_records` is the number of records in pois.db.

    `layers` are the other named layers: [(features, db_records)] after compile_db,
    where db_records is the number of records of the layer .db including the dummy
    record 1. The layer is taken from the feature codes (SEARCH_LAYER_CODE_RANGES).
    Layers without named objects are not passed (their layer_records entry is 0).
    """
    name_rows: List[Tuple[bytes, int, int, bytes]] = []
    cat_rows: List[Tuple[int, int, int, bytes]] = []
    layer_records = [0] * len(SEARCH_LAYERS)

    for poi in pois:
        if poi.code in SEARCH_EXCLUDED_CODES:
            continue
        db_idx = poi.v2
        if db_idx < 1:
            raise ValueError(f"POI {poi.osm_id} has no pois.db record")
        lon, lat = poi.bbox[0], poi.bbox[1]
        subtype = getattr(poi, "subtype", 0)
        pflags = getattr(poi, "flags", 0)
        raw = pgo_encode(poi.name, 100).rstrip(b"\x00")
        words = split_words(raw)
        named = bool(raw) and not (pflags & _POI_UNNAMED_MASK)

        first_key = b""
        first_flags = 0
        if named and words:
            rows = _name_rows_for(words, lon, lat, db_idx, poi.code, subtype,
                                  frozenset())
            name_rows.extend(rows)
            first_key = words[0][:SEARCH_KEY_SIZE]
            if len(words[0]) > SEARCH_KEY_SIZE:
                first_flags |= FLAG_KEY_TRUNC

        if subtype:
            fl = first_flags if named else FLAG_UNNAMED
            kp = first_key.ljust(SEARCH_KEY_SIZE, b"\x00") if named else bytes(16)
            kl = len(first_key) if named else 0
            cat_rows.append((subtype, morton(lon, lat), db_idx,
                             _pack_entry(lon, lat, db_idx, poi.code, subtype,
                                         fl, kp, kl)))

    # Line and polygon layers: named objects only; the words equal to a dictionary
    # keyword ("улица", "river") are not indexed as secondary words: a query word that
    # is a keyword is a category word and never selects a NAME range (21.9).
    keywords = frozenset(k.rstrip(b"\x00") for k, _ in SearchCategories.dictionary())
    for feats, recs in layers:
        feats = list(feats)
        if not feats or recs < 2:
            continue
        li = layer_index_for_code(feats[0].code)
        if li < 0:
            raise ValueError(f"search: code {feats[0].code} is not a layer code")
        if layer_records[li]:
            raise ValueError(f"search: layer {SEARCH_LAYERS[li]} given twice")
        layer_records[li] = recs
        for lon, lat, db_idx, code, subtype, words in _layer_entries(feats, keywords):
            if layer_index_for_code(code) != li:
                raise ValueError(f"search: code {code} outside layer "
                                 f"{SEARCH_LAYERS[li]}")
            name_rows.extend(_name_rows_for(words, lon, lat, db_idx, code, subtype,
                                            keywords))
            if subtype:
                first_key = words[0][:SEARCH_KEY_SIZE]
                fl = FLAG_KEY_TRUNC if len(words[0]) > SEARCH_KEY_SIZE else 0
                cat_rows.append((subtype, morton(lon, lat), db_idx,
                                 _pack_entry(lon, lat, db_idx, code, subtype, fl,
                                             first_key.ljust(SEARCH_KEY_SIZE, b"\x00"),
                                             len(first_key))))

    # Order (21.5, 21.6). db_idx is unique only inside a layer, so the layer (code
    # range) comes before it: otherwise the order of NAME rows with an equal key would
    # depend on the order the layers were passed.
    def lkey(code_byte: int) -> int:
        return layer_index_for_code(code_byte) + 1 if code_byte < 128 else 0

    name_rows.sort(key=lambda r: (r[0], lkey(r[3][12]), r[1], r[2]))
    cat_rows.sort(key=lambda r: (r[0], r[1], lkey(r[3][12]), r[2]))

    dict_entries = SearchCategories.dictionary()
    dict_blob = struct.pack("<HHI", len(dict_entries), SEARCH_DICT_ENTRY_SIZE, 0)
    dict_blob += b"".join(k + m for k, m in dict_entries)

    dict_offset = 0x20 + SEARCH_HEADER_SIZE          # 0x60
    name_offset = _align(dict_offset + len(dict_blob))
    name_blob = b"".join(r[3] for r in name_rows)
    name_size = _align(len(name_blob))
    cat_offset = name_offset + name_size
    cat_blob = b"".join(r[3] for r in cat_rows)
    cat_size = _align(len(cat_blob))

    srch = struct.pack(
        "<4sHHIIIIIIIIII4I",
        b"SRCH", 1, SEARCH_ENTRY_SIZE, 0,
        db_records,
        name_offset, len(name_rows),
        cat_offset, len(cat_rows),
        dict_offset, len(dict_blob),
        0, 0,
        *layer_records,
    )
    assert len(srch) == SEARCH_HEADER_SIZE

    body = bytearray(srch)
    body += dict_blob
    body = bytearray(body.ljust(name_offset - 0x20, b"\x00"))
    body += name_blob.ljust(name_size, b"\x00")
    body += cat_blob.ljust(cat_size, b"\x00")

    header = (b"PGO" + bytes([4]) + struct.pack("<I", len(body))
              + bytes(24))
    return bytes(header) + bytes(body)
