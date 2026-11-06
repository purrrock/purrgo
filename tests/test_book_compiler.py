#!/usr/bin/env python3
"""Тесты конвертера книг PGB (tools/book-compiler).

Запуск из корня репозитория:  python3 -m unittest tests/test_book_compiler.py
"""
import base64
import contextlib
import io
import struct
import sys
import tempfile
import unittest
import zipfile
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools" / "book-compiler"))

import purrgo_book_compiler as bc  # noqa: E402


def compile_text(text: str, **kwargs) -> bytes:
    reflow = kwargs.pop("reflow", "off")
    blocks, _ = bc.text_to_blocks(text, reflow)
    lines = bc.blocks_to_lines(blocks, **kwargs)
    return bc.build_pgb(bc.paginate(lines), b"T", b"A")


def to_pgo_text(text: str) -> str:
    return bc.pgo_bytes_to_text(bytes(bc.unicode_to_pgo(ch) for ch in text))


def page_texts(data: bytes) -> list[str]:
    bc.verify_pgb(data)
    return bc.extract_all_pages_text(data)


class Normalization(unittest.TestCase):
    def test_dashes_ellipsis_and_minus(self):
        self.assertEqual(to_pgo_text(bc.normalize_text("— а – б − в… г")), "- а - б - в... г")

    def test_combining_marks_and_soft_hyphen(self):
        self.assertEqual(bc.normalize_text("И\u0306"), "Й")
        self.assertEqual(bc.normalize_text("за\u0301мок"), "замок")
        self.assertEqual(bc.normalize_text("сло\u00adво"), "слово")
        self.assertEqual(bc.normalize_text("a\u200bb\ufeffc"), "abc")

    def test_latin_diacritics_fallback(self):
        self.assertEqual(bc.normalize_text("é ï ﬁ ²"), "e i fi 2")

    def test_unknown_becomes_question_mark(self):
        self.assertEqual(bc.normalize_text("5 €"), "5 ?")

    def test_every_output_char_is_single_byte(self):
        sample = "".join(chr(c) for c in range(0x20, 0x3000))
        for ch in bc.normalize_text(sample):
            self.assertTrue(ord(ch) < 0x80 or ch in bc.PGO_CHAR_MAP or ch == "\n", hex(ord(ch)))


class Wrapping(unittest.TestCase):
    def test_wrap_at_hard_hyphen(self):
        lines = bc.wrap_text("Слово-перенос длинное-длинное-длинное-предлинное-слово", 29)
        self.assertTrue(all(len(l) <= 29 for l in lines))
        self.assertEqual(lines[0], "Слово-перенос длинное-")
        self.assertEqual("".join(lines).replace(" ", ""), "Слово-переносдлинное-длинное-длинное-предлинное-слово")

    def test_overlong_word_is_split(self):
        lines = bc.wrap_text("x" * 70, 29)
        self.assertEqual([len(l) for l in lines], [29, 29, 12])

    def test_soft_hyphen_pieces(self):
        pieces = [("про", "soft"), ("грам", "soft"), ("ми", "soft"), ("ро", "soft"), ("ва", "soft"), ("ние", " ")]
        self.assertEqual(bc.wrap_pieces(pieces, 29), ["программирование"])
        lines = bc.wrap_pieces(pieces, 8)
        self.assertTrue(all(len(l) <= 8 for l in lines))
        self.assertEqual("".join(lines).replace("-", ""), "программирование")
        self.assertTrue(lines[0].endswith("-"))

    def test_indent(self):
        lines = bc.wrap_text("один два три", 29, indent=2)
        self.assertEqual(lines, ["  один два три"])


def sample_pages() -> list[bytes]:
    text = "\n".join(f"Это строка номер {i} с повторяющимся текстом и словами {i % 7}." for i in range(400))
    blocks, _ = bc.text_to_blocks(text, "off")
    return bc.paginate(bc.blocks_to_lines(blocks, indent=2))


