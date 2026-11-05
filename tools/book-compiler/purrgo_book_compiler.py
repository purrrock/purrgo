#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Конвертер книг (txt / FB2 / EPUB) в бинарный формат электронных книг PGB 1.1 для PurrGO.

Запуск без параметров открывает графический интерфейс (tkinter) с предпросмотром страниц;
с параметрами работает в командной строке.

Зависимости: стандартная библиотека Python 3.10+ (для GUI в Linux: python3-tk).
Необязательно:
  - пакет `Pillow`: обложка и иллюстрации (масштаб до окна читалки, 1 bit с дизерингом);
    без него книга собирается без картинок;
  - пакет `lz4`: лучшее сжатие страниц (режим HC); без него работает встроенный кодер LZ4;
  - пакет `pyphen`: расстановка переносов по словарю (--hyphenate).

Примеры:
    python purrgo_book_compiler.py                                # графический интерфейс
    python purrgo_book_compiler.py --gui book.fb2                 # GUI с уже открытой книгой
    python purrgo_book_compiler.py book.txt                       # -> book.pgb
    python purrgo_book_compiler.py book.fb2 BOOK.PGB --verify
    python purrgo_book_compiler.py book.epub --indent 2 --hyphenate ru_RU
    python purrgo_book_compiler.py book.txt --encoding cp1251 --title "Название" --author "Автор"
    python purrgo_book_compiler.py book.txt --compression none    # без сжатия
    python purrgo_book_compiler.py book.epub --dither atkinson    # другой дизеринг картинок
    python purrgo_book_compiler.py book.txt --cover cover.jpg     # своя обложка
    python purrgo_book_compiler.py book.pgb                       # проверка готового файла
