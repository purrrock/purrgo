#!/usr/bin/env python3
"""Тесты конвертера книг PGB (tools/book-compiler).

Запуск из корня репозитория:  python3 -m unittest tests/test_book_compiler.py
"""
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
            self.assertLessEqual(len(lines), 32)
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
        self.assertEqual((stats["title"], stats["author"], stats["cols"], stats["rows"]), ("T", "A", 29, 32))

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
                self.assertGreater(len(app.pages_text), 1)
                self.assertIn("Строка номер 0", app.text.get("1.0", "end"))
                app.show_page(1)
                self.assertEqual(app.v_page.get(), "2")
                pages_before = len(app.pages_text)
                app.v_compression.set("none")
                wait()
                self.assertEqual(app.result.stats["compression"], bc.COMPRESSION_NONE)
                self.assertEqual(len(app.pages_text), pages_before)
                app.v_output.set(str(Path(tmp) / "out.pgb"))
                app.save()
                bc.verify_pgb((Path(tmp) / "out.pgb").read_bytes())
                app.load_pgb(Path(tmp) / "out.pgb")
                self.assertTrue(app.view_mode)
            finally:
                app.root.destroy()


if __name__ == "__main__":
    unittest.main()