class Lz4(unittest.TestCase):
    SAMPLES = [b"a", b"abc" * 3, b"x" * 12, b"x" * 13, b"x" * 200, bytes(range(256)) * 3,
               bytes((i * 7919 + 13) % 251 for i in range(959))]

    def test_builtin_encoder_roundtrip(self):
        for sample in self.SAMPLES + sample_pages():
            packed = bc.lz4_compress_block_py(sample)
            self.assertEqual(bc.lz4_decompress_block(packed, len(sample)), sample)

    def test_builtin_encoder_is_valid_for_lz4_library(self):
        try:
            import lz4.block as lib
        except ImportError:
            self.skipTest("пакет lz4 не установлен")
        for sample in self.SAMPLES + sample_pages()[:50]:
            self.assertEqual(lib.decompress(bc.lz4_compress_block_py(sample), uncompressed_size=len(sample)), sample)
            self.assertEqual(bc.lz4_decompress_block(bc.lz4_compress_block(sample), len(sample)), sample)

    def test_decoder_rejects_malformed(self):
        good = bc.lz4_compress_block_py(b"abcabcabcabcabcabcabcabc" * 3)
        raw = len(b"abcabcabcabcabcabcabcabc" * 3)
        self.assertEqual(len(bc.lz4_decompress_block(good, raw)), raw)
        for bad, size in ((b"", 5), (b"\x50hel", 5), (b"\x50hello", 4), (b"\x50hello", 6),
                          (b"\x50hello\x00", 5), (b"\x11a\x00\x00\x00", 6), (b"\x11a\x02\x00\x00", 6),
                          (b"\x15a\x01\x00", 10), (b"\xf0\xff", 300), (good[:-1], raw), (good, raw + 1)):
            with self.assertRaises(ValueError, msg=repr(bad)):
                bc.lz4_decompress_block(bad, size)

    def test_incompressible_page_is_stored_raw(self):
        state, noise = 12345, bytearray()
        for _ in range(959):
            state = (state * 1664525 + 1013904223) & 0xFFFFFFFF
            noise.append(state >> 24)
        noise = bytes(noise)
        self.assertEqual(bc.compress_page(noise, bc.COMPRESSION_LZ4), noise)
        text = b"abc " * 100
        self.assertLess(len(bc.compress_page(text, bc.COMPRESSION_LZ4)), len(text))
        self.assertEqual(bc.compress_page(text, bc.COMPRESSION_NONE), text)

    def test_container_with_both_modes(self):
        pages = sample_pages()
        for mode in (bc.COMPRESSION_NONE, bc.COMPRESSION_LZ4):
            data = bc.build_pgb(pages, b"T", b"A", compression=mode)
            stats = bc.verify_pgb(data)
            self.assertEqual(stats["compression"], mode)
            self.assertEqual(stats["text_size"], sum(map(len, pages)))
            self.assertEqual(bc.extract_all_pages_text(data), [bc.pgo_bytes_to_text(p) for p in pages])
        packed = bc.build_pgb(pages, b"T", b"A", compression=bc.COMPRESSION_LZ4)
        plain = bc.build_pgb(pages, b"T", b"A", compression=bc.COMPRESSION_NONE)
        self.assertLess(len(packed), len(plain))

    def test_corrupted_block_is_rejected_after_reseal(self):
        # Блок со смещением 0 при верных CRC: структуру LZ4 verify_pgb проверяет сам.
        page = b"abcabcabcabcabcabcabcabc" * 3
        data = bytearray(bc.build_pgb([page], b"T", b"A"))
        data_offset = struct.unpack_from("<I", data, 24)[0]
        literals = data[data_offset] >> 4
        data[data_offset + 1 + literals:data_offset + 3 + literals] = b"\x00\x00"
        struct.pack_into("<I", data, bc.META_CRC_OFFSET, bc.crc32_zeroed(data, data_offset))
        struct.pack_into("<I", data, bc.FILE_CRC_OFFSET, bc.crc32_zeroed(data))
        with self.assertRaises(ValueError):
            bc.verify_pgb(bytes(data))


