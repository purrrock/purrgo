#!/usr/bin/env python3
"""Создаёт эталонные книги для tests/host/test_pgb.c.

Запуск из корня репозитория:  python3 tests/host/data/make_sample_pgb.py
Нужен пакет lz4 (python3 -m pip install lz4): sample.pgb сжимается его кодером (HC),
то есть декодер на C проверяется на «чужом» кодере, а не только на собственном.

Файлы (одно содержимое, 7 страниц):
  sample.pgb      сжатие LZ4 Block, кодер пакета lz4;
  sample_py.pgb   сжатие LZ4 Block, встроенный кодер конвертера;
  sample_raw.pgb  без сжатия.

Содержимое (названия и текст детерминированы):
  строки 1..160     «Строка NNN» + (для NNN % 10 != 0) пробел и NNN % 10 символов 'x';
  строки 161..192   32 строки по 29 случайных символов [A-Za-z0-9] (LCG, seed 12345) -
                    такая страница не сжимается и хранится в сжатой книге как есть;
  строки 193..202   «Хвост NN».
Страницы: 6 по 32 строки и 7-я из 10 строк. Название «Тест PGB», автор «Автор».
"""
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / "tools" / "book-compiler"))
import purrgo_book_compiler as bc  # noqa: E402

ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"


def sample_lines() -> list[str]:
    lines = []
    for i in range(1, 161):
        tail = (" " + "x" * (i % 10)) if i % 10 else ""
        lines.append(f"Строка {i:03d}{tail}")
    state = 12345
    for _ in range(32):
        chars = []
        for _ in range(29):
            state = (state * 1664525 + 1013904223) & 0xFFFFFFFF
            chars.append(ALPHABET[(state >> 16) % len(ALPHABET)])
        lines.append("".join(chars))
    for i in range(1, 11):
        lines.append(f"Хвост {i:02d}")
    return lines


def build(compression: int) -> bytes:
    blocks = [bc.Block("v", line) for line in sample_lines()]
    pages = bc.paginate(bc.blocks_to_lines(blocks, indent=0))
    notes: list[str] = []
    data = bc.build_pgb(pages, bc.encode_meta("Тест PGB", 200, "t", notes),
                        bc.encode_meta("Автор", 100, "a", notes), compression=compression)
    bc.verify_pgb(data)
    return data


def main() -> None:
    here = Path(__file__).parent
    if not bc.lz4_library_available():
        raise SystemExit("Нужен пакет lz4: python3 -m pip install lz4")
    outputs = {}
    outputs["sample.pgb"] = build(bc.COMPRESSION_LZ4)
    original = bc.lz4_compress_block
    bc.lz4_compress_block = bc.lz4_compress_block_py
    try:
        outputs["sample_py.pgb"] = build(bc.COMPRESSION_LZ4)
    finally:
        bc.lz4_compress_block = original
    outputs["sample_raw.pgb"] = build(bc.COMPRESSION_NONE)
    for name, data in outputs.items():
        (here / name).write_bytes(data)
        stats = bc.verify_pgb(data)
        print(f"{name}: {len(data)} bytes, {stats['pages']} pages, {stats['packed_pages']} packed")


if __name__ == "__main__":
    main()
