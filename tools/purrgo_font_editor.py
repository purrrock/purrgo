#!/usr/bin/env python3
"""
PurrGO 5x7 bitmap font editor.

Edits the exact format used by PurrGO:

    const unsigned char font5x7[256][5] = {
        ...
    };

Each glyph consists of 5 bytes. Each byte is one vertical column.
Only bits 0..6 are used, so a glyph is exactly 5x7 pixels.

No external Python packages are required; the GUI uses Tkinter.
"""

from __future__ import annotations

import re
import tkinter as tk
from tkinter import filedialog, messagebox
from pathlib import Path


FONT_COUNT = 256
GLYPH_WIDTH = 5
GLYPH_HEIGHT = 7
DEFAULT_C_FILE = "font5x7.c"
DEFAULT_H_FILE = "font5x7.h"


def parse_font_c(text: str) -> list[list[int]]:
    """Extract exactly 256 * 5 byte values from font5x7[256][5]."""
    match = re.search(
        r"font5x7\s*\[\s*256\s*\]\s*\[\s*5\s*\]\s*=\s*\{(.*?)\};",
        text,
        re.DOTALL,
    )
    if not match:
        raise ValueError("Не найден массив font5x7[256][5].")

    body = match.group(1)

    # Remove // comments and /* ... */ comments before parsing numbers.
    body = re.sub(r"//.*?$", "", body, flags=re.MULTILINE)
    body = re.sub(r"/\*.*?\*/", "", body, flags=re.DOTALL)

    tokens = re.findall(r"0[xX][0-9A-Fa-f]+|\b\d+\b", body)
    values = [int(token, 0) for token in tokens]

    expected = FONT_COUNT * GLYPH_WIDTH
    if len(values) != expected:
        raise ValueError(
            f"Найдено {len(values)} байт, ожидалось {expected}. "
            "Формат файла отличается от ожидаемого."
        )

    if any(value < 0 or value > 0xFF for value in values):
        raise ValueError("Обнаружено значение вне диапазона 0..255.")

    return [
        values[i * GLYPH_WIDTH : (i + 1) * GLYPH_WIDTH]
        for i in range(FONT_COUNT)
    ]


def load_font_file(path: Path) -> list[list[int]]:
    return parse_font_c(path.read_text(encoding="utf-8"))


def format_c(font: list[list[int]]) -> str:
    """Generate the same basic C representation used by PurrGO."""
    lines = [
        '#include "purrgo/font5x7.h"',
        "",
        "const unsigned char font5x7[256][5] = {",
    ]

    for code in range(FONT_COUNT):
        values = font[code]
        lines.append(
            "    { " + ", ".join(f"0x{value:02X}" for value in values) + " },"
        )

    lines.append("};")
    lines.append("")
    return "\n".join(lines)


def format_h() -> str:
    return """#ifndef PURRGO_FONT5X7_H
#define PURRGO_FONT5X7_H

extern const unsigned char font5x7[256][5];

#endif // PURRGO_FONT5X7_H
"""


