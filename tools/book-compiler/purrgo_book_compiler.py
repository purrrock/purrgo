#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Конвертер книг (txt / FB2 / EPUB) в бинарный формат электронных книг PGB 1.0 для PurrGO.

Запуск без параметров открывает графический интерфейс (tkinter) с предпросмотром страниц;
с параметрами работает в командной строке.

Зависимости: стандартная библиотека Python 3.10+ (для GUI в Linux: python3-tk).
Необязательно:
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
COMPRESSION_LZ4 = 1      # сырой LZ4 Block на каждую страницу (не LZ4 Frame)

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
                    blank_between_paragraphs: bool = False,
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
            if blank_between_paragraphs:
                blank_pending = True

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


def build_pgb(pages: list[bytes], title: bytes, author: bytes,
              cols: int = DEFAULT_COLS, rows: int = DEFAULT_ROWS,
              compression: int = COMPRESSION_LZ4) -> bytes:
    if not pages:
        raise ValueError("В книге должна быть хотя бы одна страница.")
    if not (1 <= cols <= 255 and 1 <= rows <= 255):
        raise ValueError("cols и rows должны быть в диапазоне 1..255.")
    if compression not in (COMPRESSION_NONE, COMPRESSION_LZ4):
        raise ValueError(f"Неизвестный способ сжатия: {compression}.")
    if len(title) > MAX_TITLE_BYTES or len(author) > MAX_AUTHOR_BYTES:
        raise ValueError("Название или автор слишком длинные.")

    stored_pages = [compress_page(page, compression) for page in pages]
    page_count = len(pages)
    meta_offset = HEADER_SIZE
    index_offset = meta_offset + len(title) + len(author)
    data_offset = index_offset + page_count * INDEX_ENTRY_SIZE

    index = bytearray()
    offset = data_offset
    for raw, stored in zip(pages, stored_pages):
        index += INDEX_STRUCT.pack(offset, len(stored), len(raw))
        offset += len(stored)
        if offset > UINT32_MAX:
            raise ValueError("Размер файла превышает диапазон uint32.")
    file_size = offset

    header = HEADER_STRUCT.pack(
        MAGIC, VERSION_MAJOR, VERSION_MINOR, HEADER_SIZE, 0,
        page_count, meta_offset, index_offset, data_offset, file_size, 0, 0,
        cols, rows, compression, 0, len(title), len(author), b"\x00" * 16,
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
    if compression not in (COMPRESSION_NONE, COMPRESSION_LZ4):
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
    raw_total = 0
    stored_total = 0
    packed_pages = 0
    for number, (page_offset, stored, raw) in _iter_pages(data, page_count, index_offset):
        label = f"Страница {number + 1}"
        if stored == 0 or raw == 0:
            raise ValueError(f"{label}: нулевой размер.")
        if raw > limit:
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
        "file_size": file_size,
        "text_size": raw_total,
        "stored_size": stored_total,
        "packed_pages": packed_pages,
        "compression": compression,
        "crc32": file_crc,
        "cols": cols,
        "rows": rows,
        "title": pgo_bytes_to_text(data[title_start:author_start]),
        "author": pgo_bytes_to_text(data[author_start:index_offset]),
    }


def extract_all_pages_text(data: bytes) -> list[str]:
    fields = HEADER_STRUCT.unpack_from(data, 0)
    page_count, index_offset, compression = fields[5], fields[7], fields[14]
    pages = []
    for _number, (offset, stored, raw) in _iter_pages(data, page_count, index_offset):
        pages.append(pgo_bytes_to_text(decompress_page(data[offset:offset + stored], raw, compression)))
    return pages


def describe_compression(stats: dict) -> str:
    if stats["compression"] == COMPRESSION_NONE:
        return "нет"
    ratio = 100.0 * stats["stored_size"] / stats["text_size"]
    return f"LZ4 Block, данные страниц {ratio:.0f}% от текста ({stats['packed_pages']} из {stats['pages']} стр. сжаты)"


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


@dataclass
class BookResult:
    data: bytes
    pages: list[bytes]       # страницы в PurrGO-256 до сжатия
    lines: list[str]
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


def render_document(doc: Document, path: Path, opts: Options) -> BookResult:
    """Вторая стадия: переносы, страницы, сжатие, сборка и проверка PGB."""
    if opts.compression not in COMPRESSION_NAMES:
        raise ValueError(f"Неизвестный способ сжатия: {opts.compression!r} (lz4 или none).")
    hyphenator = make_hyphenator(opts.hyphenate or None)
    lines = blocks_to_lines(doc.blocks, opts.cols, opts.indent, opts.collapse_blank,
                            opts.blank_between_paragraphs, hyphenator)
    pages = paginate(lines, opts.rows, opts.cols)

    notes = list(doc.notes)
    title = opts.title if opts.title else (doc.title or _default_title(path))
    author = opts.author if opts.author is not None and opts.author != "" else doc.author
    title_bytes = encode_meta(title, MAX_TITLE_BYTES, "Название", notes)
    author_bytes = encode_meta(author, MAX_AUTHOR_BYTES, "Автор", notes)

    data = build_pgb(pages, title_bytes, author_bytes, opts.cols, opts.rows, COMPRESSION_NAMES[opts.compression])
    stats = verify_pgb(data)
    if opts.compression == "lz4" and not lz4_library_available():
        notes.append("Сжатие: встроенный кодер LZ4 (пакет lz4 не установлен, с ним файл получится меньше)")
    return BookResult(data=data, pages=pages, lines=lines, stats=stats, notes=notes)


