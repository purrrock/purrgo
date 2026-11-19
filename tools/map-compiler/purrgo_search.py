#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
search.idx for PurrGO map format V4 (docs/map-format-v4.md, section 21).

Contains:
  * normalization of names to search keys (21.7),
  * search_categories.csv loader and subtype matching (21.8),
  * search.idx writer: SRCH header, dictionary, NAME and CAT sections.
"""

import csv
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
# search.idx writer
# ---------------------------------------------------------------------------

def _align(n: int) -> int:
    return (n + SECTION_ALIGN - 1) // SECTION_ALIGN * SECTION_ALIGN


def _pack_entry(lon: int, lat: int, db_idx: int, code: int, subtype: int,
                flags: int, key: bytes, key_len: int) -> bytes:
    return struct.pack("<iiIBBBB16s", lon, lat, db_idx, code, subtype, flags,
                       key_len, key)


def build_search_index(pois: Sequence, db_records: int) -> bytes:
    """
    Whole search.idx file content (including the PGO header).

    `pois` are PoiFeature objects after compile_db (v2 = record number in
    pois.db); `db_records` is the number of records in pois.db.
    """
    name_rows: List[Tuple[bytes, int, int, bytes]] = []
    cat_rows: List[Tuple[int, int, int, bytes]] = []

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
            w = words[0]
            first_key = w[:SEARCH_KEY_SIZE]
            if len(w) > SEARCH_KEY_SIZE:
                first_flags |= FLAG_KEY_TRUNC
            name_rows.append((
                first_key.ljust(SEARCH_KEY_SIZE, b"\x00"), db_idx, 0,
                _pack_entry(lon, lat, db_idx, poi.code, subtype, first_flags,
                            first_key.ljust(SEARCH_KEY_SIZE, b"\x00"),
                            len(first_key))))
            seen = {first_key}
            secondary = 0
            for w in words[1:]:
                if secondary >= SEARCH_MAX_SECONDARY:
                    break
                if len(w) < 2:
                    continue
                k = w[:SEARCH_KEY_SIZE]
                if k in seen:
                    continue
                seen.add(k)
                secondary += 1
                fl = FLAG_SECONDARY | (FLAG_KEY_TRUNC if len(w) > SEARCH_KEY_SIZE else 0)
                kp = k.ljust(SEARCH_KEY_SIZE, b"\x00")
                name_rows.append((kp, db_idx, 1,
                                  _pack_entry(lon, lat, db_idx, poi.code, subtype,
                                              fl, kp, len(k))))

        if subtype:
            fl = first_flags if named else FLAG_UNNAMED
            kp = first_key.ljust(SEARCH_KEY_SIZE, b"\x00") if named else bytes(16)
            kl = len(first_key) if named else 0
            cat_rows.append((subtype, morton(lon, lat), db_idx,
                             _pack_entry(lon, lat, db_idx, poi.code, subtype,
                                         fl, kp, kl)))

    name_rows.sort(key=lambda r: (r[0], r[1], r[2]))
    cat_rows.sort(key=lambda r: (r[0], r[1], r[2]))

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
        "<4sHHIIIIIIIIII16s",
        b"SRCH", 1, SEARCH_ENTRY_SIZE, 0,
        db_records,
        name_offset, len(name_rows),
        cat_offset, len(cat_rows),
        dict_offset, len(dict_blob),
        0, 0,
        bytes(16),
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