class FontEditor(tk.Tk):
    def __init__(self) -> None:
        super().__init__()

        self.title("PurrGO 5x7 Font Editor")
        self.geometry("1050x650")
        self.minsize(900, 560)

        self.font = self.blank_font()
        self.current_code = 0
        self.current_path: Path | None = None
        self.modified = False

        self.pixel_size = 55

        self._build_ui()
        self._select_code(0)

    @staticmethod
    def blank_font() -> list[list[int]]:
        return [[0] * GLYPH_WIDTH for _ in range(FONT_COUNT)]

    @staticmethod
    def code_to_char(code: int) -> str:
        if 32 <= code <= 126:
            return chr(code)
        return "·"

    @staticmethod
    def glyph_to_rows(glyph: list[int]) -> list[list[int]]:
        """
        Convert PurrGO's column bytes into a 5x7 pixel matrix.

        Bit 0 is the top pixel, bit 6 is the bottom pixel.
        """
        return [
            [
                1 if (glyph[x] & (1 << y)) else 0
                for x in range(GLYPH_WIDTH)
            ]
            for y in range(GLYPH_HEIGHT)
        ]

    @staticmethod
    def rows_to_glyph(rows: list[list[int]]) -> list[int]:
        glyph = [0] * GLYPH_WIDTH
        for x in range(GLYPH_WIDTH):
            for y in range(GLYPH_HEIGHT):
                if rows[y][x]:
                    glyph[x] |= 1 << y
        return glyph

    def _build_ui(self) -> None:
        top = tk.Frame(self)
        top.pack(fill="x", padx=10, pady=10)

        tk.Button(top, text="Открыть .c", command=self.open_file).pack(
            side="left", padx=3
        )
        tk.Button(top, text="Сохранить .c", command=self.save_c).pack(
            side="left", padx=3
        )
        tk.Button(top, text="Сохранить .h", command=self.save_h).pack(
            side="left", padx=3
        )
        tk.Button(top, text="Очистить glyph", command=self.clear_glyph).pack(
            side="left", padx=15
        )

        self.path_label = tk.Label(top, text="Новый шрифт")
        self.path_label.pack(side="left", padx=10)

        main = tk.Frame(self)
        main.pack(fill="both", expand=True, padx=10, pady=(0, 10))

        # Character selector.
        left = tk.Frame(main, width=400)
        left.pack(side="left", fill="y")
        left.pack_propagate(False)

        tk.Label(
            left,
            text="Символы / коды",
            font=("TkDefaultFont", 11, "bold"),
        ).pack(anchor="w")

        list_frame = tk.Frame(left)
        list_frame.pack(fill="both", expand=True, pady=5)

        self.char_list = tk.Listbox(
            list_frame,
            width=42,
            font=("Consolas", 11),
            exportselection=False,
        )
        scrollbar = tk.Scrollbar(
            list_frame, orient="vertical", command=self.char_list.yview
        )
        self.char_list.configure(yscrollcommand=scrollbar.set)

        self.char_list.pack(side="left", fill="both", expand=True)
        scrollbar.pack(side="right", fill="y")

        for code in range(FONT_COUNT):
            self.char_list.insert(
                "end",
                f"0x{code:02X}  {code:3d}  {self.code_to_char(code)}",
            )

        self.char_list.bind("<<ListboxSelect>>", self._on_list_select)

        # Editor area.
        right = tk.Frame(main)
        right.pack(side="left", fill="both", expand=True, padx=(20, 0))

        info = tk.Frame(right)
        info.pack(fill="x")

        self.code_label = tk.Label(
            info, text="", font=("TkDefaultFont", 14, "bold")
        )
        self.code_label.pack(side="left")

        self.hex_label = tk.Label(info, text="", font=("Consolas", 12))
        self.hex_label.pack(side="left", padx=20)

        self.canvas = tk.Canvas(
            right,
            width=GLYPH_WIDTH * self.pixel_size + 1,
            height=GLYPH_HEIGHT * self.pixel_size + 1,
            background="white",
            highlightthickness=1,
            highlightbackground="black",
        )
        self.canvas.pack(pady=25)

        self.canvas.bind("<Button-1>", self._canvas_click)
        self.canvas.bind("<B1-Motion>", self._canvas_drag)

        self.status = tk.Label(right, anchor="w")
        self.status.pack(fill="x")

        tk.Label(
            right,
            text=(
                "ЛКМ — переключить пиксель. Можно рисовать с зажатой кнопкой. "
                "Формат: 5 колонок × 7 строк, bit 0 сверху."
            ),
            anchor="w",
            justify="left",
        ).pack(fill="x", pady=10)

    def _select_code(self, code: int) -> None:
        self.current_code = code

        self.char_list.selection_clear(0, "end")
        self.char_list.selection_set(code)
        self.char_list.see(code)

        self._update_info()
        self._draw()

    def _on_list_select(self, _event=None) -> None:
        selection = self.char_list.curselection()
        if not selection:
            return
        self._select_code(selection[0])

    def _update_info(self) -> None:
        code = self.current_code
        glyph = self.font[code]

        char = self.code_to_char(code)
        self.code_label.config(
            text=f"0x{code:02X} ({code})   «{char}»"
        )
        self.hex_label.config(
            text=" ".join(f"{value:02X}" for value in glyph)
        )

        self.status.config(
            text=("Изменён" if self.modified else "Без изменений")
        )

    def _draw(self) -> None:
        self.canvas.delete("all")

        glyph = self.font[self.current_code]

        for y in range(GLYPH_HEIGHT):
            for x in range(GLYPH_WIDTH):
                on = bool(glyph[x] & (1 << y))

                x0 = x * self.pixel_size
                y0 = y * self.pixel_size
                x1 = x0 + self.pixel_size
                y1 = y0 + self.pixel_size

                self.canvas.create_rectangle(
                    x0,
                    y0,
                    x1,
                    y1,
                    fill="black" if on else "white",
                    outline="gray",
                )

        # Mark the unused 8th bit conceptually: not drawn because the format
        # deliberately uses only bits 0..6.
        self.canvas.create_text(
            self.pixel_size * GLYPH_WIDTH - 5,
            self.pixel_size * GLYPH_HEIGHT - 5,
            text="5×7",
            anchor="se",
            fill="gray",
        )

    def _pixel_from_event(self, event) -> tuple[int, int] | None:
        x = event.x // self.pixel_size
        y = event.y // self.pixel_size

        if not (0 <= x < GLYPH_WIDTH and 0 <= y < GLYPH_HEIGHT):
            return None

        return x, y

    def _canvas_click(self, event) -> None:
        pixel = self._pixel_from_event(event)
        if pixel is None:
            return

        x, y = pixel
        self.font[self.current_code][x] ^= 1 << y
        self.modified = True
        self._update_info()
        self._draw()

    def _canvas_drag(self, event) -> None:
        pixel = self._pixel_from_event(event)
        if pixel is None:
            return

        x, y = pixel

        # During drag, set pixels rather than toggling them. This avoids
        # flickering/alternating pixels when the mouse moves over a cell twice.
        self.font[self.current_code][x] |= 1 << y
        self.modified = True
        self._update_info()
        self._draw()

    def clear_glyph(self) -> None:
        if not messagebox.askyesno(
            "Очистить glyph",
            f"Очистить символ 0x{self.current_code:02X}?",
        ):
            return

        self.font[self.current_code] = [0] * GLYPH_WIDTH
        self.modified = True
        self._update_info()
        self._draw()

    def open_file(self) -> None:
        path = filedialog.askopenfilename(
            title="Открыть PurrGO font5x7.c",
            filetypes=[
                ("C source", "*.c"),
                ("All files", "*.*"),
            ],
        )
        if not path:
            return

        try:
            loaded = load_font_file(Path(path))
        except Exception as exc:
            messagebox.showerror(
                "Ошибка",
                f"Не удалось прочитать шрифт:\n\n{exc}",
            )
            return

        self.font = loaded
        self.current_path = Path(path)
        self.modified = False
        self.path_label.config(text=str(self.current_path))
        self._update_info()
        self._draw()

    def save_c(self) -> None:
        initial = (
            self.current_path
            if self.current_path
            else Path(DEFAULT_C_FILE)
        )

        path = filedialog.asksaveasfilename(
            title="Сохранить PurrGO font5x7.c",
            initialfile=initial.name,
            defaultextension=".c",
            filetypes=[("C source", "*.c"), ("All files", "*.*")],
        )
        if not path:
            return

        try:
            Path(path).write_text(
                format_c(self.font),
                encoding="utf-8",
                newline="\n",
            )
        except OSError as exc:
            messagebox.showerror(
                "Ошибка",
                f"Не удалось сохранить файл:\n\n{exc}",
            )
            return

        self.current_path = Path(path)
        self.modified = False
        self.path_label.config(text=str(self.current_path))
        self._update_info()

    def save_h(self) -> None:
        path = filedialog.asksaveasfilename(
            title="Сохранить PurrGO font5x7.h",
            initialfile=DEFAULT_H_FILE,
            defaultextension=".h",
            filetypes=[("C header", "*.h"), ("All files", "*.*")],
        )
        if not path:
            return

        try:
            Path(path).write_text(
                format_h(),
                encoding="utf-8",
                newline="\n",
            )
        except OSError as exc:
            messagebox.showerror(
                "Ошибка",
                f"Не удалось сохранить файл:\n\n{exc}",
            )
            return


if __name__ == "__main__":
    app = FontEditor()
    app.mainloop()