"""

from __future__ import annotations

import argparse
import base64
import binascii
import functools
import hashlib
import html.parser
import importlib.util
import io
import posixpath
import re
import struct
import sys
import unicodedata
import urllib.parse
import xml.etree.ElementTree as ET
import zipfile
import zlib
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable, Iterable

# --------------------------------------------------------------------------
# Константы формата PGB 1.1 (см. PGB-format.md)
# --------------------------------------------------------------------------
MAGIC = b"PGB1"
VERSION_MAJOR = 1
VERSION_MINOR = 1
HEADER_SIZE = 64
INDEX_ENTRY_SIZE = 12
DEFAULT_COLS = 29
DEFAULT_ROWS = 24         # строк текста на странице; шаг строк 10 px (LINE_PITCH_PX)
LINE_PITCH_PX = 10
IMG_WINDOW_W = 176        # окно картинки под строкой статуса: 176 x 256 px
IMG_WINDOW_H = 256
MAX_TITLE_BYTES = 200
MAX_AUTHOR_BYTES = 100
COMPRESSION_NONE = 0
COMPRESSION_LZ4 = 1      # сырой LZ4 Block на каждую страницу (не LZ4 Frame)

FILE_CRC_OFFSET = 32
META_CRC_OFFSET = 36

UINT16_MAX = 0xFFFF
UINT32_MAX = 0xFFFFFFFF

# magic, ver_major, ver_minor, header_size, flags,
# page_count, meta_offset, index_offset, data_offset, file_size, file_crc32, meta_crc32,
# cols, rows, compression, reserved0, title_length, author_length, img_w, img_h, reserved1[12]
HEADER_STRUCT = struct.Struct("<4sHHHHIIIIIIIBBBBHHHH12s")
# page_offset, stored_size, raw_size, width, height (width = height = 0 - текстовая страница)
INDEX_STRUCT = struct.Struct("<IHHHH")

assert HEADER_STRUCT.size == HEADER_SIZE
assert INDEX_STRUCT.size == INDEX_ENTRY_SIZE


def max_page_bytes(cols: int, rows: int) -> int:
    """Наибольший размер текстовой страницы: rows строк по cols символов и rows-1 байт LF."""
    return cols * rows + (rows - 1)


def image_row_bytes(width: int) -> int:
    """Байт в строке картинки шириной width пикселей (строки выровнены по байту)."""
    return (width + 7) // 8


def max_image_bytes(width: int, height: int) -> int:
    return image_row_bytes(width) * height


# --------------------------------------------------------------------------
# Таблица PurrGO-256: единый источник - tools/map-compiler/purrgo_models.py
# --------------------------------------------------------------------------
def _load_char_map() -> tuple[dict[str, int], dict[int, str]]:
    models_path = Path(__file__).resolve().parent.parent / "map-compiler" / "purrgo_models.py"
    if not models_path.is_file():
        raise RuntimeError(
            f"Не найден {models_path}: таблица PurrGO-256 берётся из tools/map-compiler/purrgo_models.py."
        )
    spec = importlib.util.spec_from_file_location("purrgo_models_for_books", models_path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)  # type: ignore[union-attr]
    char_map = {ch: code for ch, code in module._PGO_CHAR_MAP.items() if ord(ch) >= 0x80}

    reverse: dict[int, str] = {i: chr(i) for i in range(0x80)}
    for ch, code in char_map.items():
        reverse.setdefault(code, ch)
    return char_map, reverse


PGO_CHAR_MAP, PGO_TO_UNICODE = _load_char_map()

# Замены для символов, которых нет в шрифте. Выполняются до таблицы PurrGO-256,
# результат всегда состоит из символов, которые кодируются одним байтом.
FOLD: dict[str, str] = {
    "\u2026": "...",   # …
    "\u00a9": "(c)",   # ©
    "\u00d7": "x",     # ×
    "\u2022": "*", "\u2023": "*", "\u25cf": "*", "\u25aa": "*",   # маркеры списков
    "\u00b7": ".", "\u2219": ".",
    "\u2032": "'", "\u2033": '"',
    "\u201a": ",", "\u201b": "'",
    "\u2039": "<", "\u203a": ">",
    "\u2044": "/", "\u2215": "/",
    "\u00ad": "",      # мягкий перенос
    "\u2010": "-", "\u2011": "-", "\u2012": "-", "\u2013": "-", "\u2014": "-",
    "\u2015": "-", "\u2212": "-",
}


def _is_mappable(ch: str) -> bool:
    return ord(ch) < 0x80 or ch in PGO_CHAR_MAP


@functools.lru_cache(maxsize=None)
def fold_char(ch: str) -> str:
    """Приводит один символ Unicode к строке из символов, которые есть в PurrGO-256."""
    cp = ord(ch)
    if cp < 0x80:
        return ch
    if ch in PGO_CHAR_MAP:
        return ch
    if ch in FOLD:
        return FOLD[ch]

    category = unicodedata.category(ch)
    if category in ("Cf", "Mn", "Mc", "Me"):
        return ""          # невидимые и комбинирующие знаки (ударения, ZWSP, BOM)
    if category in ("Zs",):
        return " "
    if category in ("Zl", "Zp"):
        return "\n"
    if category == "Cc":
        return " "
    if category in ("Cs", "Co", "Cn"):
        return "?"

    # Совместимое разложение: ﬁ -> fi, ² -> 2, ™ -> TM.
    compat = unicodedata.normalize("NFKC", ch)
    if compat != ch and all(_is_mappable(c) for c in compat):
        return compat
    # Латинские буквы с диакритикой, которых нет в таблице: é -> e, ï -> i.
    base = "".join(c for c in unicodedata.normalize("NFKD", ch) if not unicodedata.combining(c))
    if base and all(_is_mappable(c) for c in base):
        return base
    return "?"


def unicode_to_pgo(char: str) -> int:
    codepoint = ord(char)
    if codepoint < 0x80:
        return codepoint
    return PGO_CHAR_MAP.get(char, ord("?"))


def pgo_bytes_to_text(page_bytes: bytes) -> str:
    return "".join(PGO_TO_UNICODE.get(b, "?") for b in page_bytes)


def normalize_text(text: str) -> str:
    """Нормализация: LF, NFC, табуляция, невидимые знаки, замены символов вне шрифта.

    После вызова каждый символ результата кодируется ровно одним байтом PurrGO-256.
    """
    text = text.replace("\r\n", "\n").replace("\r", "\n").replace("\ufeff", "")
    text = unicodedata.normalize("NFC", text)
    out: list[str] = []
    for ch in text:
        if ch == "\n":
            out.append(ch)
        elif ch == "\t":
            out.append("    ")
        elif ch == "\x00":
            out.append("?")
        else:
            out.append(fold_char(ch))
    return "".join(out)


# --------------------------------------------------------------------------
# Модель документа
# --------------------------------------------------------------------------
@dataclass
class Block:
    """kind: 'p' абзац (с отступом), 'v' строка без отступа, 't' заголовок (по центру), 'b' пустая строка,
    'i' картинка (text - ключ в Document.images)."""
    kind: str
    text: str = ""


@dataclass
class Document:
    title: str = ""
    author: str = ""
    blocks: list[Block] = field(default_factory=list)
    notes: list[str] = field(default_factory=list)  # сообщения о принятых решениях
    images: dict[str, bytes] = field(default_factory=dict)  # ключ -> исходные байты картинки (jpg/png/...)
    cover: str | None = None                                # ключ обложки в images


@dataclass(frozen=True)
class ImageMark:
    """Место картинки среди строк текста (до разбиения на страницы)."""
    key: str


# --------------------------------------------------------------------------
# Картинки: обложка и иллюстрации -> страница-картинка PGB (1 bit, 1 - чёрный)
# --------------------------------------------------------------------------
try:  # необязательно: Pillow нужен только для обложки и иллюстраций
    from PIL import Image as _PilImage
    from PIL import ImageOps as _PilImageOps
except ImportError:  # pragma: no cover - зависит от окружения
    _PilImage = None
    _PilImageOps = None

DITHER_MODES = ("fs", "atkinson", "bayer", "none")
DEFAULT_MIN_IMAGE = 64          # картинки меньше (обе стороны, px) считаются украшениями и пропускаются


def image_support_available() -> bool:
    return _PilImage is not None


class ImageSkipped(ValueError):
    """Картинка осознанно не включена в книгу (например, слишком маленькая)."""


@dataclass(frozen=True)
class ImagePage:
    """Страница-картинка: 1 bit на пиксель, строки выровнены по байту, MSB - левый пиксель, 1 - чёрный."""
    width: int
    height: int
    bits: bytes

    def __post_init__(self) -> None:
        if self.width < 1 or self.height < 1:
            raise ValueError("Размеры картинки должны быть больше нуля.")
        if len(self.bits) != max_image_bytes(self.width, self.height):
            raise ValueError("Размер данных картинки не соответствует её ширине и высоте.")

    def pixel(self, x: int, y: int) -> int:
        """1 - чёрный пиксель, 0 - белый."""
        byte = self.bits[y * image_row_bytes(self.width) + (x >> 3)]
        return (byte >> (7 - (x & 7))) & 1

    def to_png(self) -> bytes:
        """1-битный PNG (для предпросмотра в окне): без Pillow, только zlib."""
        row_bytes = image_row_bytes(self.width)
        raw = bytearray()
        for y in range(self.height):
            raw.append(0)   # фильтр строки: нет
            raw.extend(byte ^ 0xFF for byte in self.bits[y * row_bytes:(y + 1) * row_bytes])   # в PNG 0 - чёрный

        def chunk(tag: bytes, data: bytes) -> bytes:
            return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & UINT32_MAX)

        return (b"\x89PNG\r\n\x1a\n"
                + chunk(b"IHDR", struct.pack(">IIBBBBB", self.width, self.height, 1, 0, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
                + chunk(b"IEND", b""))


def fit_size(width: int, height: int, win_w: int, win_h: int, upscale: bool) -> tuple[int, int]:
    """Размер картинки, вписанной в окно с сохранением пропорций (без увеличения, если upscale=False)."""
    scale = min(win_w / width, win_h / height)
    if not upscale:
        scale = min(scale, 1.0)
    return (max(1, min(win_w, round(width * scale))), max(1, min(win_h, round(height * scale))))


def _dither_threshold(gray: bytes, width: int, height: int) -> bytes:
    return bytes(1 if value < 128 else 0 for value in gray)


def _dither_floyd_steinberg(gray: bytes, width: int, height: int) -> bytes:
    """Floyd-Steinberg со змейкой (строки через одну идут справа налево)."""
    buf = [float(v) for v in gray]
    out = bytearray(width * height)
    for y in range(height):
        step = 1 if y % 2 == 0 else -1
        xs = range(width) if step == 1 else range(width - 1, -1, -1)
        for x in xs:
            i = y * width + x
            old = buf[i]
            new = 255.0 if old >= 128.0 else 0.0
            if new == 0.0:
                out[i] = 1
            err = old - new
            if err == 0.0:
                continue
            if 0 <= x + step < width:
                buf[i + step] += err * 7 / 16
            if y + 1 < height:
                if 0 <= x - step < width:
                    buf[i + width - step] += err * 3 / 16
                buf[i + width] += err * 5 / 16
                if 0 <= x + step < width:
                    buf[i + width + step] += err / 16
    return bytes(out)


def _dither_atkinson(gray: bytes, width: int, height: int) -> bytes:
    """Atkinson: рассеивает 6/8 ошибки, даёт более контрастный результат (светлые и тёмные места чище)."""
    buf = [float(v) for v in gray]
    out = bytearray(width * height)
    for y in range(height):
        for x in range(width):
            i = y * width + x
            old = buf[i]
            new = 255.0 if old >= 128.0 else 0.0
            if new == 0.0:
                out[i] = 1
            err = (old - new) / 8
            if err == 0.0:
                continue
            if x + 1 < width:
                buf[i + 1] += err
            if x + 2 < width:
                buf[i + 2] += err
            if y + 1 < height:
                if x > 0:
                    buf[i + width - 1] += err
                buf[i + width] += err
                if x + 1 < width:
                    buf[i + width + 1] += err
            if y + 2 < height:
                buf[i + 2 * width] += err
    return bytes(out)


def _bayer_matrix(size: int = 8) -> list[list[int]]:
    matrix = [[0]]
    n = 1
    while n < size:
        grown = [[0] * (2 * n) for _ in range(2 * n)]
        for y in range(n):
            for x in range(n):
                value = matrix[y][x] * 4
                grown[y][x] = value
                grown[y][x + n] = value + 2
                grown[y + n][x] = value + 3
                grown[y + n][x + n] = value + 1
        matrix = grown
        n *= 2
    return matrix


_BAYER8 = _bayer_matrix(8)


def _dither_bayer(gray: bytes, width: int, height: int) -> bytes:
    """Упорядоченный дизеринг матрицей Байера 8x8 (регулярная текстура, без «червячков»)."""
    out = bytearray(width * height)
    for y in range(height):
        row = _BAYER8[y % 8]
        for x in range(width):
            if gray[y * width + x] < (row[x % 8] + 0.5) * 255.0 / 64.0:
                out[y * width + x] = 1
    return bytes(out)


_DITHERS: dict[str, Callable[[bytes, int, int], bytes]] = {
    "fs": _dither_floyd_steinberg, "atkinson": _dither_atkinson,
    "bayer": _dither_bayer, "none": _dither_threshold,
}


def pack_mask(mask: bytes, width: int, height: int) -> bytes:
    """Маска 0/1 по одному байту на пиксель (1 - чёрный) -> строки по байту, MSB слева, биты выравнивания 0."""
    row_bytes = image_row_bytes(width)
    out = bytearray(row_bytes * height)
    for y in range(height):
        base = y * row_bytes
        row = mask[y * width:(y + 1) * width]
        for x, value in enumerate(row):
            if value:
                out[base + (x >> 3)] |= 0x80 >> (x & 7)
    return bytes(out)


_IMAGE_CACHE: dict[tuple, ImagePage] = {}


def image_to_page(data: bytes, dither: str = "fs", autocontrast: bool = True, upscale: bool = False,
                  min_size: int = DEFAULT_MIN_IMAGE,
                  win_w: int = IMG_WINDOW_W, win_h: int = IMG_WINDOW_H) -> ImagePage:
    """Исходная картинка -> страница PGB: вписать в окно win_w x win_h, оттенки серого, дизеринг до 1 bit.

    Прозрачность заливается белым. ImageSkipped - картинка меньше min_size по обеим сторонам;
    остальные ошибки (нет Pillow, неподдерживаемый формат, повреждённый файл) - ValueError.
    """
    if _PilImage is None:
        raise ValueError("не установлен пакет Pillow (python -m pip install Pillow)")
    if dither not in _DITHERS:
        raise ValueError(f"неизвестный дизеринг {dither!r} (допустимо: {', '.join(DITHER_MODES)})")
    cache_key = (hashlib.sha1(data).digest(), dither, autocontrast, upscale, min_size, win_w, win_h)
    cached = _IMAGE_CACHE.get(cache_key)
    if cached is not None:
        return cached

    try:
        with _PilImage.open(io.BytesIO(data)) as source:
            source.load()
            img = _PilImageOps.exif_transpose(source)
            if img.mode in ("RGBA", "LA") or (img.mode == "P" and "transparency" in img.info):
                rgba = img.convert("RGBA")
                background = _PilImage.new("RGBA", rgba.size, (255, 255, 255, 255))
                background.alpha_composite(rgba)
                img = background
            gray = img.convert("L")
    except Exception as exc:  # noqa: BLE001 - Pillow бросает разные исключения для разных форматов
        raise ValueError(f"не удалось прочитать изображение ({exc.__class__.__name__}: {exc})") from exc

    width, height = gray.size
    if max(width, height) < min_size:
        raise ImageSkipped(f"слишком маленькая ({width}x{height})")
    new_w, new_h = fit_size(width, height, win_w, win_h, upscale)
    if (new_w, new_h) != (width, height):
        resample = getattr(_PilImage, "Resampling", _PilImage).LANCZOS
        gray = gray.resize((new_w, new_h), resample)
    if autocontrast:
        gray = _PilImageOps.autocontrast(gray, cutoff=1)

    mask = _DITHERS[dither](gray.tobytes(), new_w, new_h)
    page = ImagePage(new_w, new_h, pack_mask(mask, new_w, new_h))
    if len(_IMAGE_CACHE) >= 512:
        _IMAGE_CACHE.clear()
    _IMAGE_CACHE[cache_key] = page
    return page


# --------------------------------------------------------------------------
# Чтение plain text
# --------------------------------------------------------------------------
def decode_source(raw: bytes, encoding: str) -> tuple[str, str]:
    """Возвращает (текст, фактически использованная кодировка)."""
    if raw.startswith(b"\xff\xfe") or raw.startswith(b"\xfe\xff"):
        return raw.decode("utf-16"), "utf-16"
    if encoding == "auto":
        try:
            return raw.decode("utf-8-sig"), "utf-8"
        except UnicodeDecodeError:
            try:
                return raw.decode("cp1251"), "cp1251 (определено автоматически)"
            except UnicodeDecodeError as exc:
                raise ValueError(
                    "Не удалось определить кодировку: файл не UTF-8 и не CP1251. Укажите --encoding."
                ) from exc
    actual = "utf-8-sig" if encoding == "utf-8" else encoding
    try:
        return raw.decode(actual, errors="strict"), encoding
    except UnicodeDecodeError as exc:
        raise ValueError(
            f"Файл не декодируется как {encoding}: ошибка около байта {exc.start}. Проверьте --encoding."
        ) from exc


def looks_hard_wrapped(lines: list[str]) -> bool:
    """Эвристика: текст набран строками фиксированной ширины (как в книгах Project Gutenberg)."""
    lens = sorted(len(line.rstrip()) for line in lines if line.strip())
    n = len(lens)
    if n < 20:
        return False
    p95 = lens[int(0.95 * (n - 1))]
    median = lens[n // 2]
    if not (55 <= p95 <= 120) or median < 35:
        return False
    near_max = sum(1 for v in lens if v >= 0.85 * p95) / n
    return near_max >= 0.45


def text_to_blocks(text: str, reflow: str) -> tuple[list[Block], str]:
    lines = normalize_text(text).split("\n")
    if lines and lines[-1] == "":
        lines.pop()

    if reflow == "auto":
        do_reflow = looks_hard_wrapped(lines)
        note = "жёсткие переносы строк в исходнике обнаружены, строки склеиваются в абзацы" if do_reflow \
            else "абзац = одна строка исходника"
    else:
        do_reflow = reflow == "on"
        note = "склейка строк в абзацы включена" if do_reflow else "абзац = одна строка исходника"

    blocks: list[Block] = []
    if not do_reflow:
        for line in lines:
            blocks.append(Block("p", line) if line.strip() else Block("b"))
        return blocks, note

    paragraph: list[str] = []

    def flush() -> None:
        if paragraph:
            blocks.append(Block("p", " ".join(paragraph)))
            paragraph.clear()

    for line in lines:
        if not line.strip():
            flush()
            blocks.append(Block("b"))
            continue
        indented = len(line) - len(line.lstrip(" ")) >= 2
        if indented:
            flush()
        paragraph.append(line.strip())
    flush()
    return blocks, note


# --------------------------------------------------------------------------
# Чтение FB2
# --------------------------------------------------------------------------
def _local(tag: object) -> str:
    return tag.rsplit("}", 1)[-1] if isinstance(tag, str) else ""


def _inline_text(element: ET.Element) -> str:
    return " ".join("".join(element.itertext()).split())


def _decode_base64(text: str) -> bytes:
    """Base64 из FB2 (бывает с пропущенным выравниванием); пустой результат - данные непригодны."""
    for padded in (text, text + "=" * (-len(text) % 4)):
        try:
            return base64.b64decode(padded)
        except (binascii.Error, ValueError):
            continue
    return b""


def _drop_duplicate_cover(doc: Document) -> None:
    """Обложка из метаданных уже станет первой страницей: такая же картинка в начале текста лишняя."""
    if doc.cover is None:
        return
    for index, block in enumerate(doc.blocks):
        if block.kind == "i" and block.text == doc.cover:
            del doc.blocks[index]
            return
        if block.kind not in ("b", "i") and block.text.strip():
            return


def fb2_to_document(raw: bytes) -> Document:
    try:
        root = ET.fromstring(raw)
    except ET.ParseError as exc:
        raise ValueError(f"Некорректный FB2 (XML): {exc}") from exc

    doc = Document()
    for binary in root:
        if _local(binary.tag) == "binary" and binary.get("id"):
            data = _decode_base64("".join((binary.text or "").split()))
            if data:
                doc.images[binary.get("id")] = data

    def image_key(element: ET.Element) -> str | None:
        for name, value in element.attrib.items():
            if _local(name) == "href" and value.startswith("#") and value[1:] in doc.images:
                return value[1:]
        return None

    for info in root.iter():
        if _local(info.tag) == "title-info":
            for child in info:
                name = _local(child.tag)
                if name == "coverpage":
                    for image in child.iter():
                        key = image_key(image) if _local(image.tag) == "image" else None
                        if key:
                            doc.cover = key
                            break
                elif name == "book-title" and not doc.title:
                    doc.title = _inline_text(child)
                elif name == "author" and not doc.author:
                    parts = {_local(c.tag): _inline_text(c) for c in child}
                    full = " ".join(
                        parts[key] for key in ("first-name", "middle-name", "last-name") if parts.get(key)
                    )
                    doc.author = full or parts.get("nickname", "")
            break

    skipped_notes = False

    def walk(el: ET.Element) -> None:
        tag = _local(el.tag)
        if tag == "p":
            doc.blocks.append(Block("p", _inline_text(el)))
            for inline in el.iter():   # картинка внутри абзаца идёт после его текста
                if _local(inline.tag) == "image" and image_key(inline):
                    doc.blocks.append(Block("i", image_key(inline)))
        elif tag == "image":
            if image_key(el):
                doc.blocks.append(Block("i", image_key(el)))
        elif tag in ("subtitle",):
            doc.blocks.append(Block("t", _inline_text(el)))
        elif tag == "title":
            for child in el:
                if _local(child.tag) == "p":
                    doc.blocks.append(Block("t", _inline_text(child)))
        elif tag == "empty-line":
            doc.blocks.append(Block("b"))
        elif tag == "text-author":
            doc.blocks.append(Block("v", _inline_text(el)))
        elif tag == "v":
            doc.blocks.append(Block("v", _inline_text(el)))
        elif tag == "stanza":
            for child in el:
                walk(child)
            doc.blocks.append(Block("b"))
        elif tag == "table":
            for row in el.iter():
                if _local(row.tag) == "tr":
                    cells = [_inline_text(c) for c in row if _local(c.tag) in ("td", "th")]
                    doc.blocks.append(Block("v", " ".join(c for c in cells if c)))
        elif tag in ("binary", "description"):
            return
        else:
            for child in el:
                walk(child)

    for body in root:
        if _local(body.tag) != "body":
            continue
        if body.get("name") in ("notes", "comments"):
            skipped_notes = True
            continue
        walk(body)

    if skipped_notes:
        doc.notes.append("FB2: сноски (body name=\"notes\") не включены в книгу")
    _drop_duplicate_cover(doc)
    return doc


# --------------------------------------------------------------------------
# Чтение EPUB
# --------------------------------------------------------------------------
class _HtmlBlocks(html.parser.HTMLParser):
    BLOCK_TAGS = {
        "p", "div", "li", "ul", "ol", "blockquote", "pre", "tr", "table", "section", "article",
        "aside", "dd", "dt", "dl", "figure", "figcaption", "caption", "header", "footer",
        "body", "hr", "nav", "main",
    }
    HEAD_TAGS = {"h1", "h2", "h3", "h4", "h5", "h6"}
    SKIP_TAGS = {"script", "style", "head", "title", "svg", "math"}

    def __init__(self, resolve: Callable[[str], str | None] | None = None) -> None:
        super().__init__(convert_charrefs=True)
        self._resolve = resolve    # адрес картинки из разметки -> ключ (путь в архиве) или None
        self.blocks: list[Block] = []
        self._buf: list[str] = []
        self._skip = 0
        self._in_head = False
        self._cont = False

    def _flush(self) -> None:
        text = " ".join("".join(self._buf).split())
        self._buf.clear()
        if not text:
            return
        if self._in_head:
            kind = "t"
        else:
            kind = "v" if self._cont else "p"
            self._cont = True
        self.blocks.append(Block(kind, text))

    def handle_starttag(self, tag: str, attrs: list) -> None:
        if tag in ("img", "image") and self._resolve is not None:   # <img src> и <image xlink:href> (обложка в SVG)
            values = dict(attrs)
            src = values.get("src") or values.get("xlink:href") or values.get("href")
            key = self._resolve(src) if src else None
            if key:
                self._flush()
                self.blocks.append(Block("i", key))
                self._cont = False
            return
        if tag in self.SKIP_TAGS:
            self._flush()
            self._skip += 1
        elif tag in self.HEAD_TAGS:
            self._flush()
            self._in_head = True
        elif tag == "br":
            self._flush()
        elif tag in self.BLOCK_TAGS:
            self._flush()
            self._cont = False

    def handle_endtag(self, tag: str) -> None:
        if tag in self.SKIP_TAGS:
            self._skip = max(0, self._skip - 1)
        elif tag in self.HEAD_TAGS:
            self._flush()
            self._in_head = False
            self._cont = False
        elif tag in self.BLOCK_TAGS:
            self._flush()
            self._cont = False

    def handle_data(self, data: str) -> None:
        if not self._skip:
            self._buf.append(data)

    def close(self) -> None:  # noqa: D401
        super().close()
        self._flush()


def _decode_markup(raw: bytes) -> str:
    if raw.startswith(b"\xef\xbb\xbf"):
        return raw.decode("utf-8-sig", errors="replace")
    head = raw[:1024]
    match = re.search(rb"""(?:encoding|charset)\s*=\s*["']?\s*([A-Za-z0-9_\-]+)""", head)
    encoding = match.group(1).decode("ascii") if match else "utf-8"
    try:
        return raw.decode(encoding, errors="replace")
    except LookupError:
        return raw.decode("utf-8", errors="replace")


