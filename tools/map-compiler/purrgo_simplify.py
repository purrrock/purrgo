#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
Geometry simplification for LOD 2 and LOD 3 variants
(docs/map-format-v4.md, sections 3.5 and 14.4).

Douglas-Peucker, distance in metres (longitude scaled by cos(latitude), one
value per object). The result is always a subset of the original points in
the original order; first and last point of every part are kept.
"""

import struct
from math import cos, radians
from typing import List, Optional, Sequence, Tuple

import numpy as np

# 1 m ~ 89.8 units of 1e-7 degree (111 320 m per degree)
UNITS_PER_METER = 1e7 / 111320.0

# Limits of one geometry record (section 3.4.1)
MAX_POINTS = 2000
MAX_PARTS = 32

UNCHANGED = "unchanged"
DROPPED = "dropped"

Variant = Tuple[bytes, Tuple[int, ...], Tuple[int, int, int, int]]


def _dp_keep(pts: np.ndarray, tol: float) -> np.ndarray:
    """Boolean keep-mask for float points (n x 2), tol in the same units."""
    n = len(pts)
    keep = np.zeros(n, dtype=bool)
    keep[0] = keep[-1] = True
    stack = [(0, n - 1)]
    while stack:
        lo, hi = stack.pop()
        if hi - lo < 2:
            continue
        a = pts[lo]
        b = pts[hi]
        seg = pts[lo + 1:hi] - a
        d = b - a
        len2 = d[0] * d[0] + d[1] * d[1]
        if len2 == 0.0:
            dist = np.hypot(seg[:, 0], seg[:, 1])
        else:
            dist = np.abs(seg[:, 0] * d[1] - seg[:, 1] * d[0]) / np.sqrt(len2)
        i = int(np.argmax(dist))
        if dist[i] > tol:
            m = lo + 1 + i
            keep[m] = True
            stack.append((lo, m))
            stack.append((m, hi))
    return keep


def _signed_area2(pts: np.ndarray) -> float:
    x = pts[:, 0].astype(np.float64)
    y = pts[:, 1].astype(np.float64)
    return float(np.sum(x[:-1] * y[1:] - x[1:] * y[:-1]))


def _self_intersects(pts: np.ndarray) -> bool:
    """True if a closed ring (first == last) has two crossing edges."""
    p = pts.astype(np.float64)
    n = len(p) - 1                      # number of edges
    if n < 4:
        return False
    a = p[:-1]
    b = p[1:]

    def orient(o, q, r):
        return (q[..., 0] - o[..., 0]) * (r[..., 1] - o[..., 1]) - \
               (q[..., 1] - o[..., 1]) * (r[..., 0] - o[..., 0])

    for i in range(n):
        j0 = i + 2
        j1 = n - 1 if i == 0 else n     # skip adjacent edges (and closing one)
        if j0 >= j1:
            continue
        c = a[j0:j1]
        d = b[j0:j1]
        o1 = orient(a[i], b[i], c)
        o2 = orient(a[i], b[i], d)
        o3 = orient(c, d, a[i])
        o4 = orient(c, d, b[i])
        if np.any((o1 * o2 < 0) & (o3 * o4 < 0)):
            return True
    return False


def _point_in_ring(px: float, py: float, ring: np.ndarray) -> bool:
    x = ring[:, 0].astype(np.float64)
    y = ring[:, 1].astype(np.float64)
    x1, y1, x2, y2 = x[:-1], y[:-1], x[1:], y[1:]
    cond = (y1 > py) != (y2 > py)
    with np.errstate(divide="ignore", invalid="ignore"):
        xin = (x2 - x1) * (py - y1) / (y2 - y1) + x1
    return bool(np.count_nonzero(cond & (px < xin)) % 2)


def _bbox(pts: np.ndarray) -> Tuple[int, int, int, int]:
    return (int(pts[:, 0].min()), int(pts[:, 1].min()),
            int(pts[:, 0].max()), int(pts[:, 1].max()))


def _split(pts: np.ndarray, parts: Sequence[int]) -> List[np.ndarray]:
    bounds = list(parts) + [len(pts)]
    return [pts[bounds[i]:bounds[i + 1]] for i in range(len(parts))]


def simplify_line(points: bytes, parts: Sequence[int], tol_m: float):
    """Simplify a line feature. UNCHANGED or a Variant."""
    pts = np.frombuffer(points, dtype="<i4").reshape(-1, 2)
    out_parts: List[np.ndarray] = []
    changed = False
    for part in _split(pts, parts):
        if len(part) <= 2:
            out_parts.append(part)
            continue
        lat = (int(part[:, 1].min()) + int(part[:, 1].max())) / 2e7
        scale = np.array([cos(radians(lat)), 1.0])
        keep = _dp_keep(part.astype(np.float64) * scale, tol_m * UNITS_PER_METER)
        if not keep.all():
            changed = True
            part = part[keep]
        out_parts.append(part)
    if not changed:
        return UNCHANGED
    return _pack(out_parts)


def simplify_polygon(points: bytes, parts: Sequence[int], tol_m: float):
    """
    Simplify a polygon feature (rules of section 3.5, item 6).
    UNCHANGED, DROPPED (no outer ring left) or a Variant.

    A ring whose simplified form would cross itself keeps its full geometry.
    Crossings between different rings are not checked.
    """
    pts = np.frombuffer(points, dtype="<i4").reshape(-1, 2)
    rings = _split(pts, parts)
    tol_units = tol_m * UNITS_PER_METER
    result: List[Optional[np.ndarray]] = []
    changed = False
    for ring in rings:
        if len(ring) <= 4:
            result.append(ring)
            continue
        lat = (int(ring[:, 1].min()) + int(ring[:, 1].max())) / 2e7
        scale = np.array([cos(radians(lat)), 1.0])
        keep = _dp_keep(ring.astype(np.float64) * scale, tol_units)
        if keep.all():
            result.append(ring)
            continue
        simp = ring[keep]
        old_area = _signed_area2(ring)
        new_area = _signed_area2(simp)
        scaled_area = abs(new_area) / 2.0 * (scale[0] / (UNITS_PER_METER ** 2))
        degenerate = (len(simp) < 4 or scaled_area < tol_m * tol_m
                      or (old_area < 0) != (new_area < 0)
                      or (new_area == 0.0))
        if degenerate:
            changed = True
            result.append(None)
        elif _self_intersects(simp):
            result.append(ring)         # keep the full ring
        else:
            changed = True
            result.append(simp)
    if not changed:
        return UNCHANGED

    # Orientation tells outer (CW, negative area here as in is_clockwise) from hole.
    def is_outer(r):
        return _signed_area2(r) < 0

    removed_outers = [rings[i] for i, r in enumerate(result)
                      if r is None and is_outer(rings[i])]
    final: List[np.ndarray] = []
    outers_left = 0
    for orig, r in zip(rings, result):
        if r is None:
            continue
        if not is_outer(orig):
            f = orig[0]
            if any(_point_in_ring(float(f[0]), float(f[1]), ro)
                   for ro in removed_outers):
                continue                # hole of a removed outer ring
        else:
            outers_left += 1
        final.append(r)
    if outers_left == 0:
        return DROPPED
    return _pack(final)


def _pack(out_parts: Sequence[np.ndarray]) -> Variant:
    new_parts = []
    idx = 0
    for p in out_parts:
        new_parts.append(idx)
        idx += len(p)
    allpts = np.concatenate(out_parts).astype("<i4")
    if idx > MAX_POINTS or len(new_parts) > MAX_PARTS:
        raise ValueError("simplified geometry exceeds record limits")
    return allpts.tobytes(), tuple(new_parts), _bbox(allpts)