class Hyphenation(unittest.TestCase):
    def test_pyphen_if_available(self):
        try:
            import pyphen  # noqa: F401
        except ImportError:
            self.skipTest("pyphen не установлен")
        hyphenator = bc.make_hyphenator("ru_RU")
        text = "программирование " * 12
        lines = bc.wrap_text(text, 29, 0, hyphenator)
        self.assertTrue(all(len(l) <= 29 for l in lines))
        self.assertEqual("".join(lines).replace("-", "").replace(" ", ""), text.replace(" ", ""))


class Pagination(unittest.TestCase):
    def test_many_blank_lines_do_not_fail(self):
        pages = page_texts(compile_text("a\n" + "\n" * 70 + "b\n"))
        self.assertEqual(pages, ["a", "b"])

    def test_page_limits(self):
        text = "\n".join("слово " * 6 for _ in range(100))
        for page in page_texts(compile_text(text)):
            lines = page.split("\n")
            self.assertLessEqual(len(lines), 25)
            self.assertTrue(all(len(l) <= 29 for l in lines))

    def test_empty_book_rejected(self):
        with self.assertRaises(ValueError):
            compile_text("\n\n  \n")


class Reflow(unittest.TestCase):
    PARAGRAPH = "\n".join(
        ["Это длинная строка книги, которая набрана с жёсткими переносами по ширине."] * 3
        + ["Короткий конец абзаца."]
    )
    WRAPPED = "\n\n".join([PARAGRAPH] * 8) + "\n"

    def test_auto_detects_hard_wrap(self):
        blocks, note = bc.text_to_blocks(self.WRAPPED, "auto")
        self.assertIn("склеиваются", note)
        self.assertEqual(sum(1 for b in blocks if b.kind == "p"), 8)

    def test_auto_keeps_single_line_paragraphs(self):
        text = "\n".join("Абзац " + "очень длинный " * 30 for _ in range(30))
        _, note = bc.text_to_blocks(text, "auto")
        self.assertIn("одна строка", note)


class Container(unittest.TestCase):
    def setUp(self):
        self.data = compile_text("Привет, мир!\nВторая строка.")

    def test_crc_vector(self):
        self.assertEqual(zlib.crc32(b"123456789") & 0xFFFFFFFF, 0xCBF43926)

    def test_verify_ok_and_metadata(self):
        stats = bc.verify_pgb(self.data)
        self.assertEqual((stats["title"], stats["author"], stats["cols"], stats["rows"]), ("T", "A", 29, 25))

    def test_meta_is_right_after_header(self):
        self.assertEqual(self.data[64:66], b"TA")

    def test_corruption_is_detected(self):
        for position in (0, 5, 12, 40, 44, len(self.data) - 1):
            broken = bytearray(self.data)
            broken[position] ^= 0x01
            with self.assertRaises(ValueError, msg=f"byte {position}"):
                bc.verify_pgb(bytes(broken))

    def test_truncated_file_rejected(self):
        with self.assertRaises(ValueError):
            bc.verify_pgb(self.data[:-1])

    def test_unknown_compression_and_flags_rejected(self):
        for offset, fmt, value in ((42, "B", 2), (10, "<H", 1)):
            broken = bytearray(self.data)
            struct.pack_into(fmt, broken, offset, value)
            struct.pack_into("<I", broken, bc.META_CRC_OFFSET, bc.crc32_zeroed(broken, struct.unpack_from("<I", broken, 24)[0]))
            struct.pack_into("<I", broken, bc.FILE_CRC_OFFSET, bc.crc32_zeroed(broken))
            with self.assertRaises(ValueError):
                bc.verify_pgb(bytes(broken))

    def test_title_and_author_truncated(self):
        notes: list[str] = []
        self.assertEqual(len(bc.encode_meta("я" * 500, bc.MAX_TITLE_BYTES, "Название", notes)), bc.MAX_TITLE_BYTES)
        self.assertTrue(notes)