def epub_to_document(source: Path) -> Document:
    try:
        archive = zipfile.ZipFile(source)
    except (zipfile.BadZipFile, OSError) as exc:
        raise ValueError(f"Не удалось открыть EPUB как ZIP: {exc}") from exc

    with archive:
        names = set(archive.namelist())
        try:
            container = ET.fromstring(archive.read("META-INF/container.xml"))
        except (KeyError, ET.ParseError) as exc:
            raise ValueError("В EPUB нет корректного META-INF/container.xml.") from exc
        opf_path = next(
            (el.get("full-path") for el in container.iter() if _local(el.tag) == "rootfile" and el.get("full-path")),
            None,
        )
        if not opf_path or opf_path not in names:
            raise ValueError("В EPUB не найден файл пакета (OPF).")
        try:
            opf = ET.fromstring(archive.read(opf_path))
        except ET.ParseError as exc:
            raise ValueError(f"Некорректный OPF: {exc}") from exc

        doc = Document()
        manifest: dict[str, tuple[str, str, str]] = {}
        spine: list[tuple[str, bool]] = []
        cover_ids: list[str] = []
        for el in opf.iter():
            name = _local(el.tag)
            if name == "meta" and el.get("name") == "cover" and el.get("content"):
                cover_ids.append(el.get("content"))
            elif name == "title" and not doc.title and (el.text or "").strip():
                doc.title = " ".join(el.text.split())
            elif name == "creator" and not doc.author and (el.text or "").strip():
                doc.author = " ".join(el.text.split())
            elif name == "item" and el.get("id"):
                manifest[el.get("id")] = (
                    el.get("href", ""), el.get("media-type", ""), el.get("properties", ""),
                )
                if "cover-image" in el.get("properties", "").split():
                    cover_ids.insert(0, el.get("id"))
            elif name == "itemref" and el.get("idref"):
                spine.append((el.get("idref"), el.get("linear", "yes") != "no"))

        base = posixpath.dirname(opf_path)
        for cover_id in cover_ids:   # обложка: свойство cover-image или <meta name="cover">
            if cover_id in manifest:
                href, media, _props = manifest[cover_id]
                member = posixpath.normpath(posixpath.join(base, urllib.parse.unquote(href.split("#")[0])))
                if member in names and media.startswith("image/"):
                    doc.images[member] = archive.read(member)
                    doc.cover = member
                    break
        missing = 0
        for idref, linear in spine:
            if not linear or idref not in manifest:
                continue
            href, media, props = manifest[idref]
            if media not in ("application/xhtml+xml", "text/html") or "nav" in props.split():
                continue
            member = posixpath.normpath(posixpath.join(base, urllib.parse.unquote(href.split("#")[0])))
            if member not in names:
                missing += 1
                continue
            doc_dir = posixpath.dirname(member)

            def resolve(src: str, _dir: str = doc_dir) -> str | None:
                if src.startswith("data:") or "://" in src:
                    return None
                target = posixpath.normpath(
                    posixpath.join(_dir, urllib.parse.unquote(src.split("#")[0].split("?")[0])))
                return target if target in names else None

            parser = _HtmlBlocks(resolve)
            parser.feed(_decode_markup(archive.read(member)))
            parser.close()
            for block in parser.blocks:
                if block.kind == "i" and block.text not in doc.images:
                    doc.images[block.text] = archive.read(block.text)
            doc.blocks.extend(parser.blocks)

        if missing:
            doc.notes.append(f"EPUB: {missing} файл(ов) из spine не найдено в архиве и пропущено")
        if not any(block.kind != "i" for block in doc.blocks):
            raise ValueError("В EPUB не найден текст (возможно, книга защищена DRM).")
        _drop_duplicate_cover(doc)
        return doc


def read_document(path: Path, encoding: str, reflow: str) -> Document:
    name = path.name.lower()
    try:
        if name.endswith(".epub"):
            return epub_to_document(path)
        if name.endswith(".fb2"):
            return fb2_to_document(path.read_bytes())
        if name.endswith(".zip"):
            with zipfile.ZipFile(path) as archive:
                members = [n for n in archive.namelist() if n.lower().endswith(".fb2")]
                if not members:
                    raise ValueError("В ZIP-архиве нет файла .fb2.")
                return fb2_to_document(archive.read(members[0]))
        raw = path.read_bytes()
    except (OSError, zipfile.BadZipFile) as exc:
        raise ValueError(f"Не удалось прочитать {path}: {exc}") from exc

    text, used_encoding = decode_source(raw, encoding)
    blocks, note = text_to_blocks(text, reflow)
    doc = Document(blocks=blocks)
    doc.notes.append(f"Кодировка: {used_encoding}")
    doc.notes.append(f"Абзацы: {note}")
    return doc


# --------------------------------------------------------------------------
# Перенос слов и разбиение на страницы
# --------------------------------------------------------------------------
_HARD_HYPHEN_RE = re.compile(r"(?<=\w-)(?=\w)")
_WORD_CORE_RE = re.compile(r"([^\w]*)([^\W\d_]{5,})([^\w]*)")

Piece = tuple[str, str]  # (текст, разделитель после: " ", "" или "soft")


def make_hyphenator(lang: str | None) -> Callable[[str], list[str]] | None:
    if not lang:
        return None
    try:
        import pyphen
    except ImportError as exc:
        raise RuntimeError(
            "Для --hyphenate нужен пакет pyphen: python -m pip install pyphen"
        ) from exc
    if lang not in pyphen.LANGUAGES:
        raise ValueError(f"pyphen не знает язык {lang!r}. Пример: ru_RU, en_US, uk_UA.")
    dic = pyphen.Pyphen(lang=lang)

    def split(word: str) -> list[str]:
        cuts = dic.positions(word)
        parts, prev = [], 0
        for cut in cuts:
            parts.append(word[prev:cut])
            prev = cut
        parts.append(word[prev:])
        return parts

    return split


def word_pieces(word: str, hyphenator: Callable[[str], list[str]] | None) -> list[Piece]:
    parts = _HARD_HYPHEN_RE.split(word)
    pieces: list[Piece] = []
    for index, part in enumerate(parts):
        end_sep = " " if index == len(parts) - 1 else ""
        match = _WORD_CORE_RE.fullmatch(part) if hyphenator else None
        syllables = hyphenator(match.group(2)) if match else []
        if match and len(syllables) > 1:
            syllables[0] = match.group(1) + syllables[0]
            syllables[-1] = syllables[-1] + match.group(3)
            pieces.extend((s, "soft") for s in syllables[:-1])
            pieces.append((syllables[-1], end_sep))
        else:
            pieces.append((part, end_sep))
    return pieces


def wrap_pieces(pieces: Iterable[Piece], width: int, indent: int = 0) -> list[str]:
    out: list[str] = []
    cur = " " * indent
    pending = ""
    empty = True
    for text, sep in pieces:
        gap = " " if (pending == " " and not empty) else ""
        reserve = 1 if sep == "soft" else 0
        if len(cur) + len(gap) + len(text) + reserve <= width:
            cur += gap + text
            pending = sep
            empty = False
            continue
        if not empty:
            if pending == "soft":
                cur += "-"
            out.append(cur)
        cur = ""
        while len(text) + reserve > width and len(text) > 1:
            out.append(text[:width])
            text = text[width:]
        cur = text
        pending = sep
        empty = False
    if not empty:
        out.append(cur)
    return out


def wrap_text(text: str, width: int, indent: int = 0,
              hyphenator: Callable[[str], list[str]] | None = None) -> list[str]:
    pieces: list[Piece] = []
    for word in text.split():
        pieces.extend(word_pieces(word, hyphenator))
    return wrap_pieces(pieces, width, indent)