def compile_book(path: Path, opts: Options) -> BookResult:
    return render_document(load_document(path, opts), path, opts)


# --------------------------------------------------------------------------
# Командная строка
# --------------------------------------------------------------------------
def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Конвертер txt / FB2 / EPUB в формат электронных книг PurrGO PGB 1.0 и проверка PGB. "
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
    print(f"Страниц: {stats['pages']} ({stats['cols']}x{stats['rows']})")
    print(f"Текст: {stats['text_size']} байт")
    print(f"Сжатие: {describe_compression(stats)}")
    print(f"Размер PGB: {stats['file_size']} байт")
    print(f"CRC-32: 0x{stats['crc32']:08X}")


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
            print("Формат и CRC соответствуют спецификации PGB 1.0: OK")
            _print_page_preview(extract_all_pages_text(raw_pgb))
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
        self.pages_text: list[str] = []
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

        # -- результат
        res = ttk.LabelFrame(left, text="Результат", padding=8)
        res.grid(row=3, column=0, sticky="ew", pady=(8, 0))
        res.columnconfigure(0, weight=1)
        self.info = ttk.Label(res, text="Выберите книгу", justify="left", wraplength=360)
        self.info.grid(row=0, column=0, sticky="w")
        self.progress = ttk.Progressbar(res, mode="indeterminate", length=200)
        self.notes = ttk.Label(res, text="", foreground="#555555", justify="left", wraplength=360)
        self.notes.grid(row=2, column=0, sticky="w", pady=(6, 0))

        buttons = ttk.Frame(left)
        buttons.grid(row=4, column=0, sticky="ew", pady=(10, 0))
        buttons.columnconfigure(0, weight=1)
        self.btn_save = ttk.Button(buttons, text="Сохранить PGB", command=self.save, state="disabled")
        self.btn_save.grid(row=0, column=0, sticky="ew")
        ttk.Button(buttons, text="Открыть PGB…", command=self.open_pgb).grid(row=0, column=1, padx=(6, 0))
        self.status = ttk.Label(left, text="", wraplength=380, justify="left")
        self.status.grid(row=5, column=0, sticky="w", pady=(8, 0))

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
        self.btn_last = ttk.Button(nav, text=">|", width=3, command=lambda: self.show_page(len(self.pages_text) - 1))
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
        self.slider = ttk.Scale(right, from_=1, to=1, orient="horizontal", command=self._on_slider)
        self.slider.grid(row=2, column=0, sticky="ew", pady=(8, 0))
        self._set_nav_state(False)

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
                    self.v_hyphen, self.v_compression):
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

    def _key_nav(self, delta: int | None, first: bool = False, last: bool = False) -> None:
        focus = self.root.focus_get()
        if isinstance(focus, (tk.Entry, ttk.Entry, tk.Spinbox, ttk.Spinbox, ttk.Combobox)):
            return
        if first:
            self.show_page(0)
        elif last:
            self.show_page(len(self.pages_text) - 1)
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
        return Options(
            encoding=self.v_encoding.get(), title=self.v_title.get().strip() or None,
            author=self.v_author.get().strip() or None, reflow=self.v_reflow.get(), indent=indent,
            collapse_blank=self.v_collapse.get(),
            blank_between_paragraphs=self.v_blank_par.get(), hyphenate=self.v_hyphen.get().strip() or None,
            compression=self.v_compression.get(),
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
            f"Страниц: {stats['pages']}   строк: {len(result.lines)}\n"
            f"Текст: {_format_size(stats['text_size'])}  →  PGB: {_format_size(stats['file_size'])} ({ratio:.0f}%)\n"
            f"Сжатие: {describe_compression(stats)}\n"
            f"Название: {stats['title']}\nАвтор: {stats['author'] or '-'}"))
        self.notes.config(text="\n".join(result.notes))
        hint = " / ".join(x for x in (doc.title, doc.author) if x)
        self.meta_hint.config(text=("В книге: " + hint) if hint else "Пусто - из метаданных книги или имя файла")
        self.status.config(text="Предпросмотр обновлён. Проверьте текст и сохраните.", foreground="")
        self._set_pages([pgo_bytes_to_text(page) for page in result.pages])
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
            pages = extract_all_pages_text(raw)
        except (OSError, ValueError) as exc:
            self.status.config(text=f"Не удалось открыть {path.name}: {exc}", foreground="#b00020")
            return
        self.view_mode = True
        self.result = None
        self.btn_save.state(["disabled"])
        self.info.config(text=(
            f"Просмотр PGB: {path.name}\n"
            f"Страниц: {stats['pages']} ({stats['cols']}x{stats['rows']})\n"
            f"Текст: {_format_size(stats['text_size'])}  →  PGB: {_format_size(stats['file_size'])}\n"
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

    def _set_pages(self, pages: list[str]) -> None:
        self.pages_text = pages
        total = len(pages)
        self.lbl_total.config(text=f"из {total}")
        self.entry_page.config(to=max(1, total))
        self.slider.config(to=max(1, total))
        self._set_nav_state(total > 0)
        keep = min(self.page_index, max(0, total - 1))
        self.show_page(keep)

    def show_page(self, index: int) -> None:
        total = len(self.pages_text)
        self.text.config(state="normal")
        self.text.delete("1.0", "end")
        if total == 0:
            self.page_index = 0
            self.v_page.set("1")
            self.text.config(state="disabled")
            return
        index = max(0, min(total - 1, index))
        self.page_index = index
        text = self.pages_text[index]
        if self.v_spaces.get():
            text = text.replace(" ", "·")
        self.text.insert("1.0", text)
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
        if self._slider_busy or not self.pages_text:
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
