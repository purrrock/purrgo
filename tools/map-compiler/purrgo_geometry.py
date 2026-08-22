#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
Geometry helpers for PurrGO map compiler.

Contains:
- POIGeometryFactory: generator of low-polygon POI primitives
  (triangle, rhombus, cross, etc.)
- PERSPECTIVE_Y_MULTIPLIER constant (1.5) for ATS3085S compensation
- is_clockwise(points): CW winding rule checker

API mirrors original usage in purrgo_map_compiler.py:
POIGeometryFactory.generate_polygon(shape_type, center_lon, center_lat)
and is_clockwise(points) which accepts a sequence of (lon, lat) tuples
(closed or open) and returns True if ring is clockwise.
"""

from typing import List, Tuple
from math import radians, cos, pi

EARTH_RADIUS = 6378137.0
PERSPECTIVE_Y_MULTIPLIER = 1.5
R = 4.1




def is_clockwise(points: List[Tuple[int, int]]) -> bool:
    """Check ring orientation using signed area.

    Accepts sequence of (lon, lat) tuples. Works with closed rings
    (first == last) or open rings.
    Returns True if ring is clockwise (CW winding), False otherwise.
    """
    if not points:
        return False

    total = 0.0
    n = len(points)
    for i in range(n):
        x1, y1 = points[i]
        x2, y2 = points[(i + 1) % n]
        total += (x1 * y2 - x2 * y1)

    # Negative signed area indicates clockwise in the original implementation
    return total < 0.0