def blocks_to_lines(blocks: list[Block], width: int = DEFAULT_COLS, indent: int = 0,
                    collapse_blank: bool = False,
                    blank_between_paragraphs: bool = False,
                    hyphenator: Callable[[str], list[str]] | None = None) -> list[str | ImageMark]:
    """Блоки -> строки текста; картинка остаётся в потоке как ImageMark (paginate выделит ей страницу)."""
    if not 0 <= indent < width:
        raise ValueError(f"--indent должен быть от 0 до {width - 1}.")
    lines: list[str | ImageMark] = []
    blank_pending = False

    def add_blank(force: bool = False) -> None:
        if lines and (force or lines[-1] != ""):
            lines.append("")

    for block in blocks:
        if block.kind == "b":
            if not collapse_blank or (lines and lines[-1] != ""):
                lines.append("")
            blank_pending = False
            continue
        if block.kind == "i":
            lines.append(ImageMark(block.text))
            blank_pending = False
            continue
        text = " ".join(normalize_text(block.text).split())
        if not text:
            continue
        if blank_pending:
            add_blank()
            blank_pending = False
        if block.kind == "t":
            add_blank()
            for line in wrap_text(text, width):
                lines.append(" " * ((width - len(line)) // 2) + line)
            blank_pending = True
        elif block.kind == "v":
            lines.extend(wrap_text(text, width, 0, hyphenator))
        else:
            lines.extend(wrap_text(text, width, indent, hyphenator))
            if blank_between_paragraphs:
                blank_pending = True

    while lines and lines[0] == "":
        lines.pop(0)
    while lines and lines[-1] == "":
        lines.pop()
    if not lines:
        raise ValueError("Исходный текст пуст: PGB не допускает пустую страницу.")
    return lines


def _paginate_text(lines: list[str], rows: int, cols: int) -> list[bytes]:
    """Страницы максимум по `rows` строк в PurrGO-256; пустые строки в начале и конце страницы отбрасываются."""
    pages: list[bytes] = []
    limit = max_page_bytes(cols, rows)
    i = 0
    while i < len(lines):
        while i < len(lines) and lines[i] == "":
            i += 1
        if i >= len(lines):
            break
        chunk = lines[i:i + rows]
        i += rows
        while chunk and chunk[-1] == "":
            chunk.pop()
        page = bytes(unicode_to_pgo(ch) for ch in "\n".join(chunk))
        if len(page) > limit or any(len(line) > cols for line in chunk):
            raise ValueError("Внутренняя ошибка: страница не укладывается в геометрию.")
        if b"\x00" in page:
            raise ValueError("Внутренняя ошибка: NUL обнаружен в данных страницы.")
        pages.append(page)
    return pages


def paginate(lines: list[str | ImageMark], rows: int = DEFAULT_ROWS,
             cols: int = DEFAULT_COLS) -> list[bytes | ImageMark]:
    """Текстовые страницы (bytes) и места картинок (ImageMark). Картинка всегда занимает страницу целиком:
    текст до неё заканчивает страницу, после неё начинается новая."""
    pages: list[bytes | ImageMark] = []
    run: list[str] = []
    for item in lines:
        if isinstance(item, ImageMark):
            pages.extend(_paginate_text(run, rows, cols))
            run = []
            pages.append(item)
        else:
            run.append(item)
    pages.extend(_paginate_text(run, rows, cols))
    if not pages:
        raise ValueError("Исходный текст пуст: PGB не допускает пустую страницу.")
    return pages


def encode_meta(value: str, limit: int, what: str, notes: list[str]) -> bytes:
    text = " ".join(normalize_text(value).replace("\n", " ").split())
    if len(text) > limit:
        notes.append(f"{what} обрезано до {limit} символов")
        text = text[:limit].rstrip()
    return bytes(unicode_to_pgo(ch) for ch in text)


# --------------------------------------------------------------------------
# LZ4 Block (сырой блок без заголовка кадра) - сжатие отдельных страниц
# --------------------------------------------------------------------------
LZ4_MINMATCH = 4
LZ4_LASTLITERALS = 5      # последние 5 байт блока всегда литералы
LZ4_MFLIMIT = 12          # совпадение не может начинаться в последних 12 байтах блока
LZ4_MAX_OFFSET = 65535
_LZ4_CHAIN_DEPTH = 48

try:  # необязательно: пакет lz4 даёт лучшее сжатие (режим HC), без него работает встроенный кодер
    import lz4.block as _lz4_block
except ImportError:  # pragma: no cover - зависит от окружения
    _lz4_block = None


def lz4_library_available() -> bool:
    return _lz4_block is not None


def _lz4_put_length(out: bytearray, value: int) -> None:
    while value >= 255:
        out.append(255)
        value -= 255
    out.append(value)


def lz4_compress_block_py(data: bytes) -> bytes:
    """Встроенный кодер LZ4 Block (цепочки хешей, жадный выбор с отложенным шагом).

    Результат - обычный сырой LZ4 Block, его распаковывает любой декодер. Ратио чуть
    хуже режима HC пакета lz4, но зависимостей нет и результат детерминирован.
    """
    size = len(data)
    out = bytearray()
    anchor = 0

    if size > LZ4_MFLIMIT:
        match_start_limit = size - LZ4_MFLIMIT   # последняя допустимая позиция начала совпадения
        match_end_limit = size - LZ4_LASTLITERALS
        chains: dict[bytes, list[int]] = {}

        def insert(pos: int) -> None:
            chains.setdefault(data[pos:pos + 4], []).append(pos)

        def find(pos: int) -> tuple[int, int]:
            candidates = chains.get(data[pos:pos + 4])
            best_len = 0
            best_off = 0
            if not candidates:
                return 0, 0
            max_len = match_end_limit - pos
            for cand in reversed(candidates[-_LZ4_CHAIN_DEPTH:]):
                offset = pos - cand
                if offset > LZ4_MAX_OFFSET:
                    break
                length = 4
                while length < max_len and data[cand + length] == data[pos + length]:
                    length += 1
                if length > best_len:
                    best_len, best_off = length, offset
            return best_len, best_off

        pos = 0
        while pos <= match_start_limit:
            length, offset = find(pos)
            if length >= LZ4_MINMATCH:
                if pos + 1 <= match_start_limit:
                    next_len, _ = find(pos + 1)
                    if next_len > length + 1:
                        insert(pos)
                        pos += 1
                        continue
                literals = pos - anchor
                token_lit = min(literals, 15)
                token_match = min(length - LZ4_MINMATCH, 15)
                out.append((token_lit << 4) | token_match)
                if literals >= 15:
                    _lz4_put_length(out, literals - 15)
                out += data[anchor:pos]
                out += offset.to_bytes(2, "little")
                if length - LZ4_MINMATCH >= 15:
                    _lz4_put_length(out, length - LZ4_MINMATCH - 15)
                for covered in range(pos, min(pos + length, match_start_limit + 1)):
                    insert(covered)
                pos += length
                anchor = pos
            else:
                insert(pos)
                pos += 1

    literals = size - anchor
    out.append(min(literals, 15) << 4)
    if literals >= 15:
        _lz4_put_length(out, literals - 15)
    out += data[anchor:]
    return bytes(out)


def lz4_compress_block(data: bytes) -> bytes:
    """Сжимает блок: пакет lz4 (HC, уровень 12), если установлен, иначе встроенный кодер."""
    if _lz4_block is not None:
        return _lz4_block.compress(data, mode="high_compression", compression=12, store_size=False)
    return lz4_compress_block_py(data)


def lz4_decompress_block(src: bytes, raw_size: int) -> bytes:
    """Строгий декодер LZ4 Block: на выходе ровно raw_size байт, весь src израсходован.

    Независим от пакета lz4 и ведёт себя так же, как декодер прошивки (pgb_lz4_decode).
    """
    out = bytearray()
    pos = 0
    size = len(src)
    while True:
        if pos >= size:
            raise ValueError("LZ4: данные оборваны (нет токена)")
        token = src[pos]
        pos += 1
        literals = token >> 4
        if literals == 15:
            while True:
                if pos >= size:
                    raise ValueError("LZ4: обрыв в длине литералов")
                extra = src[pos]
                pos += 1
                literals += extra
                if literals > raw_size:
                    raise ValueError("LZ4: литералов больше, чем размер страницы")
                if extra != 255:
                    break
        if literals > raw_size - len(out) or literals > size - pos:
            raise ValueError("LZ4: литералы выходят за границы")
        out += src[pos:pos + literals]
        pos += literals
        if pos == size:
            break                      # последняя последовательность - только литералы
        if size - pos < 2:
            raise ValueError("LZ4: обрыв в смещении")
        offset = src[pos] | (src[pos + 1] << 8)
        pos += 2
        if offset == 0 or offset > len(out):
            raise ValueError("LZ4: недопустимое смещение совпадения")
        match = token & 15
        if match == 15:
            while True:
                if pos >= size:
                    raise ValueError("LZ4: обрыв в длине совпадения")
                extra = src[pos]
                pos += 1
                match += extra
                if match > raw_size:
                    raise ValueError("LZ4: совпадение длиннее страницы")
                if extra != 255:
                    break
        match += LZ4_MINMATCH
        if match > raw_size - len(out):
            raise ValueError("LZ4: совпадение выходит за размер страницы")
        if offset >= match:
            start = len(out) - offset
            out += out[start:start + match]
        else:
            for _ in range(match):
                out.append(out[-offset])
    if len(out) != raw_size:
        raise ValueError(f"LZ4: распаковано {len(out)} байт вместо {raw_size}")
    return bytes(out)


def compress_page(page: bytes, compression: int) -> bytes:
    """Данные страницы для записи в файл. Если сжатие не уменьшает размер, страница хранится как есть."""
    if compression == COMPRESSION_LZ4:
        packed = lz4_compress_block(page)
        if len(packed) < len(page):
            return packed
    return page


def decompress_page(stored: bytes, raw_size: int, compression: int) -> bytes:
    if len(stored) == raw_size:
        return stored                       # страница хранится без сжатия
    if compression != COMPRESSION_LZ4 or len(stored) > raw_size:
        raise ValueError("размер данных страницы не соответствует способу сжатия")
    return lz4_decompress_block(stored, raw_size)


COMPRESSION_NAMES = {"none": COMPRESSION_NONE, "lz4": COMPRESSION_LZ4}


# --------------------------------------------------------------------------
# Сборка и проверка PGB
# --------------------------------------------------------------------------
def crc32_zeroed(data: bytes | bytearray, end: int | None = None) -> int:
    """CRC-32/ISO-HDLC по data[0:end], где поля file_crc32 и meta_crc32 считаются нулевыми."""
    view = memoryview(data)
    end = len(data) if end is None else end
    crc = zlib.crc32(view[:FILE_CRC_OFFSET])
    crc = zlib.crc32(b"\x00" * 8, crc)
    crc = zlib.crc32(view[META_CRC_OFFSET + 4:end], crc)
    return crc & UINT32_MAX


def _page_parts(page: bytes | ImagePage) -> tuple[bytes, int, int]:
    """(данные страницы до сжатия, ширина, высота): для текстовой страницы ширина и высота равны нулю."""
    if isinstance(page, ImagePage):
        return page.bits, page.width, page.height
    return page, 0, 0


def build_pgb(pages: list[bytes | ImagePage], title: bytes, author: bytes,
              cols: int = DEFAULT_COLS, rows: int = DEFAULT_ROWS,
              compression: int = COMPRESSION_LZ4,
              img_w: int = IMG_WINDOW_W, img_h: int = IMG_WINDOW_H) -> bytes:
    if not pages:
        raise ValueError("В книге должна быть хотя бы одна страница.")
    if not (1 <= cols <= 255 and 1 <= rows <= 255):
        raise ValueError("cols и rows должны быть в диапазоне 1..255.")
    if not (1 <= img_w <= UINT16_MAX and 1 <= img_h <= UINT16_MAX) or max_image_bytes(img_w, img_h) > UINT16_MAX:
        raise ValueError("Окно картинок должно быть больше нуля, а картинка окна - не больше 65535 байт.")
    if compression not in (COMPRESSION_NONE, COMPRESSION_LZ4):
        raise ValueError(f"Неизвестный способ сжатия: {compression}.")
    if len(title) > MAX_TITLE_BYTES or len(author) > MAX_AUTHOR_BYTES:
        raise ValueError("Название или автор слишком длинные.")

    parts = [_page_parts(page) for page in pages]
    for number, (raw, width, height) in enumerate(parts, 1):
        if width > img_w or height > img_h:
            raise ValueError(f"Страница {number}: картинка {width}x{height} не помещается в окно {img_w}x{img_h}.")
        if len(raw) > UINT16_MAX:
            raise ValueError(f"Страница {number}: размер {len(raw)} превышает 65535 байт.")

    stored_pages = [compress_page(raw, compression) for raw, _w, _h in parts]
    page_count = len(pages)
    meta_offset = HEADER_SIZE
    index_offset = meta_offset + len(title) + len(author)
    data_offset = index_offset + page_count * INDEX_ENTRY_SIZE

    index = bytearray()
    offset = data_offset
    for (raw, width, height), stored in zip(parts, stored_pages):
        index += INDEX_STRUCT.pack(offset, len(stored), len(raw), width, height)
        offset += len(stored)
        if offset > UINT32_MAX:
            raise ValueError("Размер файла превышает диапазон uint32.")
    file_size = offset

    header = HEADER_STRUCT.pack(
        MAGIC, VERSION_MAJOR, VERSION_MINOR, HEADER_SIZE, 0,
        page_count, meta_offset, index_offset, data_offset, file_size, 0, 0,
        cols, rows, compression, 0, len(title), len(author), img_w, img_h, b"\x00" * 12,
    )
    result = bytearray(header)
    result += title
    result += author
    result += index
    for stored in stored_pages:
        result += stored
    if len(result) != file_size:
        raise AssertionError(f"Размер собранного файла {len(result)} != {file_size}")

    struct.pack_into("<I", result, META_CRC_OFFSET, crc32_zeroed(result, data_offset))
    struct.pack_into("<I", result, FILE_CRC_OFFSET, crc32_zeroed(result))
    return bytes(result)


def _iter_pages(data: bytes, page_count: int, index_offset: int):
    for number in range(page_count):
        yield number, INDEX_STRUCT.unpack_from(data, index_offset + number * INDEX_ENTRY_SIZE)


def verify_pgb(data: bytes) -> dict:
    if len(data) < HEADER_SIZE:
        raise ValueError("Файл короче заголовка PGB.")
    (
        magic, major, minor, header_size, flags, page_count, meta_offset, index_offset,
        data_offset, file_size, file_crc, meta_crc, cols, rows, compression, reserved0,
        title_len, author_len, img_w, img_h, reserved1,
    ) = HEADER_STRUCT.unpack_from(data, 0)

    if magic != MAGIC:
        raise ValueError("Неверный magic: ожидалось PGB1.")
    if (major, minor) != (VERSION_MAJOR, VERSION_MINOR):
        raise ValueError(f"Неподдерживаемая версия PGB {major}.{minor} (нужна {VERSION_MAJOR}.{VERSION_MINOR}).")
    if header_size != HEADER_SIZE:
        raise ValueError(f"Неверный header_size: {header_size}.")
    if flags != 0:
        raise ValueError(f"Неподдерживаемые flags: 0x{flags:04X}.")
    if compression not in (COMPRESSION_NONE, COMPRESSION_LZ4):
        raise ValueError(f"Неподдерживаемый способ сжатия: {compression}.")
    if reserved0 != 0 or any(reserved1):
        raise ValueError("Зарезервированные поля заголовка должны быть нулевыми.")
    if page_count == 0:
        raise ValueError("page_count равен нулю.")
    if cols == 0 or rows == 0:
        raise ValueError("cols и rows должны быть больше нуля.")
    if img_w == 0 or img_h == 0 or max_image_bytes(img_w, img_h) > UINT16_MAX:
        raise ValueError(f"Окно картинок {img_w}x{img_h} недопустимо.")
    if file_size != len(data):
        raise ValueError(f"file_size={file_size}, фактический размер={len(data)}.")
    if title_len > MAX_TITLE_BYTES or author_len > MAX_AUTHOR_BYTES:
        raise ValueError("Название или автор длиннее допустимого.")
    if meta_offset != HEADER_SIZE:
        raise ValueError("meta_offset должен быть равен 64.")
    if index_offset != meta_offset + title_len + author_len:
        raise ValueError("index_offset не соответствует концу метаданных.")
    if data_offset != index_offset + page_count * INDEX_ENTRY_SIZE:
        raise ValueError("data_offset не соответствует концу индекса.")
    if data_offset >= file_size:
        raise ValueError("Нет данных страниц: data_offset выходит за границы файла.")

    calculated_meta = crc32_zeroed(data, data_offset)
    if calculated_meta != meta_crc:
        raise ValueError(f"meta_crc32 не совпадает: в файле 0x{meta_crc:08X}, рассчитан 0x{calculated_meta:08X}.")
    calculated_file = crc32_zeroed(data)
    if calculated_file != file_crc:
        raise ValueError(f"file_crc32 не совпадает: в файле 0x{file_crc:08X}, рассчитан 0x{calculated_file:08X}.")

    limit = max_page_bytes(cols, rows)
    expected = data_offset
    raw_total = 0
    stored_total = 0
    packed_pages = 0
    image_pages = 0
    for number, (page_offset, stored, raw, width, height) in _iter_pages(data, page_count, index_offset):
        label = f"Страница {number + 1}"
        if stored == 0 or raw == 0:
            raise ValueError(f"{label}: нулевой размер.")
        if (width == 0) != (height == 0):
            raise ValueError(f"{label}: width и height должны быть оба нулевыми или оба ненулевыми.")
        is_image = width != 0
        if is_image:
            if width > img_w or height > img_h:
                raise ValueError(f"{label}: картинка {width}x{height} не помещается в окно {img_w}x{img_h}.")
            if raw != max_image_bytes(width, height):
                raise ValueError(f"{label}: raw_size={raw} не соответствует картинке {width}x{height}.")
        elif raw > limit:
            raise ValueError(f"{label}: размер {raw} превышает {limit}.")
        if compression == COMPRESSION_NONE and stored != raw:
            raise ValueError(f"{label}: stored_size != raw_size при отсутствии сжатия.")
        if stored > raw:
            raise ValueError(f"{label}: stored_size больше raw_size.")
        if page_offset != expected:
            raise ValueError(f"{label}: page_offset={page_offset}, ожидалось {expected}; промежуток или перекрытие.")
        end = page_offset + stored
        if end > file_size:
            raise ValueError(f"{label}: страница выходит за файл.")

        try:
            page = decompress_page(data[page_offset:end], raw, compression)
        except ValueError as exc:
            raise ValueError(f"{label}: {exc}") from exc
        if stored < raw:
            packed_pages += 1
        if is_image:
            image_pages += 1
            row_bytes = image_row_bytes(width)
            pad_mask = (1 << (row_bytes * 8 - width)) - 1
            if pad_mask and any(page[y * row_bytes + row_bytes - 1] & pad_mask for y in range(height)):
                raise ValueError(f"{label}: биты выравнивания строк картинки должны быть нулевыми.")
        else:
            if any((b < 0x20 and b != 0x0A) or b == 0x7F for b in page):
                raise ValueError(f"{label}: запрещённый управляющий символ в тексте.")
            if page.endswith(b"\n"):
                raise ValueError(f"{label}: запрещён завершающий LF.")
            lines = page.split(b"\n")
            if len(lines) > rows:
                raise ValueError(f"{label}: больше {rows} строк.")
            if any(len(line) > cols for line in lines):
                raise ValueError(f"{label}: строка длиннее {cols} байт.")
        expected = end
        raw_total += raw
        stored_total += stored

    if expected != file_size:
        raise ValueError("Конец последней страницы не совпадает с file_size.")

    title_start = meta_offset
    author_start = title_start + title_len
    return {
        "pages": page_count,
        "image_pages": image_pages,
        "file_size": file_size,
        "text_size": raw_total,       # размер всех страниц до сжатия (текст и картинки)
        "stored_size": stored_total,
        "packed_pages": packed_pages,
        "compression": compression,
        "crc32": file_crc,
        "cols": cols,
        "rows": rows,
        "img_w": img_w,
        "img_h": img_h,
        "title": pgo_bytes_to_text(data[title_start:author_start]),
        "author": pgo_bytes_to_text(data[author_start:index_offset]),
    }


def extract_all_pages(data: bytes) -> list[str | ImagePage]:
    """Все страницы готового PGB: текст (str) или картинка (ImagePage)."""
    fields = HEADER_STRUCT.unpack_from(data, 0)
    page_count, index_offset, compression = fields[5], fields[7], fields[14]
    pages: list[str | ImagePage] = []
    for _number, (offset, stored, raw, width, height) in _iter_pages(data, page_count, index_offset):
        page = decompress_page(data[offset:offset + stored], raw, compression)
        pages.append(ImagePage(width, height, page) if width else pgo_bytes_to_text(page))
    return pages


def extract_all_pages_text(data: bytes) -> list[str]:
    """Как extract_all_pages, но картинки заменены строкой-описанием (номера страниц сохраняются)."""
    return [page if isinstance(page, str) else f"[картинка {page.width}x{page.height}]"
            for page in extract_all_pages(data)]


def describe_compression(stats: dict) -> str:
    if stats["compression"] == COMPRESSION_NONE:
        return "нет"
    ratio = 100.0 * stats["stored_size"] / stats["text_size"]
    return f"LZ4 Block, данные страниц {ratio:.0f}% от исходных ({stats['packed_pages']} из {stats['pages']} стр. сжаты)"


# --------------------------------------------------------------------------
# Конвейер: источник -> документ -> строки -> страницы -> PGB
# --------------------------------------------------------------------------
@dataclass
class Options:
    encoding: str = "auto"
    title: str | None = None
    author: str | None = None
    reflow: str = "auto"
    indent: int = 2
    collapse_blank: bool = False
    blank_between_paragraphs: bool = False
    hyphenate: str | None = None
    cols: int = DEFAULT_COLS
    rows: int = DEFAULT_ROWS
    compression: str = "lz4"
    images: bool = True                  # иллюстрации из текста книги (FB2, EPUB)
    cover: bool = True                   # обложка первой страницей
    cover_file: Path | None = None       # своя обложка вместо обложки из книги
    dither: str = "fs"                   # fs, atkinson, bayer, none
    autocontrast: bool = True
    upscale: bool = False                # увеличивать картинки меньше окна (по умолчанию только уменьшаются)
    min_image: int = DEFAULT_MIN_IMAGE   # картинки меньше (обе стороны, px) пропускаются


@dataclass
class BookResult:
    data: bytes
    pages: list[bytes | ImagePage]   # страницы до сжатия: текст в PurrGO-256 или картинка
    lines: list[str]                 # строки текста (без картинок)
    stats: dict
    notes: list[str]


def _default_title(path: Path) -> str:
    name = path.name
    for suffix in (".fb2.zip", ".fb2", ".epub", ".zip", ".txt"):
        if name.lower().endswith(suffix):
            return name[: -len(suffix)]
    return path.stem


def _default_output(path: Path) -> Path:
    return path.with_name(_default_title(path) + ".pgb")


def load_document(path: Path, opts: Options) -> Document:
    """Первая стадия: чтение и разбор источника (зависит только от файла, кодировки и reflow)."""
    return read_document(path, opts.encoding, opts.reflow)


def _prepare_images(lines: list[str | ImageMark], doc: Document, opts: Options, notes: list[str]
                    ) -> tuple[list[str | ImageMark], dict[str, ImagePage], ImagePage | None]:
    """Преобразует картинки до разбиения на страницы.

    Возвращает строки без мест тех картинок, которые в книгу не попадают (выключены, слишком маленькие,
    не читаются: текст вокруг них не разрывается), словарь ключ -> страница-картинка и обложку (или None).
    """
    images = dict(doc.images)
    cover_key: str | None = None
    if opts.cover_file is not None:
        try:
            images["<cover-file>"] = Path(opts.cover_file).read_bytes()
        except OSError as exc:
            raise ValueError(f"Не удалось прочитать обложку {opts.cover_file}: {exc}") from exc
        cover_key = "<cover-file>"
    elif opts.cover and doc.cover is not None:
        cover_key = doc.cover

    marks = opts.images and any(isinstance(i, ImageMark) for i in lines)
    if not image_support_available():
        if marks or cover_key is not None:
            notes.append("Картинки пропущены: не установлен пакет Pillow (python -m pip install Pillow)")
        return [i for i in lines if not isinstance(i, ImageMark)], {}, None

    skipped_small = 0
    failed: list[str] = []
    converted: dict[str, ImagePage] = {}
    attempted: set[str] = set()

    def convert(key: str) -> ImagePage | None:
        nonlocal skipped_small
        if key in attempted:
            return converted.get(key)
        attempted.add(key)
        data = images.get(key)
        if data is None:
            return None
        try:
            converted[key] = image_to_page(data, opts.dither, opts.autocontrast, opts.upscale, opts.min_image)
        except ImageSkipped:
            skipped_small += 1
        except ValueError as exc:
            failed.append(f"{key}: {exc}")
        return converted.get(key)

    cover = convert(cover_key) if cover_key is not None else None
    kept: list[str | ImageMark] = []
    illustrations = 0
    for item in lines:
        if isinstance(item, ImageMark):
            if not opts.images or convert(item.key) is None:
                continue
            illustrations += 1
        kept.append(item)

    if cover is not None or illustrations:
        notes.append(f"Картинки: обложка {'есть' if cover is not None else 'нет'}, иллюстраций {illustrations} "
                     f"(дизеринг {opts.dither}, окно {IMG_WINDOW_W}x{IMG_WINDOW_H})")
    if skipped_small:
        notes.append(f"Пропущено маленьких картинок (меньше {opts.min_image} px): {skipped_small}")
    for message in failed[:5]:
        notes.append(f"Картинка не включена: {message}")
    if len(failed) > 5:
        notes.append(f"... и ещё {len(failed) - 5} картинок не включено")
    return kept, converted, cover


def render_document(doc: Document, path: Path, opts: Options) -> BookResult:
    """Вторая стадия: переносы, страницы, картинки, сжатие, сборка и проверка PGB."""
    if opts.compression not in COMPRESSION_NAMES:
        raise ValueError(f"Неизвестный способ сжатия: {opts.compression!r} (lz4 или none).")
    if opts.dither not in DITHER_MODES:
        raise ValueError(f"Неизвестный дизеринг: {opts.dither!r} ({', '.join(DITHER_MODES)}).")
    hyphenator = make_hyphenator(opts.hyphenate or None)
    lines = blocks_to_lines(doc.blocks, opts.cols, opts.indent, opts.collapse_blank,
                            opts.blank_between_paragraphs, hyphenator)
    notes = list(doc.notes)
    lines, converted, cover = _prepare_images(lines, doc, opts, notes)
    items = paginate(lines, opts.rows, opts.cols) if lines else []
    pages: list[bytes | ImagePage] = [cover] if cover is not None else []
    pages.extend(converted[item.key] if isinstance(item, ImageMark) else item for item in items)
    if not pages:
        raise ValueError("В книге не осталось ни одной страницы.")
    title = opts.title if opts.title else (doc.title or _default_title(path))
    author = opts.author if opts.author is not None and opts.author != "" else doc.author
    title_bytes = encode_meta(title, MAX_TITLE_BYTES, "Название", notes)
    author_bytes = encode_meta(author, MAX_AUTHOR_BYTES, "Автор", notes)

    data = build_pgb(pages, title_bytes, author_bytes, opts.cols, opts.rows, COMPRESSION_NAMES[opts.compression])
    stats = verify_pgb(data)
    if opts.compression == "lz4" and not lz4_library_available():
        notes.append("Сжатие: встроенный кодер LZ4 (пакет lz4 не установлен, с ним файл получится меньше)")
    text_lines = [line for line in lines if isinstance(line, str)]
    return BookResult(data=data, pages=pages, lines=text_lines, stats=stats, notes=notes)


def compile_book(path: Path, opts: Options) -> BookResult:
    return render_document(load_document(path, opts), path, opts)


# --------------------------------------------------------------------------
# Командная строка
# --------------------------------------------------------------------------
def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Конвертер txt / FB2 / EPUB в формат электронных книг PurrGO PGB 1.1 и проверка PGB. "
                    "Без параметров открывается графический интерфейс с предпросмотром.",
    )
    parser.add_argument("input", type=Path, nargs="?", default=None,
                        help="Исходная книга (.txt, .fb2, .fb2.zip, .epub) или готовый .pgb для проверки.")
    parser.add_argument("output", type=Path, nargs="?", default=None,
                        help="Выходной .pgb (по умолчанию - имя входного файла с расширением .pgb).")
    parser.add_argument("--encoding", choices=("auto", "utf-8", "cp1251", "cp866"), default="auto",
                        help="Кодировка txt (по умолчанию auto: UTF-8, иначе CP1251; CP866 только явно).")
    parser.add_argument("--title", help="Название книги (по умолчанию из метаданных или имя файла).")
    parser.add_argument("--author", help="Автор (по умолчанию из метаданных FB2/EPUB).")
    parser.add_argument("--reflow", choices=("auto", "on", "off"), default="auto",
                        help="txt: склеивать строки с жёсткими переносами (как в книгах Project Gutenberg) в абзацы.")
    parser.add_argument("--indent", type=int, default=2,
                        help="Отступ первой строки абзаца в пробелах (по умолчанию 2, 0 - без отступа).")
    parser.add_argument("--collapse-blank", action="store_true",
                        help="Сворачивать несколько пустых строк подряд в одну.")
    parser.add_argument("--blank-between-paragraphs", action="store_true",
                        help="Разделять абзацы пустой строкой (по умолчанию отключено).")
    parser.add_argument("--hyphenate", metavar="LANG",
                        help="Расставлять переносы по словарю pyphen (например ru_RU, en_US).")
    parser.add_argument("--compression", choices=tuple(COMPRESSION_NAMES), default="lz4",
                        help="Сжатие страниц: lz4 (LZ4 Block, по умолчанию) или none.")
    parser.add_argument("--cover", type=Path, metavar="FILE",
                        help="Своя обложка (jpg, png, ...) вместо обложки из книги; первая страница книги.")
    parser.add_argument("--no-cover", action="store_true", help="Не добавлять обложку.")
    parser.add_argument("--no-images", action="store_true",
                        help="Не включать иллюстрации из текста книги (FB2, EPUB); обложка управляется отдельно.")
    parser.add_argument("--dither", choices=DITHER_MODES, default="fs",
                        help="Дизеринг картинок до 1 bit: fs (Floyd-Steinberg, по умолчанию), atkinson, "
                             "bayer (упорядоченный), none (порог).")
    parser.add_argument("--no-autocontrast", action="store_true", help="Не растягивать контраст картинок.")
    parser.add_argument("--upscale", action="store_true",
                        help="Увеличивать картинки меньше окна читалки (по умолчанию только уменьшаются).")
    parser.add_argument("--min-image", type=int, default=DEFAULT_MIN_IMAGE, metavar="PX",
                        help=f"Пропускать картинки, у которых обе стороны меньше PX пикселей "
                             f"(украшения; по умолчанию {DEFAULT_MIN_IMAGE}).")
    parser.add_argument("--save-images", type=Path, metavar="DIR",
                        help="Сохранить страницы-картинки готовой книги в DIR как PNG (как их покажет читалка).")
    parser.add_argument("--cols", type=int, default=DEFAULT_COLS, help=argparse.SUPPRESS)
    parser.add_argument("--rows", type=int, default=DEFAULT_ROWS, help=argparse.SUPPRESS)
    parser.add_argument("--verify", action="store_true",
                        help="Перечитать записанный файл, проверить и показать первую и последнюю страницы.")
    parser.add_argument("--gui", action="store_true",
                        help="Открыть графический интерфейс (входной файл, если указан, загружается сразу).")
    return parser


def options_from_args(args: argparse.Namespace) -> Options:
    return Options(
        encoding=args.encoding, title=args.title, author=args.author, reflow=args.reflow,
        indent=args.indent, collapse_blank=args.collapse_blank,
        blank_between_paragraphs=args.blank_between_paragraphs, hyphenate=args.hyphenate,
        cols=args.cols, rows=args.rows, compression=args.compression,
        images=not args.no_images, cover=not args.no_cover, cover_file=args.cover,
        dither=args.dither, autocontrast=not args.no_autocontrast, upscale=args.upscale,
        min_image=args.min_image,
    )


def _print_page_preview(pages_text: list[str]) -> None:
    bar = "=" * 40
    print(f"\n{bar}\n ПЕРВАЯ СТРАНИЦА \n{bar}\n{pages_text[0]}\n{bar}")
    if len(pages_text) > 1:
        print(f"\n{bar}\n ПОСЛЕДНЯЯ СТРАНИЦА ({len(pages_text)} из {len(pages_text)}) \n{bar}\n{pages_text[-1]}\n{bar}")


def _print_stats(stats: dict) -> None:
    print(f"Название: {stats['title']}")
    if stats["author"]:
        print(f"Автор: {stats['author']}")
    print(f"Страниц: {stats['pages']} ({stats['cols']}x{stats['rows']} символов), "
          f"из них картинок: {stats['image_pages']} (окно {stats['img_w']}x{stats['img_h']})")
    print(f"Данные страниц до сжатия: {stats['text_size']} байт")
    print(f"Сжатие: {describe_compression(stats)}")
    print(f"Размер PGB: {stats['file_size']} байт")
    print(f"CRC-32: 0x{stats['crc32']:08X}")


def save_page_images(data: bytes, directory: Path) -> list[Path]:
    """Страницы-картинки PGB -> PNG-файлы page_NNNN.png (номер страницы с единицы)."""
    directory.mkdir(parents=True, exist_ok=True)
    written = []
    for number, page in enumerate(extract_all_pages(data), 1):
        if isinstance(page, ImagePage):
            target = directory / f"page_{number:04d}.png"
            target.write_bytes(page.to_png())
            written.append(target)
    return written


def main(argv: Iterable[str] | None = None) -> int:
    arguments = list(sys.argv[1:] if argv is None else argv)
    if not arguments:
        return run_gui()

    parser = build_parser()
    args = parser.parse_args(arguments)
    if args.gui:
        return run_gui(args.input, options_from_args(args))
    if args.input is None:
        parser.error("не указан входной файл")

    try:
        if args.input.suffix.lower() == ".pgb":
            try:
                raw_pgb = args.input.read_bytes()
            except OSError as exc:
                print(f"Ошибка: Не удалось прочитать {args.input}: {exc}", file=sys.stderr)
                return 2
            stats = verify_pgb(raw_pgb)
            print(f"Успешно проверен файл: {args.input}")
            _print_stats(stats)
            print("Формат и CRC соответствуют спецификации PGB 1.1: OK")
            _print_page_preview(extract_all_pages_text(raw_pgb))
            if args.save_images is not None:
                saved = save_page_images(raw_pgb, args.save_images)
                print(f"Сохранено страниц-картинок: {len(saved)} ({args.save_images})")
            return 0

        output_path = args.output if args.output is not None else _default_output(args.input)
        result = compile_book(args.input, options_from_args(args))
        try:
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_bytes(result.data)
        except OSError as exc:
            raise ValueError(f"Не удалось записать {output_path}: {exc}") from exc

        for note in result.notes:
            print(note)
        stats = result.stats
        if args.verify:
            written = output_path.read_bytes()
            stats = verify_pgb(written)
            print(f"Создан и проверен файл: {output_path}")
        else:
            print(f"Создан файл: {output_path}")
        print(f"Строк текста: {len(result.lines)}")
        _print_stats(stats)
        print("Сборка и проверка структуры выполнены успешно.")
        if args.save_images is not None:
            saved = save_page_images(output_path.read_bytes(), args.save_images)
            print(f"Сохранено страниц-картинок: {len(saved)} ({args.save_images})")
        if args.verify:
            _print_page_preview(extract_all_pages_text(written))
        return 0

    except (ValueError, RuntimeError, OSError) as exc:
        print(f"Ошибка: {exc}", file=sys.stderr)
        return 2


# --------------------------------------------------------------------------
# Графический интерфейс (tkinter): настройки слева, предпросмотр страниц справа
# --------------------------------------------------------------------------
try:
    import queue
    import threading
    import tkinter as tk
    from tkinter import filedialog, messagebox, ttk
    from tkinter import font as tkfont
except ImportError:  # tkinter не установлен (в Linux: sudo apt install python3-tk)
    tk = None  # type: ignore[assignment]

PREFERRED_MONO_FONTS = ("Consolas", "DejaVu Sans Mono", "Liberation Mono", "Cascadia Mono",
                        "Courier New", "Menlo", "Monaco")
HYPHEN_LANGUAGES = ("", "ru_RU", "uk_UA", "be_BY", "en_US", "en_GB", "de_DE", "fr_FR")
BOOK_FILETYPES = [
    ("Книги", "*.txt *.fb2 *.zip *.epub"),
    ("Текст", "*.txt"), ("FB2", "*.fb2 *.zip"), ("EPUB", "*.epub"),
    ("Все файлы", "*.*"),
]


def _format_size(size: int) -> str:
    if size >= 1024 * 1024:
        return f"{size / (1024 * 1024):.2f} МБ"
    if size >= 1024:
        return f"{size / 1024:.1f} КБ"
    return f"{size} байт"


class BookCompilerGui:
    """Окно конвертера. Обработка идёт в фоновом потоке, предпросмотр обновляется после правки настроек."""

    REBUILD_DELAY_MS = 400

    def __init__(self, initial: Path | None = None, options: Options | None = None) -> None:
        if tk is None:
            raise RuntimeError("tkinter не установлен")
        self.root = tk.Tk()
        self.root.title("PurrGO Book Compiler")
        self.root.minsize(900, 640)

        self.results: "queue.Queue[tuple]" = queue.Queue()
        self.busy = False
        self.pending = False
        self.rebuild_job: str | None = None
        self.cache: tuple[tuple, Document] | None = None
        self.result: BookResult | None = None
        self.view_mode = False          # показан открытый .pgb, а не результат конвертации
        self.pages: list[str | ImagePage] = []   # страницы предпросмотра: текст или картинка
        self.page_index = 0
        self.output_edited = False
        self._suspend = False           # запрет перезапуска обработки при программной правке полей
        self._slider_busy = False
        self._output_from_dialog = ""

        opts = options or Options()
        self.v_input = tk.StringVar()
        self.v_output = tk.StringVar()
        self.v_title = tk.StringVar(value=opts.title or "")
        self.v_author = tk.StringVar(value=opts.author or "")
        self.v_encoding = tk.StringVar(value=opts.encoding)
        self.v_reflow = tk.StringVar(value=opts.reflow)
        self.v_indent = tk.StringVar(value=str(opts.indent))
        self.v_collapse = tk.BooleanVar(value=opts.collapse_blank)
        self.v_blank_par = tk.BooleanVar(value=opts.blank_between_paragraphs)
        self.v_hyphen = tk.StringVar(value=opts.hyphenate or "")
        self.v_compression = tk.StringVar(value=opts.compression)
        self.v_images = tk.BooleanVar(value=opts.images)
        self.v_cover = tk.BooleanVar(value=opts.cover)
        self.v_dither = tk.StringVar(value=opts.dither)
        self.v_autocontrast = tk.BooleanVar(value=opts.autocontrast)
        self.v_upscale = tk.BooleanVar(value=opts.upscale)
        self.v_min_image = tk.StringVar(value=str(opts.min_image))
        self.cover_file: Path | None = opts.cover_file
        self._photo = None              # PhotoImage предпросмотра картинки (ссылка нужна, чтобы Tk её не удалил)
        self.v_spaces = tk.BooleanVar(value=False)
        self.v_font_size = tk.StringVar(value="12" if self.root.winfo_screenheight() >= 900 else "10")
        self.v_page = tk.StringVar(value="1")

        self._build_widgets()
        self._bind_events()
        self._poll()
        if initial is not None:
            self.set_input(initial)

    # ---- построение окна ----------------------------------------------------
    def _mono_font_family(self) -> str:
        available = set(tkfont.families(self.root))
        for name in PREFERRED_MONO_FONTS:
            if name in available:
                return name
        return tkfont.nametofont("TkFixedFont").actual("family")

    def _build_widgets(self) -> None:
        root = self.root
        root.columnconfigure(1, weight=1)
        root.rowconfigure(0, weight=1)

        left = ttk.Frame(root, padding=10)
        left.grid(row=0, column=0, sticky="ns")
        right = ttk.Frame(root, padding=(0, 10, 10, 10))
        right.grid(row=0, column=1, sticky="nsew")
        right.columnconfigure(0, weight=1)
        right.rowconfigure(1, weight=1)

        # -- файлы
        files = ttk.LabelFrame(left, text="Файлы", padding=8)
        files.grid(row=0, column=0, sticky="ew")
        files.columnconfigure(0, weight=1)
        ttk.Label(files, text="Книга (txt, FB2, EPUB):").grid(row=0, column=0, columnspan=2, sticky="w")
        ttk.Entry(files, textvariable=self.v_input, width=42).grid(row=1, column=0, sticky="ew")
        ttk.Button(files, text="Обзор…", command=self.choose_input).grid(row=1, column=1, padx=(6, 0))
        ttk.Label(files, text="Сохранить как (.pgb):").grid(row=2, column=0, columnspan=2, sticky="w", pady=(8, 0))
        self.entry_output = ttk.Entry(files, textvariable=self.v_output, width=42)
        self.entry_output.grid(row=3, column=0, sticky="ew")
        ttk.Button(files, text="Обзор…", command=self.choose_output).grid(row=3, column=1, padx=(6, 0))

        # -- метаданные
        meta = ttk.LabelFrame(left, text="Метаданные", padding=8)
        meta.grid(row=1, column=0, sticky="ew", pady=(8, 0))
        meta.columnconfigure(1, weight=1)
        ttk.Label(meta, text="Название:").grid(row=0, column=0, sticky="w")
        ttk.Entry(meta, textvariable=self.v_title).grid(row=0, column=1, sticky="ew", padx=(6, 0))
        ttk.Label(meta, text="Автор:").grid(row=1, column=0, sticky="w", pady=(4, 0))
        ttk.Entry(meta, textvariable=self.v_author).grid(row=1, column=1, sticky="ew", padx=(6, 0), pady=(4, 0))
        self.meta_hint = ttk.Label(meta, text="Пусто - из метаданных книги или имя файла", foreground="#666666",
                                   wraplength=330, justify="left")
        self.meta_hint.grid(row=2, column=0, columnspan=2, sticky="w", pady=(4, 0))

        # -- преобразование текста
        conv = ttk.LabelFrame(left, text="Преобразование текста", padding=8)
        conv.grid(row=2, column=0, sticky="ew", pady=(8, 0))
        conv.columnconfigure(1, weight=1)
        ttk.Label(conv, text="Кодировка txt:").grid(row=0, column=0, sticky="w")
        ttk.Combobox(conv, textvariable=self.v_encoding, state="readonly", width=14,
                     values=("auto", "utf-8", "cp1251", "cp866")).grid(row=0, column=1, sticky="w", padx=(6, 0))
        ttk.Label(conv, text="Склейка строк txt:").grid(row=1, column=0, sticky="w", pady=(4, 0))
        ttk.Combobox(conv, textvariable=self.v_reflow, state="readonly", width=14,
                     values=("auto", "on", "off")).grid(row=1, column=1, sticky="w", padx=(6, 0), pady=(4, 0))
        ttk.Label(conv, text="Отступ абзаца:").grid(row=2, column=0, sticky="w", pady=(4, 0))
        ttk.Spinbox(conv, textvariable=self.v_indent, from_=0, to=8, width=5, wrap=False,
                    command=self._on_option_changed).grid(row=2, column=1, sticky="w", padx=(6, 0), pady=(4, 0))
        ttk.Checkbutton(conv, text="Сворачивать пустые строки", variable=self.v_collapse,
                        command=self._on_option_changed).grid(row=3, column=0, columnspan=2, sticky="w", pady=(4, 0))
        ttk.Checkbutton(conv, text="Разделять абзацы пустой строкой", variable=self.v_blank_par,
                        command=self._on_option_changed).grid(row=4, column=0, columnspan=2, sticky="w", pady=(4, 0))
        ttk.Label(conv, text="Переносы (pyphen):").grid(row=5, column=0, sticky="w", pady=(4, 0))
        self.combo_hyphen = ttk.Combobox(conv, textvariable=self.v_hyphen, width=14, values=HYPHEN_LANGUAGES)
        self.combo_hyphen.grid(row=5, column=1, sticky="w", padx=(6, 0), pady=(4, 0))
        self.hyphen_hint = ttk.Label(conv, text="", foreground="#a00000", wraplength=330, justify="left")
        self.hyphen_hint.grid(row=6, column=0, columnspan=2, sticky="w")
        ttk.Label(conv, text="Сжатие страниц:").grid(row=7, column=0, sticky="w", pady=(4, 0))
        ttk.Combobox(conv, textvariable=self.v_compression, state="readonly", width=14,
                     values=tuple(COMPRESSION_NAMES)).grid(row=7, column=1, sticky="w", padx=(6, 0), pady=(4, 0))

        # -- картинки
        pic = ttk.LabelFrame(left, text="Обложка и иллюстрации (1 bit, окно 176x256)", padding=8)
        pic.grid(row=3, column=0, sticky="ew", pady=(8, 0))
        pic.columnconfigure(1, weight=1)
        ttk.Checkbutton(pic, text="Иллюстрации", variable=self.v_images,
                        command=self._on_option_changed).grid(row=0, column=0, sticky="w")
        ttk.Checkbutton(pic, text="Обложка", variable=self.v_cover,
                        command=self._on_option_changed).grid(row=0, column=1, sticky="w", padx=(6, 0))
        ttk.Label(pic, text="Дизеринг:").grid(row=1, column=0, sticky="w", pady=(4, 0))
        ttk.Combobox(pic, textvariable=self.v_dither, state="readonly", width=14,
                     values=DITHER_MODES).grid(row=1, column=1, sticky="w", padx=(6, 0), pady=(4, 0))
        ttk.Checkbutton(pic, text="Автоконтраст", variable=self.v_autocontrast,
                        command=self._on_option_changed).grid(row=2, column=0, sticky="w", pady=(4, 0))
        ttk.Checkbutton(pic, text="Увеличивать маленькие", variable=self.v_upscale,
                        command=self._on_option_changed).grid(row=2, column=1, sticky="w", padx=(6, 0), pady=(4, 0))
        ttk.Label(pic, text="Пропускать меньше (px):").grid(row=3, column=0, sticky="w", pady=(4, 0))
        ttk.Spinbox(pic, textvariable=self.v_min_image, from_=0, to=256, increment=16, width=6, wrap=False,
                    command=self._on_option_changed).grid(row=3, column=1, sticky="w", padx=(6, 0), pady=(4, 0))
        cover_row = ttk.Frame(pic)
        cover_row.grid(row=4, column=0, columnspan=2, sticky="ew", pady=(4, 0))
        cover_row.columnconfigure(2, weight=1)
        ttk.Button(cover_row, text="Своя обложка…", command=self.choose_cover).grid(row=0, column=0)
        ttk.Button(cover_row, text="×", width=3, command=self.clear_cover).grid(row=0, column=1, padx=(4, 6))
        self.lbl_cover = ttk.Label(cover_row, text="из книги", foreground="#555555")
        self.lbl_cover.grid(row=0, column=2, sticky="w")
        self.pic_hint = ttk.Label(pic, text="", foreground="#a00000", wraplength=330, justify="left")
        self.pic_hint.grid(row=5, column=0, columnspan=2, sticky="w")

        # -- результат
        res = ttk.LabelFrame(left, text="Результат", padding=8)
        res.grid(row=4, column=0, sticky="ew", pady=(8, 0))
        res.columnconfigure(0, weight=1)
        self.info = ttk.Label(res, text="Выберите книгу", justify="left", wraplength=360)
        self.info.grid(row=0, column=0, sticky="w")
        self.progress = ttk.Progressbar(res, mode="indeterminate", length=200)
        self.notes = ttk.Label(res, text="", foreground="#555555", justify="left", wraplength=360)
        self.notes.grid(row=2, column=0, sticky="w", pady=(6, 0))

        buttons = ttk.Frame(left)
        buttons.grid(row=5, column=0, sticky="ew", pady=(10, 0))
        buttons.columnconfigure(0, weight=1)
        self.btn_save = ttk.Button(buttons, text="Сохранить PGB", command=self.save, state="disabled")
        self.btn_save.grid(row=0, column=0, sticky="ew")
        ttk.Button(buttons, text="Открыть PGB…", command=self.open_pgb).grid(row=0, column=1, padx=(6, 0))
        self.status = ttk.Label(left, text="", wraplength=380, justify="left")
        self.status.grid(row=6, column=0, sticky="w", pady=(8, 0))

        # -- предпросмотр
        nav = ttk.Frame(right)
        nav.grid(row=0, column=0, sticky="ew")
        nav.columnconfigure(5, weight=1)
        self.btn_first = ttk.Button(nav, text="|<", width=3, command=lambda: self.show_page(0))
        self.btn_prev = ttk.Button(nav, text="<", width=3, command=lambda: self.show_page(self.page_index - 1))
        self.entry_page = ttk.Spinbox(nav, textvariable=self.v_page, width=7, from_=1, to=1, wrap=False,
                                      command=self._on_page_entered)
        self.lbl_total = ttk.Label(nav, text="из 0")
        self.btn_next = ttk.Button(nav, text=">", width=3, command=lambda: self.show_page(self.page_index + 1))
        self.btn_last = ttk.Button(nav, text=">|", width=3, command=lambda: self.show_page(len(self.pages) - 1))
        self.btn_first.grid(row=0, column=0)
        self.btn_prev.grid(row=0, column=1, padx=(2, 8))
        self.entry_page.grid(row=0, column=2)
        self.lbl_total.grid(row=0, column=3, padx=(4, 8))
        self.btn_next.grid(row=0, column=4)
        self.btn_last.grid(row=0, column=5, sticky="w", padx=(2, 0))
        ttk.Checkbutton(nav, text="пробелы как ·", variable=self.v_spaces,
                        command=lambda: self.show_page(self.page_index)).grid(row=0, column=6, padx=(8, 0))
        ttk.Label(nav, text="Шрифт:").grid(row=0, column=7, padx=(12, 2))
        ttk.Spinbox(nav, textvariable=self.v_font_size, from_=8, to=28, width=3, wrap=False,
                    command=self._apply_font).grid(row=0, column=8)

        stage = ttk.Frame(right)
        stage.grid(row=1, column=0, sticky="nsew", pady=(8, 0))
        stage.columnconfigure(0, weight=1)
        stage.rowconfigure(1, weight=1)
        holder = ttk.Frame(stage)
        holder.grid(row=0, column=0, rowspan=2)

        family = self._mono_font_family()
        self.mono = tkfont.Font(family=family, size=int(self.v_font_size.get()))
        self.ruler = tk.Label(holder, font=self.mono, anchor="w", justify="left", padx=7, fg="#888888",
                              text=self._ruler_text(DEFAULT_COLS))
        self.ruler.grid(row=0, column=0, sticky="w")
        self.text = tk.Text(holder, width=DEFAULT_COLS, height=DEFAULT_ROWS, wrap="none", font=self.mono,
                            relief="solid", borderwidth=1, padx=6, pady=4, background="#ffffff",
                            foreground="#000000", state="disabled", cursor="arrow", takefocus=1,
                            spacing1=0, spacing2=0, spacing3=0)
        self.text.grid(row=1, column=0)
        # Страница-картинка показывается на месте текста (в той же ячейке), увеличенная вдвое.
        self.image_label = tk.Label(holder, background="#ffffff", relief="solid", borderwidth=1)
        self.slider = ttk.Scale(right, from_=1, to=1, orient="horizontal", command=self._on_slider)
        self.slider.grid(row=2, column=0, sticky="ew", pady=(8, 0))
        self._set_nav_state(False)

        if not image_support_available():
            self.pic_hint.config(text="Картинки отключены: python -m pip install Pillow")
        if "pyphen" not in sys.modules:
            try:
                import pyphen  # noqa: F401
            except ImportError:
                self.combo_hyphen.state(["disabled"])
                self.hyphen_hint.config(text="Переносы по словарю: python -m pip install pyphen")

    @staticmethod
    def _ruler_text(cols: int) -> str:
        return "".join(str((i + 1) % 10) for i in range(cols))

    def _bind_events(self) -> None:
        for var in (self.v_title, self.v_author, self.v_encoding, self.v_reflow, self.v_indent,
                    self.v_hyphen, self.v_compression, self.v_dither, self.v_min_image):
            var.trace_add("write", lambda *_: self._on_option_changed())
        self.v_output.trace_add("write", lambda *_: self._on_output_typed())
        self.v_input.trace_add("write", lambda *_: None)
        self.entry_page.bind("<Return>", lambda _e: self._on_page_entered())
        self.entry_page.bind("<FocusOut>", lambda _e: self._on_page_entered())

        self.root.bind("<Left>", lambda _e: self._key_nav(-1))
        self.root.bind("<Right>", lambda _e: self._key_nav(1))
        self.root.bind("<Prior>", lambda _e: self._key_nav(-10))
        self.root.bind("<Next>", lambda _e: self._key_nav(10))
        self.root.bind("<Home>", lambda _e: self._key_nav(None, first=True))
        self.root.bind("<End>", lambda _e: self._key_nav(None, last=True))
        self.text.bind("<MouseWheel>", lambda e: self.show_page(self.page_index + (-1 if e.delta > 0 else 1)))
        self.text.bind("<Button-4>", lambda _e: self.show_page(self.page_index - 1))
        self.text.bind("<Button-5>", lambda _e: self.show_page(self.page_index + 1))
        self.text.bind("<Button-1>", lambda _e: self.text.focus_set())
        self.image_label.bind("<MouseWheel>", lambda e: self.show_page(self.page_index + (-1 if e.delta > 0 else 1)))
        self.image_label.bind("<Button-4>", lambda _e: self.show_page(self.page_index - 1))
        self.image_label.bind("<Button-5>", lambda _e: self.show_page(self.page_index + 1))

    def _key_nav(self, delta: int | None, first: bool = False, last: bool = False) -> None:
        focus = self.root.focus_get()
        if isinstance(focus, (tk.Entry, ttk.Entry, tk.Spinbox, ttk.Spinbox, ttk.Combobox)):
            return
        if first:
            self.show_page(0)
        elif last:
            self.show_page(len(self.pages) - 1)
        elif delta is not None:
            self.show_page(self.page_index + delta)

    # ---- файлы -------------------------------------------------------------
    def choose_input(self) -> None:
        name = filedialog.askopenfilename(title="Выберите книгу", filetypes=BOOK_FILETYPES)
        if name:
            self.set_input(Path(name))

    def choose_output(self) -> None:
        initial = Path(self.v_output.get()) if self.v_output.get() else None
        name = filedialog.asksaveasfilename(
            title="Сохранить PGB", defaultextension=".pgb", filetypes=[("PGB", "*.pgb *.PGB"), ("Все файлы", "*.*")],
            initialdir=str(initial.parent) if initial else None, initialfile=initial.name if initial else None)
        if name:
            self.output_edited = True
            self.v_output.set(name)
            self._output_from_dialog = name

    def choose_cover(self) -> None:
        name = filedialog.askopenfilename(
            title="Выберите обложку",
            filetypes=[("Картинки", "*.jpg *.jpeg *.png *.gif *.bmp *.webp"), ("Все файлы", "*.*")])
        if name:
            self.cover_file = Path(name)
            self.lbl_cover.config(text=self.cover_file.name)
            self._on_option_changed()

    def clear_cover(self) -> None:
        self.cover_file = None
        self.lbl_cover.config(text="из книги")
        self._on_option_changed()

    def _on_output_typed(self) -> None:
        if not self._suspend:
            self.output_edited = True

    def set_input(self, path: Path) -> None:
        self._suspend = True
        try:
            self.v_input.set(str(path))
            if not self.output_edited:
                self.v_output.set(str(_default_output(path)))
            self.v_title.set("")
            self.v_author.set("")
        finally:
            self._suspend = False
        self.view_mode = False
        self.cache = None
        self.status.config(text="")
        self.request_build(immediate=True)

    # ---- обработка ----------------------------------------------------------
    def _on_option_changed(self) -> None:
        if self._suspend or not self.v_input.get().strip():
            return
        self.request_build()

    def collect_options(self) -> Options:
        try:
            indent = max(0, min(8, int(self.v_indent.get())))
        except ValueError:
            indent = 2
        try:
            min_image = max(0, int(self.v_min_image.get()))
        except ValueError:
            min_image = DEFAULT_MIN_IMAGE
        return Options(
            encoding=self.v_encoding.get(), title=self.v_title.get().strip() or None,
            author=self.v_author.get().strip() or None, reflow=self.v_reflow.get(), indent=indent,
            collapse_blank=self.v_collapse.get(),
            blank_between_paragraphs=self.v_blank_par.get(), hyphenate=self.v_hyphen.get().strip() or None,
            compression=self.v_compression.get(),
            images=self.v_images.get(), cover=self.v_cover.get(), cover_file=self.cover_file,
            dither=self.v_dither.get(), autocontrast=self.v_autocontrast.get(), upscale=self.v_upscale.get(),
            min_image=min_image,
        )

    def request_build(self, immediate: bool = False) -> None:
        if self.rebuild_job is not None:
            self.root.after_cancel(self.rebuild_job)
            self.rebuild_job = None
        self.rebuild_job = self.root.after(0 if immediate else self.REBUILD_DELAY_MS, self._start_build)

    def _start_build(self) -> None:
        self.rebuild_job = None
        if self.busy:
            self.pending = True
            return
        path_text = self.v_input.get().strip()
        if not path_text:
            return
        self.pending = False
        self.busy = True
        self.view_mode = False
        self.btn_save.state(["disabled"])
        self.progress.grid(row=1, column=0, sticky="w", pady=(6, 0))
        self.progress.start(12)
        self.status.config(text="Обработка…", foreground="")
        opts = self.collect_options()
        threading.Thread(target=self._worker, args=(Path(path_text), opts, self.cache), daemon=True).start()

    def _worker(self, path: Path, opts: Options, cache: tuple[tuple, Document] | None) -> None:
        try:
            key = (str(path), path.stat().st_mtime_ns, opts.encoding, opts.reflow)
            if cache is not None and cache[0] == key:
                doc = cache[1]
            else:
                doc = load_document(path, opts)
            result = render_document(doc, path, opts)
            self.results.put(("ok", key, doc, result))
        except Exception as exc:  # noqa: BLE001 - любая ошибка показывается пользователю
            self.results.put(("error", str(exc) or exc.__class__.__name__))

    def _poll(self) -> None:
        try:
            while True:
                self._handle(self.results.get_nowait())
        except queue.Empty:
            pass
        self.root.after(60, self._poll)

    def _handle(self, message: tuple) -> None:
        self.busy = False
        self.progress.stop()
        self.progress.grid_remove()
        if message[0] == "ok":
            _kind, key, doc, result = message
            self.cache = (key, doc)
            if self.pending:
                self.request_build(immediate=True)
                return
            self.result = result
            self.view_mode = False
            self._show_result(doc, result)
        else:
            if self.pending:
                self.request_build(immediate=True)
                return
            self.result = None
            self._set_pages([])
            self.btn_save.state(["disabled"])
            self.info.config(text="Не удалось преобразовать книгу")
            self.notes.config(text="")
            self.status.config(text=f"Ошибка: {message[1]}", foreground="#b00020")

    def _show_result(self, doc: Document, result: BookResult) -> None:
        stats = result.stats
        ratio = 100.0 * stats["file_size"] / max(1, stats["text_size"])
        self.info.config(text=(
            f"Страниц: {stats['pages']} (картинок: {stats['image_pages']})   строк: {len(result.lines)}\n"
            f"Страницы: {_format_size(stats['text_size'])}  →  PGB: {_format_size(stats['file_size'])} ({ratio:.0f}%)\n"
            f"Сжатие: {describe_compression(stats)}\n"
            f"Название: {stats['title']}\nАвтор: {stats['author'] or '-'}"))
        self.notes.config(text="\n".join(result.notes))
        hint = " / ".join(x for x in (doc.title, doc.author) if x)
        self.meta_hint.config(text=("В книге: " + hint) if hint else "Пусто - из метаданных книги или имя файла")
        self.status.config(text="Предпросмотр обновлён. Проверьте текст и сохраните.", foreground="")
        self._set_pages([page if isinstance(page, ImagePage) else pgo_bytes_to_text(page)
                         for page in result.pages])
        self.btn_save.state(["!disabled"])

    def save(self) -> None:
        if self.result is None or self.view_mode or self.busy:
            return
        target = self.v_output.get().strip()
        if not target:
            self.choose_output()
            target = self.v_output.get().strip()
            if not target:
                return
        path = Path(target)
        if path.exists() and target != self._output_from_dialog:
            if not messagebox.askyesno("Файл существует", f"Заменить {path}?"):
                return
        try:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(self.result.data)
            verify_pgb(path.read_bytes())
        except (OSError, ValueError) as exc:
            self.status.config(text=f"Ошибка записи: {exc}", foreground="#b00020")
            return
        self.status.config(text=f"Сохранено: {path} ({len(self.result.data)} байт), проверка CRC и страниц: OK",
                           foreground="#006400")

    def open_pgb(self) -> None:
        name = filedialog.askopenfilename(title="Открыть PGB", filetypes=[("PGB", "*.pgb *.PGB"), ("Все файлы", "*.*")])
        if name:
            self.load_pgb(Path(name))

    def load_pgb(self, path: Path) -> None:
        try:
            raw = path.read_bytes()
            stats = verify_pgb(raw)
            pages = extract_all_pages(raw)
        except (OSError, ValueError) as exc:
            self.status.config(text=f"Не удалось открыть {path.name}: {exc}", foreground="#b00020")
            return
        self.view_mode = True
        self.result = None
        self.btn_save.state(["disabled"])
        self.info.config(text=(
            f"Просмотр PGB: {path.name}\n"
            f"Страниц: {stats['pages']} ({stats['cols']}x{stats['rows']}, картинок: {stats['image_pages']})\n"
            f"Страницы: {_format_size(stats['text_size'])}  →  PGB: {_format_size(stats['file_size'])}\n"
            f"Сжатие: {describe_compression(stats)}\n"
            f"Название: {stats['title']}\nАвтор: {stats['author'] or '-'}\nCRC-32: 0x{stats['crc32']:08X}"))
        self.notes.config(text="")
        self.status.config(text="Файл проверен (структура, оба CRC, страницы). Для новой конвертации выберите книгу.",
                           foreground="#006400")
        self._set_pages(pages)

    # ---- предпросмотр -------------------------------------------------------
    def _apply_font(self) -> None:
        try:
            size = max(8, min(28, int(self.v_font_size.get())))
        except ValueError:
            size = 12
        self.mono.configure(size=size)

    def _set_nav_state(self, enabled: bool) -> None:
        state = ["!disabled"] if enabled else ["disabled"]
        for widget in (self.btn_first, self.btn_prev, self.btn_next, self.btn_last, self.entry_page, self.slider):
            widget.state(state)

    def _set_pages(self, pages: list[str | ImagePage]) -> None:
        self.pages = pages
        total = len(pages)
        self.lbl_total.config(text=f"из {total}")
        self.entry_page.config(to=max(1, total))
        self.slider.config(to=max(1, total))
        self._set_nav_state(total > 0)
        keep = min(self.page_index, max(0, total - 1))
        self.show_page(keep)

    def _show_image(self, page: ImagePage) -> None:
        """Страница-картинка точно в том виде, как её покажет читалка (1 bit), увеличенная вдвое."""
        photo = tk.PhotoImage(data=base64.b64encode(page.to_png())).zoom(2)
        self._photo = photo
        self.image_label.config(image=photo)
        self.text.grid_remove()
        self.image_label.grid(row=1, column=0)
        self.ruler.config(text=f"Картинка {page.width}x{page.height} (в окне {IMG_WINDOW_W}x{IMG_WINDOW_H}), x2")

    def _show_text(self) -> None:
        self.image_label.grid_remove()
        self.text.grid(row=1, column=0)
        self.ruler.config(text=self._ruler_text(DEFAULT_COLS))

    def show_page(self, index: int) -> None:
        total = len(self.pages)
        self.text.config(state="normal")
        self.text.delete("1.0", "end")
        if total == 0:
            self.page_index = 0
            self.v_page.set("1")
            self._show_text()
            self.text.config(state="disabled")
            return
        index = max(0, min(total - 1, index))
        self.page_index = index
        page = self.pages[index]
        if isinstance(page, ImagePage):
            self._show_image(page)
        else:
            self._show_text()
            self.text.insert("1.0", page.replace(" ", "·") if self.v_spaces.get() else page)
        self.text.config(state="disabled")
        self.v_page.set(str(index + 1))
        self._slider_busy = True
        self.slider.set(index + 1)
        self._slider_busy = False

    def _on_page_entered(self) -> None:
        try:
            self.show_page(int(self.v_page.get()) - 1)
        except ValueError:
            self.show_page(self.page_index)

    def _on_slider(self, value: str) -> None:
        if self._slider_busy or not self.pages:
            return
        index = int(round(float(value))) - 1
        if index != self.page_index:
            self.show_page(index)

    # ---- запуск -------------------------------------------------------------
    def run(self) -> int:
        self.root.mainloop()
        return 0


def run_gui(initial: Path | None = None, options: Options | None = None) -> int:
    if tk is None:
        print("Графический интерфейс недоступен: не найден tkinter "
              "(в Linux: sudo apt install python3-tk). Запустите с параметрами, см. --help.", file=sys.stderr)
        return 2
    try:
        app = BookCompilerGui(initial, options)
    except tk.TclError as exc:
        print(f"Не удалось открыть окно ({exc}). Запустите с параметрами, см. --help.", file=sys.stderr)
        return 2
    return app.run()


if __name__ == "__main__":
    raise SystemExit(main())
