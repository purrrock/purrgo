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
  строки 1..120     «Строка NNN» + (для NNN % 10 != 0) пробел и NNN % 10 символов 'x';
  строки 121..144   24 строки по 29 случайных символов [A-Za-z0-9] (LCG, seed 12345) -
                    такая страница не сжимается и хранится в сжатой книге как есть;
  строки 145..154   «Хвост NN».
Страницы: 6 по 24 строки и 7-я из 10 строк. Название «Тест PGB», автор «Автор».

Отдельная книга sample_img.pgb (LZ4, кодер пакета lz4) со страницами-картинками, 4 страницы:
  0  картинка 176x256: диагональные полосы, чёрный пиксель, если ((x + y) // 8) % 2 == 0;
  1  текст «Between» / «images»;
  2  картинка 13x5 (нечётная ширина, биты выравнивания нулевые): чёрный пиксель - бит 16 LCG (seed 777);
  3  картинка 100x60: то же, LCG seed 4242 (не сжимается, хранится как есть).
Название «Image book», автор «Pic».
"""
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / "tools" / "book-compiler"))
import purrgo_book_compiler as bc  # noqa: E402

ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"


def sample_lines() -> list[str]:
    lines = []
    for i in range(1, 121):
        tail = (" " + "x" * (i % 10)) if i % 10 else ""
        lines.append(f"Строка {i:03d}{tail}")
    state = 12345
    for _ in range(24):
        chars = []
        for _ in range(29):
            state = (state * 1664525 + 1013904223) & 0xFFFFFFFF
            chars.append(ALPHABET[(state >> 16) % len(ALPHABET)])
        lines.append("".join(chars))
    for i in range(1, 11):
        lines.append(f"Хвост {i:02d}")
    return lines


def lcg_image(width: int, height: int, seed: int) -> "bc.ImagePage":
    mask = bytearray()
    state = seed
    for _ in range(width * height):
        state = (state * 1664525 + 1013904223) & 0xFFFFFFFF
        mask.append((state >> 16) & 1)
    return bc.ImagePage(width, height, bc.pack_mask(bytes(mask), width, height))


def stripes_image(width: int, height: int) -> "bc.ImagePage":
    mask = bytes(1 if ((x + y) // 8) % 2 == 0 else 0 for y in range(height) for x in range(width))
    return bc.ImagePage(width, height, bc.pack_mask(mask, width, height))


def build_images() -> bytes:
    pages = [stripes_image(176, 256), b"Between\nimages", lcg_image(13, 5, 777), lcg_image(100, 60, 4242)]
    notes: list[str] = []
    data = bc.build_pgb(pages, bc.encode_meta("Image book", 200, "t", notes),
                        bc.encode_meta("Pic", 100, "a", notes), compression=bc.COMPRESSION_LZ4)
    bc.verify_pgb(data)
    return data


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
    outputs["sample_img.pgb"] = build_images()
    for name, data in outputs.items():
        (here / name).write_bytes(data)
        stats = bc.verify_pgb(data)
        print(f"{name}: {len(data)} bytes, {stats['pages']} pages, {stats['packed_pages']} packed")


if __name__ == "__main__":
    main()
