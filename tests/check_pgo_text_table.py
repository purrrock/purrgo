#!/usr/bin/env python3
"""Сверяет таблицу src/core/pgo_text.c с _PGO_CHAR_MAP из purrgo_models.py.

Запуск из корня репозитория:  python3 tests/check_pgo_text_table.py
Код возврата 0 - таблицы совпадают.
"""
import re
import sys
from pathlib import Path

root = Path(__file__).resolve().parent.parent
models = (root / "tools/map-compiler/purrgo_models.py").read_text(encoding="utf-8")
start, end = models.index("_PGO_CHAR_MAP = {"), models.index("class HWConfig")
ns = {}
exec(models[start:end], ns)
py = {ord(k): v for k, v in ns["_PGO_CHAR_MAP"].items() if ord(k) >= 0x80}

c = (root / "src/core/pgo_text.c").read_text(encoding="utf-8")
body = c[c.index("k_pgo_map[] = {"):]
body = body[:body.index("};")]
cc = {int(a, 16): int(b, 16) for a, b in re.findall(r"\{\s*0x([0-9A-Fa-f]{4}),\s*0x([0-9A-Fa-f]{2})\s*\}", body)}
order = [int(a, 16) for a, _ in re.findall(r"\{\s*0x([0-9A-Fa-f]{4}),\s*0x([0-9A-Fa-f]{2})\s*\}", body)]

ok = True
if order != sorted(order) or len(order) != len(set(order)):
    print("таблица в pgo_text.c не отсортирована или есть дубликаты"); ok = False
for cp in sorted(set(py) | set(cc)):
    if py.get(cp) != cc.get(cp):
        print(f"U+{cp:04X}: python={py.get(cp)} c={cc.get(cp)}"); ok = False
print("OK" if ok else "РАСХОЖДЕНИЕ", f"({len(py)} символов)")
sys.exit(0 if ok else 1)
