#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Конвертер plain-text в бинарный формат электронных книг PGB 1.0 для PurrGO.

Зависимость:
    python -m pip install -r requirements.txt

Примеры использования:
    python purrgo_book_compiler.py book.txt                  # Выходной файл станет book.pgb
    python purrgo_book_compiler.py book.txt custom.pgb --encoding cp1251 --title "Название книги"
    python purrgo_book_compiler.py book.pgb --verify         # Проверка готового .pgb файла и вывод 1-й и последней страниц
"""

from __future__ import annotations

import argparse
import struct
import sys
import unicodedata
import zlib
from pathlib import Path
from typing import Iterable

try:
    import lz4.block
except ImportError:
    lz4 = None
else:
    lz4 = lz4


# Константы бинарного формата PGB 1.0.
MAGIC = b"PGB1"
VERSION_MAJOR = 1
VERSION_MINOR = 0
HEADER_SIZE = 64
INDEX_ENTRY_SIZE = 12
MAX_LINE_CHARS = 29
MAX_PAGE_LINES = 32
MAX_PAGE_UNCOMPRESSED = MAX_LINE_CHARS * MAX_PAGE_LINES + (MAX_PAGE_LINES - 1)
CRC_OFFSET = 36
UINT16_MAX = 0xFFFF
UINT32_MAX = 0xFFFFFFFF

# Формат заголовка: все целые поля little-endian.
HEADER_STRUCT = struct.Struct("<4sHHHHIIIIIHHI24s")
INDEX_STRUCT = struct.Struct("<III")

assert HEADER_STRUCT.size == HEADER_SIZE
assert INDEX_STRUCT.size == INDEX_ENTRY_SIZE
assert MAX_PAGE_UNCOMPRESSED == 959


# Таблица PurrGO-256
PGO_CHAR_MAP: dict[str, int] = {
    "№": 0x4E,
    "«": 0x22,
    "»": 0x22,
    "“": 0x22,
    "”": 0x22,
    "„": 0x22,
    "’": 0x27,
    "‘": 0x27,
    "`": 0x27,
    "´": 0x27,
    "ʼ": 0x27,

    "А": 0x80, "Б": 0x81, "В": 0x82, "Г": 0x83, "Д": 0x84, "Е": 0x85,
    "Ж": 0x86, "З": 0x87, "И": 0x88, "Й": 0x89, "К": 0x8A, "Л": 0x8B,
    "М": 0x8C, "Н": 0x8D, "О": 0x8E, "П": 0x8F, "Р": 0x90, "С": 0x91,
    "Т": 0x92, "У": 0x93, "Ф": 0x94, "Х": 0x95, "Ц": 0x96, "Ч": 0x97,
    "Ш": 0x98, "Щ": 0x99, "Ъ": 0x9A, "Ы": 0x9B, "Ь": 0x9C, "Э": 0x9D,
    "Ю": 0x9E, "Я": 0x9F,

    "а": 0xA0, "б": 0xA1, "в": 0xA2, "г": 0xA3, "д": 0xA4, "е": 0xA5,
    "ж": 0xA6, "з": 0xA7, "и": 0xA8, "й": 0xA9, "к": 0xAA, "л": 0xAB,
    "м": 0xAC, "н": 0xAD, "о": 0xAE, "п": 0xAF, "р": 0xB0, "с": 0xB1,
    "т": 0xB2, "у": 0xB3, "ф": 0xB4, "х": 0xB5, "ц": 0xB6, "ч": 0xB7,
    "ш": 0xB8, "щ": 0xB9, "ъ": 0xBA, "ы": 0xBB, "ь": 0xBC, "э": 0xBD,
    "ю": 0xBE, "я": 0xBF,

    "Ґ": 0xC0, "Є": 0xC1, "І": 0xC2, "Ї": 0xC3, "Ў": 0xC4, "Ђ": 0xC5,
    "Ј": 0xC6, "Љ": 0xC7, "Њ": 0xC8, "Џ": 0xC9, "Ѓ": 0xCA, "Ѕ": 0xCB,
    "Ғ": 0xCC, "Қ": 0xCD, "Ң": 0xCE, "Ұ": 0xCF,
    "ґ": 0xD0, "є": 0xD1, "і": 0xD2, "ї": 0xD3, "ў": 0xD4, "ђ": 0xD5,
    "ј": 0xD6, "љ": 0xD7, "њ": 0xD8, "џ": 0xD9, "ѓ": 0xDA, "ѕ": 0xDB,
    "ғ": 0xDC, "қ": 0xDD, "ң": 0xDE, "ұ": 0xDF,
    "Ё": 0xF0, "ё": 0xF1,
}

LATIN_EXT = {
    0xE0: "ÄäÀàÂâÃãÅå",
    0xE1: "ÖöÒòÔôÕõØø",
    0xE2: "ŬŭÜüÙùÛû",
    0xE3: "ß",
    0xE4: "ÇçĆćČč",
    0xE5: "ĞğĜĝ",
    0xE6: "İ",
    0xE7: "ı",
    0xE8: "ŠšŞşȘșŚś",
    0xE9: "Ąą",
    0xEA: "Ęę",
    0xEB: "ŁłĹĺ",
    0xEC: "ŃńŇňÑñ",
    0xED: "Óó",
    0xEE: "ŽžŹźŻż",
    0xEF: "Řř",
}
for _code, _chars in LATIN_EXT.items():
    for _char in _chars:
        PGO_CHAR_MAP[_char] = _code

# Обратная таблица для визуального контроля (--verify)
PGO_TO_UNICODE = {v: k for k, v in PGO_CHAR_MAP.items()}
for _i in range(128):
    PGO_TO_UNICODE.setdefault(_i, chr(_i))


def require_lz4() -> None:
    if lz4 is None:
        raise RuntimeError(
            "Не найден пакет 'lz4'. Установите зависимости командой: "
            "python -m pip install -r requirements.txt"
        )


def decode_source(raw: bytes, encoding: str) -> str:
    actual_encoding = "utf-8-sig" if encoding == "utf-8" else encoding
    try:
        return raw.decode(actual_encoding, errors="strict")
    except UnicodeDecodeError as exc:
        raise ValueError(
            f"Файл не декодируется как {encoding}: ошибка около байта {exc.start}. "
            "Проверьте --encoding."
        ) from exc


def unicode_to_pgo(char: str) -> int:
    codepoint = ord(char)
    if codepoint < 0x80:
        return codepoint
    return PGO_CHAR_MAP.get(char, ord("?"))


def pgo_bytes_to_text(page_bytes: bytes) -> str:
    return "".join(PGO_TO_UNICODE.get(b, "?") for b in page_bytes)


def normalize_source(text: str) -> str:
    text = text.replace("\r\n", "\n").replace("\r", "\n")
    text = text.replace("\t", " ")
    result: list[str] = []

    for char in text:
        if char == "\n":
            result.append(char)
        elif char == "\x00":
            result.append("?")
        elif unicodedata.category(char).startswith("C"):
            result.append(" ")
        else:
            result.append(char)

    return "".join(result)


def wrap_line(line: str, width: int = MAX_LINE_CHARS) -> list[str]:
    stripped = line.strip()
    if not stripped:
        return [""]

    words = stripped.split()
    output: list[str] = []
    current = ""

    for word in words:
        if len(word) > width:
            if current:
                output.append(current)
                current = ""
            while len(word) > width:
                output.append(word[:width])
                word = word[width:]
            if word:
                current = word
            continue

        candidate = word if not current else current + " " + word
        if len(candidate) <= width:
            current = candidate
        else:
            output.append(current)
            current = word

    if current:
        output.append(current)

    return output or [""]


def text_to_lines(text: str) -> list[str]:
    normalized = normalize_source(text)
    source_lines = normalized.split("\n")
    if source_lines and source_lines[-1] == "":
        source_lines.pop()

    lines: list[str] = []
    for line in source_lines:
        lines.extend(wrap_line(line))

    if not lines or (len(lines) == 1 and lines[0] == ""):
        raise ValueError("Исходный текст пуст: PGB 1.0 не допускает пустую страницу.")

    return lines


def paginate(lines: list[str]) -> list[bytes]:
    """Создаёт страницы максимум по 32 строки и кодирует их в PurrGO-256."""
    pages: list[bytes] = []

    for start in range(0, len(lines), MAX_PAGE_LINES):
        page_lines = lines[start:start + MAX_PAGE_LINES]
        page_text = "\n".join(page_lines)
        page_bytes = bytes(unicode_to_pgo(char) for char in page_text)
        
        # Гарантированно удаляем завершающий LF на конце страницы
        page_bytes = page_bytes.rstrip(b"\n")

        if not page_bytes:
            raise ValueError(
                "Получилась пустая страница. Измените исходный текст: "
                "страница PGB не может иметь нулевой размер."
            )
        if len(page_bytes) > MAX_PAGE_UNCOMPRESSED:
            raise ValueError(
                f"Внутренняя ошибка: страница имеет {len(page_bytes)} байт, "
                f"максимум {MAX_PAGE_UNCOMPRESSED}."
            )
        if b"\x00" in page_bytes:
            raise ValueError("Внутренняя ошибка: NUL обнаружен в данных страницы.")
        pages.append(page_bytes)

    return pages


def encode_title(title: str) -> bytes:
    normalized = normalize_source(title).replace("\n", " ").strip()
    encoded = bytes(unicode_to_pgo(char) for char in normalized)
    if len(encoded) > UINT16_MAX:
        raise ValueError("Название книги слишком длинное: максимум 65535 байт.")
    if b"\x00" in encoded:
        raise ValueError("Название содержит NUL, который запрещен форматом.")
    return encoded


def crc32_with_zeroed_field(data: bytes | bytearray) -> int:
    mutable = bytearray(data)
    mutable[CRC_OFFSET:CRC_OFFSET + 4] = b"\x00\x00\x00\x00"
    return zlib.crc32(mutable) & UINT32_MAX


def build_pgb(pages: list[bytes], title: bytes) -> bytes:
    require_lz4()

    if not pages:
        raise ValueError("В книге должна быть хотя бы одна страница.")
    if len(pages) > UINT32_MAX:
        raise ValueError("Слишком много страниц для поля page_count uint32.")

    compressed_pages = [
        lz4.block.compress(page, store_size=False)
        for page in pages
    ]

    page_count = len(pages)
    index_offset = HEADER_SIZE
    title_offset = index_offset + page_count * INDEX_ENTRY_SIZE
    data_offset = title_offset + len(title)
    if any(value > UINT32_MAX for value in (title_offset, data_offset)):
        raise ValueError("Размер метаданных превышает диапазон uint32.")

    index_data = bytearray()
    page_offset = data_offset
    for original, compressed in zip(pages, compressed_pages):
        if not compressed:
            raise ValueError("LZ4 вернул пустой блок для непустой страницы.")
        if len(compressed) > UINT32_MAX or len(original) > UINT32_MAX:
            raise ValueError("Размер страницы превышает диапазон uint32.")
        index_data.extend(INDEX_STRUCT.pack(page_offset, len(compressed), len(original)))
        page_offset += len(compressed)
        if page_offset > UINT32_MAX:
            raise ValueError("Размер файла превышает диапазон uint32.")

    file_size = page_offset
    header = HEADER_STRUCT.pack(
        MAGIC,
        VERSION_MAJOR,
        VERSION_MINOR,
        HEADER_SIZE,
        0,
        page_count,
        index_offset,
        data_offset,
        file_size,
        title_offset,
        len(title),
        0,
        0,
        b"\x00" * 24,
    )

    result = bytearray(header)
    result.extend(index_data)
    result.extend(title)
    for block in compressed_pages:
        result.extend(block)

    if len(result) != file_size:
        raise AssertionError(f"Размер собранного файла {len(result)} != {file_size}")

    crc = crc32_with_zeroed_field(result)
    struct.pack_into("<I", result, CRC_OFFSET, crc)
    return bytes(result)


def verify_pgb(data: bytes) -> dict[str, int]:
    require_lz4()

    if len(data) < HEADER_SIZE:
        raise ValueError("Файл короче заголовка PGB.")
    fields = HEADER_STRUCT.unpack_from(data, 0)
    (
        magic, major, minor, header_size, flags, page_count,
        index_offset, data_offset, file_size, title_offset,
        title_length, reserved0, stored_crc, reserved1,
    ) = fields

    if magic != MAGIC:
        raise ValueError("Неверный magic: ожидалось PGB1.")
    if (major, minor) != (VERSION_MAJOR, VERSION_MINOR):
        raise ValueError(f"Неподдерживаемая версия PGB {major}.{minor}.")
    if header_size != HEADER_SIZE:
        raise ValueError(f"Неверный header_size: {header_size}.")
    if flags != 0:
        raise ValueError(f"Неподдерживаемые flags: 0x{flags:04X}.")
    if page_count == 0:
        raise ValueError("page_count равен нулю.")
    if reserved0 != 0 or any(reserved1):
        raise ValueError("Зарезервированные поля заголовка должны быть нулевыми.")
    if file_size != len(data):
        raise ValueError(f"file_size={file_size}, фактический размер={len(data)}.")
    if index_offset != HEADER_SIZE:
        raise ValueError("index_offset должен быть равен 64.")

    expected_title_offset = index_offset + page_count * INDEX_ENTRY_SIZE
    if title_offset != expected_title_offset:
        raise ValueError("title_offset не соответствует концу индекса.")
    if title_offset + title_length != data_offset:
        raise ValueError("data_offset не соответствует концу названия.")
    if title_offset + title_length > file_size:
        raise ValueError("Название выходит за границы файла.")
    if data_offset > file_size:
        raise ValueError("data_offset выходит за границы файла.")

    calculated_crc = crc32_with_zeroed_field(data)
    if calculated_crc != stored_crc:
        raise ValueError(
            f"CRC-32 не совпадает: в файле 0x{stored_crc:08X}, "
            f"рассчитан 0x{calculated_crc:08X}."
        )

    expected_offset = data_offset
    total_uncompressed = 0

    for page_number in range(page_count):
        entry_offset = index_offset + page_number * INDEX_ENTRY_SIZE
        page_offset, compressed_size, uncompressed_size = INDEX_STRUCT.unpack_from(
            data, entry_offset
        )

        if compressed_size == 0 or uncompressed_size == 0:
            raise ValueError(f"Страница {page_number + 1}: нулевой размер.")
        if uncompressed_size > MAX_PAGE_UNCOMPRESSED:
            raise ValueError(
                f"Страница {page_number + 1}: uncompressed_size={uncompressed_size} "
                f"превышает {MAX_PAGE_UNCOMPRESSED}."
            )
        if page_offset != expected_offset:
            raise ValueError(
                f"Страница {page_number + 1}: page_offset={page_offset}, "
                f"ожидалось {expected_offset}; найдены промежуток или перекрытие."
            )
        page_end = page_offset + compressed_size
        if page_end > file_size or page_end < page_offset:
            raise ValueError(f"Страница {page_number + 1}: блок выходит за файл.")

        block = data[page_offset:page_end]
        try:
            decoded = lz4.block.decompress(block, uncompressed_size=uncompressed_size)
        except Exception as exc:
            raise ValueError(
                f"Страница {page_number + 1}: ошибка распаковки LZ4: {exc}"
            ) from exc

        if len(decoded) != uncompressed_size:
            raise ValueError(
                f"Страница {page_number + 1}: после распаковки {len(decoded)} байт, "
                f"ожидалось {uncompressed_size}."
            )
        if b"\x00" in decoded or b"\r" in decoded:
            raise ValueError(
                f"Страница {page_number + 1}: запрещенный NUL или CR в тексте."
            )

        lines = decoded.split(b"\n")
        if len(lines) > MAX_PAGE_LINES:
            raise ValueError(f"Страница {page_number + 1}: больше 32 строк.")
        if any(len(line) > MAX_LINE_CHARS for line in lines):
            raise ValueError(
                f"Страница {page_number + 1}: строка длиннее 29 байт."
            )
        if decoded.endswith(b"\n"):
            raise ValueError(
                f"Страница {page_number + 1}: запрещен завершающий LF."
            )

        expected_offset = page_end
        total_uncompressed += len(decoded)

    if expected_offset != file_size:
        raise ValueError("Конец последнего LZ4-блока не совпадает с file_size.")

    return {
        "pages": page_count,
        "file_size": file_size,
        "uncompressed_size": total_uncompressed,
        "crc32": stored_crc,
    }


def extract_all_pages_text(data: bytes) -> list[str]:
    """Извлекает и декодирует текст всех страниц из PGB-файла."""
    require_lz4()
    fields = HEADER_STRUCT.unpack_from(data, 0)
    _, _, _, _, _, page_count, index_offset, _, _, _, _, _, _, _ = fields

    pages_text = []
    for page_number in range(page_count):
        entry_offset = index_offset + page_number * INDEX_ENTRY_SIZE
        page_offset, compressed_size, uncompressed_size = INDEX_STRUCT.unpack_from(
            data, entry_offset
        )
        block = data[page_offset:page_offset + compressed_size]
        decoded = lz4.block.decompress(block, uncompressed_size=uncompressed_size)
        pages_text.append(pgo_bytes_to_text(decoded))
    return pages_text


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Конвертер plain-text в бинарный формат PurrGO PGB 1.0 и проверка."
    )
    parser.add_argument("input", type=Path, help="Исходный текстовый файл или готовый .pgb файл.")
    parser.add_argument(
        "output",
        type=Path,
        nargs="?",
        default=None,
        help="Путь выходного файла .pgb (если не задан, используется имя входного файла с расширением .pgb).",
    )
    parser.add_argument(
        "--encoding",
        choices=("utf-8", "cp1251", "cp866"),
        default="utf-8",
        help="Кодировка исходного файла (по умолчанию utf-8).",
    )
    parser.add_argument(
        "--title",
        help="Название книги. По умолчанию используется имя исходного файла без расширения.",
    )
    parser.add_argument(
        "--verify",
        action="store_true",
        help="Проверить соответствие формату и вывести первую и последнюю страницы текста в консоль.",
    )
    return parser


def main(argv: Iterable[str] | None = None) -> int:
    args = build_parser().parse_args(argv)

    try:
        # Режим проверки готового .pgb файла (если передан .pgb или указан --verify с .pgb)
        if args.input.suffix.lower() == ".pgb":
            try:
                raw_pgb = args.input.read_bytes()
            except OSError as exc:
                print(f"Ошибка: Не удалось прочитать {args.input}: {exc}", file=sys.stderr)
                return 2

            stats = verify_pgb(raw_pgb)
            print(f"Успешно проверен файл: {args.input}")
            print(f"Страниц: {stats['pages']}")
            print(f"Размер PGB: {stats['file_size']} байт")
            print(f"CRC-32: 0x{stats['crc32']:08X}")
            print("Формат и CRC соответствуют спецификации PGB 1.0: OK")

            # Вывод первой и последней страниц для визуального контроля
            pages_text = extract_all_pages_text(raw_pgb)
            if pages_text:
                print("\n" + "=" * 40)
                print(" ПЕРВАЯ СТРАНИЦА ")
                print("=" * 40)
                print(pages_text[0])
                print("=" * 40)

                if len(pages_text) > 1:
                    print("\n" + "=" * 40)
                    print(f" ПОСЛЕДНЯЯ СТРАНИЦА ({len(pages_text)} из {len(pages_text)}) ")
                    print("=" * 40)
                    print(pages_text[-1])
                    print("=" * 40)
            return 0

        # Режим компиляции из текстового файла
        output_path = args.output if args.output is not None else args.input.with_suffix(".pgb")

        try:
            raw = args.input.read_bytes()
        except OSError as exc:
            raise ValueError(f"Не удалось прочитать {args.input}: {exc}") from exc

        text = decode_source(raw, args.encoding)
        lines = text_to_lines(text)
        pages = paginate(lines)

        book_title = args.title if args.title is not None else args.input.stem
        title_bytes = encode_title(book_title)
        pgb_data = build_pgb(pages, title_bytes)

        stats = verify_pgb(pgb_data)
        try:
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_bytes(pgb_data)
        except OSError as exc:
            raise ValueError(f"Не удалось записать {output_path}: {exc}") from exc

        if args.verify:
            # Если запрошен --verify при компиляции, перепроверим файл с диска и выведем страницы
            written_data = output_path.read_bytes()
            stats = verify_pgb(written_data)
            pages_text = extract_all_pages_text(written_data)
            
            print(f"Создан и проверен файл: {output_path}")
            print(f"Страниц: {stats['pages']}")
            print(f"Строк текста: {len(lines)}")
            print(f"Распакованный текст: {stats['uncompressed_size']} байт")
            print(f"Размер PGB: {stats['file_size']} байт")
            print(f"CRC-32: 0x{stats['crc32']:08X}")
            print("Проверка структуры, CRC и LZ4: OK")

            if pages_text:
                print("\n" + "=" * 40)
                print(" ПЕРВАЯ СТРАНИЦА ")
                print("=" * 40)
                print(pages_text[0])
                print("=" * 40)

                if len(pages_text) > 1:
                    print("\n" + "=" * 40)
                    print(f" ПОСЛЕДНЯЯ СТРАНИЦА ({len(pages_text)} из {len(pages_text)}) ")
                    print("=" * 40)
                    print(pages_text[-1])
                    print("=" * 40)
        else:
            print(f"Создан файл: {output_path}")
            print(f"Страниц: {stats['pages']}")
            print(f"Строк текста: {len(lines)}")
            print(f"Распакованный текст: {stats['uncompressed_size']} байт")
            print(f"Размер PGB: {stats['file_size']} байт")
            print(f"CRC-32: 0x{stats['crc32']:08X}")
            print("Сборка и проверка структуры выполнены успешно.")

        return 0

    except (ValueError, RuntimeError, OSError) as exc:
        print(f"Ошибка: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())