FB2 = """<?xml version="1.0" encoding="windows-1251"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0">
<description><title-info><author><first-name>Иван</first-name><last-name>Петров</last-name></author>
<book-title>Тестовая книга</book-title></title-info></description>
<body><section><title><p>Глава 1</p></title><p>Первый абзац.</p><p>Второй абзац.</p>
<poem><stanza><v>Строка раз</v><v>Строка два</v></stanza></poem></section></body>
<body name="notes"><section><p>Сноска</p></section></body>
</FictionBook>""".encode("cp1251")


class Inputs(unittest.TestCase):
    def test_fb2(self):
        doc = bc.fb2_to_document(FB2)
        self.assertEqual((doc.title, doc.author), ("Тестовая книга", "Иван Петров"))
        lines = bc.blocks_to_lines(doc.blocks, indent=2)
        self.assertEqual(lines[0], "           Глава 1")
        self.assertEqual(lines[1], "")
        self.assertIn("  Первый абзац.", lines)
        self.assertIn("Строка раз", lines)
        self.assertNotIn("Сноска", "".join(lines))

    def test_epub(self):
        chapter = ('<?xml version="1.0" encoding="utf-8"?><html xmlns="http://www.w3.org/1999/xhtml">'
                   "<head><title>X</title><style>p{}</style></head><body><h1>Глава</h1>"
                   "<p>Первый&nbsp;абзац.</p><p>Строка<br/>после br</p></body></html>")
        opf = ('<?xml version="1.0"?><package xmlns="http://www.idpf.org/2007/opf" version="3.0">'
               '<metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:title>Книга</dc:title>'
               "<dc:creator>Автор А.</dc:creator></metadata><manifest>"
               '<item id="c1" href="text/c1.xhtml" media-type="application/xhtml+xml"/></manifest>'
               '<spine><itemref idref="c1"/></spine></package>')
        container = ('<?xml version="1.0"?><container xmlns="urn:oasis:names:tc:opendocument:xmlns:container">'
                     '<rootfiles><rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/>'
                     "</rootfiles></container>")
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "book.epub"
            with zipfile.ZipFile(path, "w") as z:
                z.writestr("META-INF/container.xml", container)
                z.writestr("OEBPS/content.opf", opf)
                z.writestr("OEBPS/text/c1.xhtml", chapter)
            doc = bc.epub_to_document(path)
        self.assertEqual((doc.title, doc.author), ("Книга", "Автор А."))
        lines = bc.blocks_to_lines(doc.blocks, indent=2)
        self.assertEqual([l.strip() for l in lines if l.strip()],
                         ["Глава", "Первый абзац.", "Строка", "после br"])

    def test_auto_encoding_falls_back_to_cp1251(self):
        text, used = bc.decode_source("Привет".encode("cp1251"), "auto")
        self.assertEqual(text, "Привет")
        self.assertIn("cp1251", used)

    def test_cli_compression_option(self):
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp) / "book.txt"
            src.write_text("\n".join(f"Строка номер {i} повторяется и повторяется." for i in range(300)), encoding="utf-8")
            sizes = {}
            for mode in ("none", "lz4"):
                out = Path(tmp) / f"{mode}.pgb"
                with contextlib.redirect_stdout(io.StringIO()):
                    self.assertEqual(bc.main([str(src), str(out), "--compression", mode]), 0)
                sizes[mode] = out.stat().st_size
            self.assertLess(sizes["lz4"], sizes["none"])

    def test_cli_roundtrip(self):
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp) / "book.txt"
            src.write_text("Привет, мир!\n", encoding="utf-8")
            out = Path(tmp) / "BOOK.PGB"
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(bc.main([str(src), str(out), "--verify", "--author", "Я"]), 0)
                self.assertEqual(bc.main([str(out)]), 0)


