#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Конвертер книг (txt / FB2 / EPUB) в бинарный формат электронных книг PGB 1.0 для PurrGO.

Зависимости: только стандартная библиотека Python 3.10+.
Необязательно: пакет `pyphen` (нужен только для --hyphenate).

Примеры:
    python purrgo_book_compiler.py book.txt                       # -> book.pgb
    python purrgo_book_compiler.py book.fb2 BOOK.PGB --verify
    python purrgo_book_compiler.py book.epub --indent 2
    python purrgo_book_compiler.py book.txt --encoding cp1251 --title "Название" --author "Автор"
    python purrgo_book_compiler.py book.pgb                       # проверка готового файла
"""

from __future__ import annotations

import argparse
import functools
import html.parser
import importlib.util
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
# Константы формата PGB 1.0 (см. PGB-format.md)
# --------------------------------------------------------------------------
MAGIC = b"PGB1"
VERSION_MAJOR = 1
VERSION_MINOR = 0
HEADER_SIZE = 64
INDEX_ENTRY_SIZE = 8
DEFAULT_COLS = 29
DEFAULT_ROWS = 32
MAX_TITLE_BYTES = 200
MAX_AUTHOR_BYTES = 100
COMPRESSION_NONE = 0

FILE_CRC_OFFSET = 32
META_CRC_OFFSET = 36

UINT16_MAX = 0xFFFF
UINT32_MAX = 0xFFFFFFFF

# magic, ver_major, ver_minor, header_size, flags,
# page_count, meta_offset, index_offset, data_offset, file_size, file_crc32, meta_crc32,
# cols, rows, compression, reserved0, title_length, author_length, reserved1[16]
HEADER_STRUCT = struct.Struct("<4sHHHHIIIIIIIBBBBHH16s")
INDEX_STRUCT = struct.Struct("<IHH")  # page_offset, stored_size, raw_size

assert HEADER_STRUCT.size == HEADER_SIZE
assert INDEX_STRUCT.size == INDEX_ENTRY_SIZE


def max_page_bytes(cols: int, rows: int) -> int:
    return cols * rows + (rows - 1)


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
    """kind: 'p' абзац (с отступом), 'v' строка без отступа, 't' заголовок (по центру), 'b' пустая строка."""
    kind: str
    text: str = ""


@dataclass
class Document:
    title: str = ""
    author: str = ""
    blocks: list[Block] = field(default_factory=list)
    notes: list[str] = field(default_factory=list)  # сообщения о принятых решениях


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


def fb2_to_document(raw: bytes) -> Document:
    try:
        root = ET.fromstring(raw)
    except ET.ParseError as exc:
        raise ValueError(f"Некорректный FB2 (XML): {exc}") from exc

    doc = Document()
    for info in root.iter():
        if _local(info.tag) == "title-info":
            for child in info:
                name = _local(child.tag)
                if name == "book-title" and not doc.title:
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
        elif tag in ("image", "binary", "description"):
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

    def __init__(self) -> None:
        super().__init__(convert_charrefs=True)
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
        for el in opf.iter():
            name = _local(el.tag)
            if name == "title" and not doc.title and (el.text or "").strip():
                doc.title = " ".join(el.text.split())
            elif name == "creator" and not doc.author and (el.text or "").strip():
                doc.author = " ".join(el.text.split())
            elif name == "item" and el.get("id"):
                manifest[el.get("id")] = (
                    el.get("href", ""), el.get("media-type", ""), el.get("properties", ""),
                )
            elif name == "itemref" and el.get("idref"):
                spine.append((el.get("idref"), el.get("linear", "yes") != "no"))

        base = posixpath.dirname(opf_path)
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
            parser = _HtmlBlocks()
            parser.feed(_decode_markup(archive.read(member)))
            parser.close()
            doc.blocks.extend(parser.blocks)

        if missing:
            doc.notes.append(f"EPUB: {missing} файл(ов) из spine не найдено в архиве и пропущено")
        if not doc.blocks:
            raise ValueError("В EPUB не найден текст (возможно, книга защищена DRM).")
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
                    hyphenator: Callable[[str], list[str]] | None = None) -> list[str]:
    if not 0 <= indent < width:
        raise ValueError(f"--indent должен быть от 0 до {width - 1}.")
    lines: list[str] = []
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

    while lines and lines[0] == "":
        lines.pop(0)
    while lines and lines[-1] == "":
        lines.pop()
    if not lines:
        raise ValueError("Исходный текст пуст: PGB не допускает пустую страницу.")
    return lines


def paginate(lines: list[str], rows: int = DEFAULT_ROWS, cols: int = DEFAULT_COLS) -> list[bytes]:
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


def build_pgb(pages: list[bytes], title: bytes, author: bytes,
              cols: int = DEFAULT_COLS, rows: int = DEFAULT_ROWS) -> bytes:
    if not pages:
        raise ValueError("В книге должна быть хотя бы одна страница.")
    if not (1 <= cols <= 255 and 1 <= rows <= 255):
        raise ValueError("cols и rows должны быть в диапазоне 1..255.")
    if len(title) > MAX_TITLE_BYTES or len(author) > MAX_AUTHOR_BYTES:
        raise ValueError("Название или автор слишком длинные.")

    page_count = len(pages)
    meta_offset = HEADER_SIZE
    index_offset = meta_offset + len(title) + len(author)
    data_offset = index_offset + page_count * INDEX_ENTRY_SIZE

    index = bytearray()
    offset = data_offset
    for page in pages:
        index += INDEX_STRUCT.pack(offset, len(page), len(page))
        offset += len(page)
        if offset > UINT32_MAX:
            raise ValueError("Размер файла превышает диапазон uint32.")
    file_size = offset

    header = HEADER_STRUCT.pack(
        MAGIC, VERSION_MAJOR, VERSION_MINOR, HEADER_SIZE, 0,
        page_count, meta_offset, index_offset, data_offset, file_size, 0, 0,
        cols, rows, COMPRESSION_NONE, 0, len(title), len(author), b"\x00" * 16,
    )
    result = bytearray(header)
    result += title
    result += author
    result += index
    for page in pages:
        result += page
    if len(result) != file_size:
        raise AssertionError(f"Размер собранного файла {len(result)} != {file_size}")

    struct.pack_into("<I", result, META_CRC_OFFSET, crc32_zeroed(result, data_offset))
    struct.pack_into("<I", result, FILE_CRC_OFFSET, crc32_zeroed(result))
    return bytes(result)


def verify_pgb(data: bytes) -> dict:
    if len(data) < HEADER_SIZE:
        raise ValueError("Файл короче заголовка PGB.")
    (
        magic, major, minor, header_size, flags, page_count, meta_offset, index_offset,
        data_offset, file_size, file_crc, meta_crc, cols, rows, compression, reserved0,
        title_len, author_len, reserved1,
    ) = HEADER_STRUCT.unpack_from(data, 0)

    if magic != MAGIC:
        raise ValueError("Неверный magic: ожидалось PGB1.")
    if (major, minor) != (VERSION_MAJOR, VERSION_MINOR):
        raise ValueError(f"Неподдерживаемая версия PGB {major}.{minor}.")
    if header_size != HEADER_SIZE:
        raise ValueError(f"Неверный header_size: {header_size}.")
    if flags != 0:
        raise ValueError(f"Неподдерживаемые flags: 0x{flags:04X}.")
    if compression != COMPRESSION_NONE:
        raise ValueError(f"Неподдерживаемый способ сжатия: {compression}.")
    if reserved0 != 0 or any(reserved1):
        raise ValueError("Зарезервированные поля заголовка должны быть нулевыми.")
    if page_count == 0:
        raise ValueError("page_count равен нулю.")
    if cols == 0 or rows == 0:
        raise ValueError("cols и rows должны быть больше нуля.")
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
    total = 0
    for number in range(page_count):
        page_offset, stored, raw = INDEX_STRUCT.unpack_from(data, index_offset + number * INDEX_ENTRY_SIZE)
        label = f"Страница {number + 1}"
        if stored == 0 or raw == 0:
            raise ValueError(f"{label}: нулевой размер.")
        if stored != raw:
            raise ValueError(f"{label}: stored_size != raw_size при отсутствии сжатия.")
        if raw > limit:
            raise ValueError(f"{label}: размер {raw} превышает {limit}.")
        if page_offset != expected:
            raise ValueError(f"{label}: page_offset={page_offset}, ожидалось {expected}; промежуток или перекрытие.")
        end = page_offset + stored
        if end > file_size:
            raise ValueError(f"{label}: страница выходит за файл.")

        page = data[page_offset:end]
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
        total += raw

    if expected != file_size:
        raise ValueError("Конец последней страницы не совпадает с file_size.")

    title_start = meta_offset
    author_start = title_start + title_len
    return {
        "pages": page_count,
        "file_size": file_size,
        "text_size": total,
        "crc32": file_crc,
        "cols": cols,
        "rows": rows,
        "title": pgo_bytes_to_text(data[title_start:author_start]),
        "author": pgo_bytes_to_text(data[author_start:index_offset]),
    }


def extract_all_pages_text(data: bytes) -> list[str]:
    fields = HEADER_STRUCT.unpack_from(data, 0)
    page_count, index_offset = fields[5], fields[7]
    pages = []
    for number in range(page_count):
        offset, stored, _raw = INDEX_STRUCT.unpack_from(data, index_offset + number * INDEX_ENTRY_SIZE)
        pages.append(pgo_bytes_to_text(data[offset:offset + stored]))
    return pages


# --------------------------------------------------------------------------
# Командная строка
# --------------------------------------------------------------------------
def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Конвертер txt / FB2 / EPUB в формат электронных книг PurrGO PGB 1.0 и проверка PGB."
    )
    parser.add_argument("input", type=Path,
                        help="Исходная книга (.txt, .fb2, .fb2.zip, .epub) или готовый .pgb для проверки.")
    parser.add_argument("output", type=Path, nargs="?", default=None,
                        help="Выходной .pgb (по умолчанию - имя входного файла с расширением .pgb).")
    parser.add_argument("--encoding", choices=("auto", "utf-8", "cp1251", "cp866"), default="auto",
                        help="Кодировка txt (по умолчанию auto: UTF-8, иначе CP1251; CP866 только явно).")
    parser.add_argument("--title", help="Название книги (по умолчанию из метаданных или имя файла).")
    parser.add_argument("--author", help="Автор (по умолчанию из метаданных FB2/EPUB).")
    parser.add_argument("--reflow", choices=("auto", "on", "off"), default="auto",
                        help="txt: склеивать строки с жёсткими переносами в абзацы (по умолчанию auto).")
    parser.add_argument("--indent", type=int, default=2,
                        help="Отступ первой строки абзаца в пробелах (по умолчанию 2, 0 - без отступа).")
    parser.add_argument("--collapse-blank", action="store_true",
                        help="Сворачивать несколько пустых строк подряд в одну.")
    parser.add_argument("--hyphenate", metavar="LANG",
                        help="Расставлять переносы по словарю pyphen (например ru_RU, en_US).")
    parser.add_argument("--cols", type=int, default=DEFAULT_COLS, help=argparse.SUPPRESS)
    parser.add_argument("--rows", type=int, default=DEFAULT_ROWS, help=argparse.SUPPRESS)
    parser.add_argument("--verify", action="store_true",
                        help="Перечитать записанный файл, проверить и показать первую и последнюю страницы.")
    return parser


def _print_page_preview(pages_text: list[str]) -> None:
    bar = "=" * 40
    print(f"\n{bar}\n ПЕРВАЯ СТРАНИЦА \n{bar}\n{pages_text[0]}\n{bar}")
    if len(pages_text) > 1:
        print(f"\n{bar}\n ПОСЛЕДНЯЯ СТРАНИЦА ({len(pages_text)} из {len(pages_text)}) \n{bar}\n{pages_text[-1]}\n{bar}")


def _default_title(path: Path) -> str:
    name = path.name
    for suffix in (".fb2.zip", ".fb2", ".epub", ".zip", ".txt"):
        if name.lower().endswith(suffix):
            return name[: -len(suffix)]
    return path.stem


def _default_output(path: Path) -> Path:
    return path.with_name(_default_title(path) + ".pgb")


def main(argv: Iterable[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        if args.input.suffix.lower() == ".pgb":
            try:
                raw_pgb = args.input.read_bytes()
            except OSError as exc:
                print(f"Ошибка: Не удалось прочитать {args.input}: {exc}", file=sys.stderr)
                return 2
            stats = verify_pgb(raw_pgb)
            print(f"Успешно проверен файл: {args.input}")
            print(f"Название: {stats['title']}")
            print(f"Автор: {stats['author']}")
            print(f"Страниц: {stats['pages']} ({stats['cols']}x{stats['rows']})")
            print(f"Размер PGB: {stats['file_size']} байт")
            print(f"CRC-32: 0x{stats['crc32']:08X}")
            print("Формат и CRC соответствуют спецификации PGB 1.0: OK")
            _print_page_preview(extract_all_pages_text(raw_pgb))
            return 0

        output_path = args.output if args.output is not None else _default_output(args.input)
        hyphenator = make_hyphenator(args.hyphenate)

        doc = read_document(args.input, args.encoding, args.reflow)
        lines = blocks_to_lines(doc.blocks, args.cols, args.indent, args.collapse_blank, hyphenator)
        pages = paginate(lines, args.rows, args.cols)

        notes = list(doc.notes)
        title = args.title if args.title is not None else (doc.title or _default_title(args.input))
        author = args.author if args.author is not None else doc.author
        title_bytes = encode_meta(title, MAX_TITLE_BYTES, "Название", notes)
        author_bytes = encode_meta(author, MAX_AUTHOR_BYTES, "Автор", notes)

        pgb_data = build_pgb(pages, title_bytes, author_bytes, args.cols, args.rows)
        stats = verify_pgb(pgb_data)
        try:
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_bytes(pgb_data)
        except OSError as exc:
            raise ValueError(f"Не удалось записать {output_path}: {exc}") from exc

        for note in notes:
            print(note)
        if args.verify:
            written = output_path.read_bytes()
            stats = verify_pgb(written)
            print(f"Создан и проверен файл: {output_path}")
        else:
            print(f"Создан файл: {output_path}")
        print(f"Название: {stats['title']}")
        if stats["author"]:
            print(f"Автор: {stats['author']}")
        print(f"Страниц: {stats['pages']}")
        print(f"Строк текста: {len(lines)}")
        print(f"Текст: {stats['text_size']} байт")
        print(f"Размер PGB: {stats['file_size']} байт")
        print(f"CRC-32: 0x{stats['crc32']:08X}")
        print("Сборка и проверка структуры выполнены успешно.")
        if args.verify:
            _print_page_preview(extract_all_pages_text(written))
        return 0

    except (ValueError, RuntimeError, OSError) as exc:
        print(f"Ошибка: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
