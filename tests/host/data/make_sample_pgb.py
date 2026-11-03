#!/usr/bin/env python3
"""Создаёт tests/host/data/sample.pgb для tests/host/test_pgb.c.

Запуск из корня репозитория:  python3 tests/host/data/make_sample_pgb.py
Файл собирается независимой реализацией (tools/book-compiler, CRC из zlib), поэтому
тест проверяет C-читалку против компилятора, а не против самой себя.

Содержимое: 150 строк «Строка NNN» (после трёх цифр для i % 10 != 0 добавляется
пробел и i % 10 символов 'x'), без переносов и абзацных отступов; страниц 5
(4 по 32 строки и 22 строки на последней).
Название «Тест PGB», автор «Автор».
"""
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / "tools" / "book-compiler"))
import purrgo_book_compiler as bc  # noqa: E402

lines = []
for i in range(1, 151):
    tail = (" " + "x" * (i % 10)) if i % 10 else ""
    lines.append(f"Строка {i:03d}{tail}")

blocks = [bc.Block("v", line) for line in lines]
pages = bc.paginate(bc.blocks_to_lines(blocks, indent=0))
notes: list[str] = []
data = bc.build_pgb(pages, bc.encode_meta("Тест PGB", 200, "t", notes),
                    bc.encode_meta("Автор", 100, "a", notes))
bc.verify_pgb(data)
out = Path(__file__).with_name("sample.pgb")
out.write_bytes(data)
print(f"{out}: {len(data)} bytes, {len(pages)} pages")