def _gradient_png(width: int, height: int, mode: str = "RGB") -> bytes:
    from PIL import Image
    img = Image.new(mode, (width, height))
    px = img.load()
    for y in range(height):
        for x in range(width):
            v = (x * 255) // max(1, width - 1)
            px[x, y] = v if mode == "L" else ((v, v, v, 255) if mode == "RGBA" else (v, v, v))
    buf = io.BytesIO()
    img.save(buf, "PNG")
    return buf.getvalue()


def _need_pillow(test: unittest.TestCase) -> None:
    if not bc.image_support_available():
        test.skipTest("Pillow не установлен")


class Images(unittest.TestCase):
    def setUp(self):
        _need_pillow(self)

    def test_fit_size(self):
        self.assertEqual(bc.fit_size(600, 900, 176, 256, False), (171, 256))
        self.assertEqual(bc.fit_size(1000, 200, 176, 256, False), (176, 35))
        self.assertEqual(bc.fit_size(100, 50, 176, 256, False), (100, 50))      # без увеличения
        self.assertEqual(bc.fit_size(100, 50, 176, 256, True), (176, 88))       # с увеличением
        w, h = bc.fit_size(1, 5000, 176, 256, False)
        self.assertTrue(1 <= w <= 176 and 1 <= h <= 256)

    def test_pack_mask_msb_first_and_zero_padding(self):
        mask = bytes([1, 0, 0, 0, 0, 0, 0, 1,  1, 1, 0, 0, 0, 0, 0, 0,  0, 0, 1, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
                      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1])
        bits = bc.pack_mask(mask[:2 * 12], 12, 2)
        self.assertEqual(len(bits), 4)
        self.assertEqual(bits[0], 0b10000001)
        self.assertEqual(bits[1], 0b11000000 & 0xF0)    # биты 8..11 строки, выравнивание нулевое
        page = bc.ImagePage(12, 2, bits)
        self.assertEqual([page.pixel(x, 0) for x in range(12)], list(mask[:12]))
        self.assertEqual(bits[1] & 0x0F, 0)

    def test_dithers_black_white_and_gray(self):
        white, black, gray = bytes([255]) * 400, bytes([0]) * 400, bytes([128]) * 400
        for mode, func in bc._DITHERS.items():
            self.assertEqual(sum(func(white, 20, 20)), 0, mode)
            self.assertEqual(sum(func(black, 20, 20)), 400, mode)
            if mode != "none":   # порог: ровно 128 - уже белый
                dark = sum(func(gray, 20, 20))
                self.assertTrue(120 <= dark <= 280, (mode, dark))       # серый - примерно половина чёрных точек

    def test_image_to_page_fits_window_and_is_deterministic(self):
        data = _gradient_png(600, 900)
        page = bc.image_to_page(data)
        self.assertEqual((page.width, page.height), (171, 256))
        again = bc.image_to_page(data, "fs")
        self.assertEqual(page.bits, again.bits)
        left = sum(page.pixel(x, y) for y in range(256) for x in range(20))
        right = sum(page.pixel(x, y) for y in range(256) for x in range(150, 171))
        self.assertGreater(left, right * 3)       # слева тёмный край градиента, справа светлый

    def test_small_image_skipped_and_transparency_is_white(self):
        with self.assertRaises(bc.ImageSkipped):
            bc.image_to_page(_gradient_png(30, 30))
        from PIL import Image
        img = Image.new("RGBA", (100, 100), (0, 0, 0, 0))     # полностью прозрачная
        buf = io.BytesIO()
        img.save(buf, "PNG")
        page = bc.image_to_page(buf.getvalue(), "none")
        self.assertEqual(sum(page.bits), 0)

    def test_bad_image_data(self):
        with self.assertRaises(ValueError):
            bc.image_to_page(b"not an image")

    def test_png_preview_is_valid(self):
        from PIL import Image
        page = bc.image_to_page(_gradient_png(200, 100))
        img = Image.open(io.BytesIO(page.to_png()))
        self.assertEqual(img.size, (page.width, page.height))
        self.assertEqual(img.mode, "1")
        for x, y in ((0, 0), (page.width - 1, page.height - 1), (page.width // 2, 5)):
            self.assertEqual(img.getpixel((x, y)) == 0, bool(page.pixel(x, y)))

    def test_paginate_gives_image_its_own_page(self):
        lines = ["текст"] * 30 + [bc.ImageMark("k")] + ["после"]
        pages = bc.paginate(lines)
        self.assertEqual([isinstance(p, bc.ImageMark) for p in pages], [False, False, True, False])
        self.assertEqual(pages[0].count(b"\n") + 1, 25)

    def test_build_and_verify_with_image_pages(self):
        img = bc.image_to_page(_gradient_png(300, 400))
        text = b"abc\ndef"
        for mode in (bc.COMPRESSION_NONE, bc.COMPRESSION_LZ4):
            data = bc.build_pgb([img, text, img], b"T", b"A", compression=mode)
            stats = bc.verify_pgb(data)
            self.assertEqual((stats["pages"], stats["image_pages"], stats["img_w"], stats["img_h"]), (3, 2, 176, 256))
            pages = bc.extract_all_pages(data)
            self.assertEqual(pages[0], img)
            self.assertEqual(pages[1], bc.pgo_bytes_to_text(text))
            self.assertEqual(bc.extract_all_pages_text(data)[0], f"[картинка {img.width}x{img.height}]")

    def test_verify_rejects_bad_image_entries(self):
        img = bc.image_to_page(_gradient_png(300, 400))
        good = bc.build_pgb([img, b"x"], b"T", b"A", compression=bc.COMPRESSION_NONE)
        index = struct.unpack_from("<I", good, 20)[0]

        def reseal(data: bytearray) -> bytes:
            data_offset = struct.unpack_from("<I", data, 24)[0]
            struct.pack_into("<I", data, bc.META_CRC_OFFSET, bc.crc32_zeroed(data, data_offset))
            struct.pack_into("<I", data, bc.FILE_CRC_OFFSET, bc.crc32_zeroed(data))
            return bytes(data)

        for field_offset, value in ((8, 0), (10, 0), (8, 177), (10, 257), (8, img.width - 8)):
            bad = bytearray(good)
            struct.pack_into("<H", bad, index + field_offset, value)
            with self.assertRaises(ValueError, msg=(field_offset, value)):
                bc.verify_pgb(reseal(bad))
        # ненулевой бит выравнивания строки
        odd = bc.ImagePage(13, 2, bytes([0xFF, 0xF8, 0xFF, 0xF8]))
        bc.verify_pgb(bc.build_pgb([odd], b"T", b"A"))
        with self.assertRaises(ValueError):
            bc.verify_pgb(bc.build_pgb([bc.ImagePage(13, 2, bytes([0xFF, 0xF8, 0xFF, 0xF9]))], b"T", b"A"))

    def test_fb2_with_cover_and_illustrations(self):
        big, small = _gradient_png(400, 300), _gradient_png(20, 20)
        b64 = lambda raw: base64.b64encode(raw).decode()
        fb2 = ('<?xml version="1.0" encoding="utf-8"?><FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0" '
               'xmlns:l="http://www.w3.org/1999/xlink"><description><title-info><book-title>К</book-title>'
               '<coverpage><image l:href="#c"/></coverpage></title-info></description><body><section>'
               '<p>Один.</p><image l:href="#i"/><p>Два <image l:href="#s"/> текст.</p><image l:href="#c"/></section></body>'
               f'<binary id="c" content-type="image/png">{b64(big)}</binary>'
               f'<binary id="i" content-type="image/png">{b64(big)}</binary>'
               f'<binary id="s" content-type="image/png">{b64(small)}</binary></FictionBook>').encode()
        doc = bc.fb2_to_document(fb2)
        self.assertEqual(doc.cover, "c")
        self.assertEqual([b.text for b in doc.blocks if b.kind == "i"], ["i", "s", "c"])
        result = bc.render_document(doc, Path("x.fb2"), bc.Options())
        kinds = ["img" if isinstance(p, bc.ImagePage) else "txt" for p in result.pages]
        self.assertEqual(kinds, ["img", "txt", "img", "txt", "img"])       # обложка, текст, картинка, текст, картинка; маленькая пропущена
        self.assertTrue(any("маленьких" in n for n in result.notes))
        no_img = bc.render_document(doc, Path("x.fb2"), bc.Options(images=False))
        self.assertEqual(["img" if isinstance(p, bc.ImagePage) else "txt" for p in no_img.pages], ["img", "txt"])
        none = bc.render_document(doc, Path("x.fb2"), bc.Options(images=False, cover=False))
        self.assertEqual(len(none.pages), 1)

    def test_epub_cover_dedup_and_illustration(self):
        big = _gradient_png(500, 700)
        opf = ('<?xml version="1.0"?><package xmlns="http://www.idpf.org/2007/opf" version="2.0"><metadata '
               'xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:title>Книга</dc:title><meta name="cover" content="cov"/></metadata>'
               '<manifest><item id="cov" href="img/cover.png" media-type="image/png"/>'
               '<item id="c0" href="cover.xhtml" media-type="application/xhtml+xml"/>'
               '<item id="c1" href="text/c1.xhtml" media-type="application/xhtml+xml"/></manifest>'
               '<spine><itemref idref="c0"/><itemref idref="c1"/></spine></package>')
        container = ('<?xml version="1.0"?><container xmlns="urn:oasis:names:tc:opendocument:xmlns:container">'
                     '<rootfiles><rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/></rootfiles></container>')
        cover_page = ('<html xmlns="http://www.w3.org/1999/xhtml"><body><svg xmlns="http://www.w3.org/2000/svg" '
                      'xmlns:xlink="http://www.w3.org/1999/xlink"><image xlink:href="img/cover.png"/></svg></body></html>')
        chapter = ('<html xmlns="http://www.w3.org/1999/xhtml"><body><p>Текст.</p><div><img src="../img/ill.png"/></div>'
                   '<p>Ещё.</p><img src="../img/missing.png"/></body></html>')
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "book.epub"
            with zipfile.ZipFile(path, "w") as z:
                z.writestr("META-INF/container.xml", container)
                z.writestr("OEBPS/content.opf", opf)
                z.writestr("OEBPS/cover.xhtml", cover_page)
                z.writestr("OEBPS/text/c1.xhtml", chapter)
                z.writestr("OEBPS/img/cover.png", big)
                z.writestr("OEBPS/img/ill.png", big)
            doc = bc.epub_to_document(path)
        self.assertEqual(doc.cover, "OEBPS/img/cover.png")
        self.assertEqual([b.text for b in doc.blocks if b.kind == "i"], ["OEBPS/img/ill.png"])   # дубль обложки убран
        result = bc.render_document(doc, path, bc.Options())
        self.assertEqual(["img" if isinstance(p, bc.ImagePage) else "txt" for p in result.pages],
                         ["img", "txt", "img", "txt"])

    def test_custom_cover_file_and_cli(self):
        with tempfile.TemporaryDirectory() as tmp:
            cover = Path(tmp) / "cover.png"
            cover.write_bytes(_gradient_png(300, 300))
            src = Path(tmp) / "book.txt"
            src.write_text("Привет, мир!\n", encoding="utf-8")
            out = Path(tmp) / "BOOK.PGB"
            shots = Path(tmp) / "shots"
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(bc.main([str(src), str(out), "--cover", str(cover), "--dither", "atkinson",
                                          "--save-images", str(shots)]), 0)
            stats = bc.verify_pgb(out.read_bytes())
            self.assertEqual((stats["pages"], stats["image_pages"]), (2, 1))
            self.assertEqual([p.name for p in shots.iterdir()], ["page_0001.png"])
            self.assertEqual(bc.main([str(src), str(out), "--cover", str(Path(tmp) / "nope.png")]), 2)

    def test_without_pillow_images_are_skipped(self):
        doc = bc.Document(blocks=[bc.Block("p", "Текст"), bc.Block("i", "k")], images={"k": _gradient_png(200, 200)},
                          cover="k")
        saved = bc._PilImage
        bc._PilImage = None
        try:
            result = bc.render_document(doc, Path("x.fb2"), bc.Options())
        finally:
            bc._PilImage = saved
        self.assertEqual(len(result.pages), 1)
        self.assertTrue(any("Pillow" in n for n in result.notes))

    def test_gui_shows_image_pages(self):
        if bc.tk is None:
            self.skipTest("tkinter не установлен")
        with tempfile.TemporaryDirectory() as tmp:
            cover = Path(tmp) / "cover.png"
            cover.write_bytes(_gradient_png(300, 400))
            src = Path(tmp) / "book.txt"
            src.write_text("Текст книги\n", encoding="utf-8")
            try:
                app = bc.BookCompilerGui(src, bc.Options(cover_file=cover))
            except bc.tk.TclError:
                self.skipTest("нет дисплея")
            try:
                import time
                end = time.time() + 30
                while (app.busy or app.pending or app.rebuild_job or app.result is None) and time.time() < end:
                    app.root.update()
                    time.sleep(0.01)
                self.assertIsInstance(app.pages[0], bc.ImagePage)
                app.show_page(0)
                self.assertTrue(app.image_label.winfo_ismapped() or app.image_label.winfo_manager() == "grid")
                self.assertEqual(app.text.winfo_manager(), "")
                app.show_page(1)
                self.assertEqual(app.text.winfo_manager(), "grid")
                self.assertIn("Текст книги", app.text.get("1.0", "end"))
                app.v_dither.set("atkinson")
                app.clear_cover()
                end = time.time() + 30
                while (app.busy or app.pending or app.rebuild_job) and time.time() < end:
                    app.root.update()
                    time.sleep(0.01)
                self.assertEqual(len(app.pages), 1)
            finally:
                app.root.destroy()


class Gui(unittest.TestCase):
    """Проверяется только при наличии tkinter и дисплея (в CI без экрана пропускается)."""

    def test_gui_builds_preview_and_saves(self):
        if bc.tk is None:
            self.skipTest("tkinter не установлен")
        import time
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp) / "book.txt"
            src.write_text("\n".join(f"Строка номер {i} повторяется и повторяется." for i in range(200)), encoding="utf-8")
            try:
                app = bc.BookCompilerGui(src)
            except bc.tk.TclError:
                self.skipTest("нет дисплея")
            try:
                def wait():
                    end = time.time() + 30
                    while (app.busy or app.pending or app.rebuild_job) and time.time() < end:
                        app.root.update()
                        time.sleep(0.01)
                    app.root.update()
                wait()
                self.assertIsNotNone(app.result)
                self.assertGreater(len(app.pages), 1)
                self.assertIn("Строка номер 0", app.text.get("1.0", "end"))
                app.show_page(1)
                self.assertEqual(app.v_page.get(), "2")
                pages_before = len(app.pages)
                app.v_compression.set("none")
                wait()
                self.assertEqual(app.result.stats["compression"], bc.COMPRESSION_NONE)
                self.assertEqual(len(app.pages), pages_before)
                app.v_output.set(str(Path(tmp) / "out.pgb"))
                app.save()
                bc.verify_pgb((Path(tmp) / "out.pgb").read_bytes())
                app.load_pgb(Path(tmp) / "out.pgb")
                self.assertTrue(app.view_mode)
            finally:
                app.root.destroy()


if __name__ == "__main__":
    unittest.main()
