#!/usr/bin/env python3
"""
PurrGO 7x7 POI Icon Editor

Simple Tkinter editor for 7x7 POI icons.

Pixel format:
    2 bits grayscale + 1 bit transparency
    gray: 0..3
    alpha: 0 transparent, 1 opaque

Each pixel is stored as one byte:
    bit 2 = alpha
    bits 1..0 = grayscale

Thus the editor's native icon data is:
    49 bytes per icon.

The generated C array uses one byte per pixel. This is deliberately simple
and avoids packing/unpacking 2-bit grayscale and transparency in firmware.

Icon selection is one byte:
    icon index 0..255

Generated data:
    purrgo_poi_icons[256][7][7]

Unused icon slots are transparent.
"""

import re
import tkinter as tk
from tkinter import filedialog, messagebox, simpledialog
from pathlib import Path

GRID = 7
MAX_ICONS = 256

# Pixel encoding:
#   0x00..0x03 = transparent, grayscale 0..3
#   0x04..0x07 = opaque,      grayscale 0..3
ALPHA_BIT = 0x04

GRAY_NAMES = {
    0: "black",
    1: "dark gray",
    2: "light gray",
    3: "white",
}


def blank_icon():
    """Create a completely transparent 7x7 icon."""
    return [[0 for _ in range(GRID)] for _ in range(GRID)]


class IconEditor:
    def __init__(self, root):
        self.root = root
        self.root.title("PurrGO 7x7 POI Icon Editor")

        self.icons = [blank_icon() for _ in range(MAX_ICONS)]
        self.current = 0

        # Current drawing state.
        self.gray = 0
        self.alpha = 1

        self.cell_size = 55
        self.margin = 10

        self._build_ui()
        self._draw_grid()
        self._update_info()

    # ------------------------------------------------------------------
    # UI
    # ------------------------------------------------------------------

    def _build_ui(self):
        main = tk.Frame(self.root)
        main.pack(padx=10, pady=10)

        left = tk.Frame(main)
        left.pack(side=tk.LEFT)

        right = tk.Frame(main)
        right.pack(side=tk.LEFT, padx=(15, 0), fill=tk.Y)

        canvas_size = self.margin * 2 + GRID * self.cell_size
        self.canvas = tk.Canvas(
            left,
            width=canvas_size,
            height=canvas_size,
            highlightthickness=1,
        )
        self.canvas.pack()

        self.canvas.bind("<Button-1>", self._paint_at_event)
        self.canvas.bind("<B1-Motion>", self._paint_at_event)

        # Icon number.
        tk.Label(right, text="Icon:").pack(anchor="w")

        nav = tk.Frame(right)
        nav.pack(fill=tk.X)

        tk.Button(nav, text="◀", width=3, command=self._previous).pack(side=tk.LEFT)
        self.icon_var = tk.IntVar(value=0)
        self.icon_spin = tk.Spinbox(
            nav,
            from_=0,
            to=MAX_ICONS - 1,
            width=5,
            textvariable=self.icon_var,
            command=self._spin_changed,
        )
        self.icon_spin.pack(side=tk.LEFT, padx=4)
        self.icon_spin.bind("<Return>", self._spin_changed_event)

        tk.Button(nav, text="▶", width=3, command=self._next).pack(side=tk.LEFT)

        tk.Button(
            right,
            text="Clear icon",
            command=self._clear_current,
        ).pack(fill=tk.X, pady=(10, 3))

        tk.Button(
            right,
            text="Copy current → next",
            command=self._copy_next,
        ).pack(fill=tk.X, pady=3)

        # Grayscale selector.
        tk.Label(right, text="Grayscale:").pack(anchor="w", pady=(15, 3))

        self.gray_var = tk.IntVar(value=0)
        for value in range(4):
            tk.Radiobutton(
                right,
                text=f"{value} — {GRAY_NAMES[value]}",
                variable=self.gray_var,
                value=value,
                command=self._tool_changed,
            ).pack(anchor="w")

        # Alpha selector.
        tk.Label(right, text="Pixel:").pack(anchor="w", pady=(15, 3))

        self.alpha_var = tk.IntVar(value=1)

        tk.Radiobutton(
            right,
            text="Opaque",
            variable=self.alpha_var,
            value=1,
            command=self._tool_changed,
        ).pack(anchor="w")

        tk.Radiobutton(
            right,
            text="Transparent",
            variable=self.alpha_var,
            value=0,
            command=self._tool_changed,
        ).pack(anchor="w")

        # File operations.
        tk.Label(right, text="File:").pack(anchor="w", pady=(15, 3))

        tk.Button(
            right,
            text="Load C array (.h)…",
            command=self._load_c,
        ).pack(fill=tk.X, pady=2)

        tk.Button(
            right,
            text="Save C array (.h)…",
            command=self._save_c,
        ).pack(fill=tk.X, pady=2)

        self.info_var = tk.StringVar()
        tk.Label(
            right,
            textvariable=self.info_var,
            justify=tk.LEFT,
            anchor="w",
        ).pack(anchor="w", pady=(15, 0))

        tk.Label(
            right,
            text=(
                "Mouse: draw pixels\n"
                "Right mouse: erase pixel\n"
                "Each icon: 7×7 = 49 bytes"
            ),
            justify=tk.LEFT,
        ).pack(anchor="w", pady=(15, 0))

        # Keyboard navigation.
        self.root.bind("<Left>", lambda e: self._previous())
        self.root.bind("<Right>", lambda e: self._next())

    # ------------------------------------------------------------------
    # Pixel encoding
    # ------------------------------------------------------------------

    @staticmethod
    def encode_pixel(gray, alpha):
        """
        Encode one pixel.

        The lower two bits contain grayscale 0..3.
        Bit 2 contains opacity.
        """
        return (gray & 0x03) | (ALPHA_BIT if alpha else 0)

    @staticmethod
    def decode_pixel(value):
        """Decode one stored pixel into (gray, alpha)."""
        return value & 0x03, 1 if (value & ALPHA_BIT) else 0

    # ------------------------------------------------------------------
    # Drawing
    # ------------------------------------------------------------------

    def _tool_changed(self):
        self.gray = self.gray_var.get()
        self.alpha = self.alpha_var.get()
        self._draw_grid()

    def _paint_at_event(self, event):
        x = (event.x - self.margin) // self.cell_size
        y = (event.y - self.margin) // self.cell_size

        if not (0 <= x < GRID and 0 <= y < GRID):
            return

        # Right mouse button is not delivered here on most Tk builds.
        # Normal drawing is therefore handled separately by <Button-3>.
        value = self.encode_pixel(self.gray, self.alpha)
        self.icons[self.current][y][x] = value
        self._draw_grid()
        self._update_info()

    def _erase_at_event(self, event):
        x = (event.x - self.margin) // self.cell_size
        y = (event.y - self.margin) // self.cell_size

        if not (0 <= x < GRID and 0 <= y < GRID):
            return

        self.icons[self.current][y][x] = 0
        self._draw_grid()
        self._update_info()

    def _draw_grid(self):
        self.canvas.delete("all")

        for y in range(GRID):
            for x in range(GRID):
                value = self.icons[self.current][y][x]
                gray, alpha = self.decode_pixel(value)

                # Draw a checkerboard under transparent pixels.
                if alpha:
                    # Four grayscale levels:
                    # 0 = black, 3 = white.
                    shade = int(gray * 255 / 3)
                    fill = f"#{shade:02x}{shade:02x}{shade:02x}"
                else:
                    # Checkerboard makes transparency visible.
                    fill = "#d0d0d0" if (x + y) % 2 == 0 else "#f0f0f0"

                x0 = self.margin + x * self.cell_size
                y0 = self.margin + y * self.cell_size
                x1 = x0 + self.cell_size
                y1 = y0 + self.cell_size

                self.canvas.create_rectangle(
                    x0,
                    y0,
                    x1,
                    y1,
                    fill=fill,
                    outline="#808080",
                )

                # Show grayscale number in opaque cells.
                if alpha:
                    self.canvas.create_text(
                        (x0 + x1) // 2,
                        (y0 + y1) // 2,
                        text=str(gray),
                        fill="#ffffff" if gray <= 1 else "#000000",
                    )

        # Outer border.
        size = GRID * self.cell_size
        self.canvas.create_rectangle(
            self.margin,
            self.margin,
            self.margin + size,
            self.margin + size,
            outline="#000000",
            width=2,
        )

    # ------------------------------------------------------------------
    # Icon navigation
    # ------------------------------------------------------------------

    def _set_current(self, index):
        index = max(0, min(MAX_ICONS - 1, index))
        self.current = index
        self.icon_var.set(index)
        self._draw_grid()
        self._update_info()

    def _previous(self):
        self._set_current((self.current - 1) % MAX_ICONS)

    def _next(self):
        self._set_current((self.current + 1) % MAX_ICONS)

    def _spin_changed(self):
        try:
            self._set_current(int(self.icon_var.get()))
        except ValueError:
            self.icon_var.set(self.current)

    def _spin_changed_event(self, _event):
        self._spin_changed()

    # ------------------------------------------------------------------
    # Icon operations
    # ------------------------------------------------------------------

    def _clear_current(self):
        self.icons[self.current] = blank_icon()
        self._draw_grid()
        self._update_info()

    def _copy_next(self):
        if self.current >= MAX_ICONS - 1:
            messagebox.showinfo("Copy", "Icon 255 has no next icon.")
            return

        source = self.icons[self.current]
        self.icons[self.current + 1] = [row[:] for row in source]
        self._set_current(self.current + 1)

    def _update_info(self):
        used = sum(
            1
            for icon in self.icons
            if any(pixel != 0 for row in icon for pixel in row)
        )

        opaque = sum(
            1
            for row in self.icons[self.current]
            for pixel in row
            if pixel & ALPHA_BIT
        )

        self.info_var.set(
            f"Icon {self.current}\n"
            f"Opaque pixels: {opaque}/49\n"
            f"Used icons: {used}/{MAX_ICONS}"
        )

    # ------------------------------------------------------------------
    # C output and input (.h parsing)
    # ------------------------------------------------------------------

    def _save_c(self):
        path = filedialog.asksaveasfilename(
            title="Save C array",
            defaultextension=".h",
            filetypes=[
                ("C header", "*.h"),
                ("C source", "*.c"),
                ("All files", "*.*"),
            ],
            initialfile="purrgo_poi_icons.h",
        )

        if not path:
            return

        try:
            text = self._generate_c()
            Path(path).write_text(text, encoding="utf-8")
            messagebox.showinfo("Saved", f"Saved:\n{path}")
        except OSError as exc:
            messagebox.showerror("Save error", str(exc))

    def _load_c(self):
        path = filedialog.askopenfilename(
            title="Load C array",
            filetypes=[
                ("C header", "*.h"),
                ("C source", "*.c"),
                ("All files", "*.*"),
            ],
        )

        if not path:
            return

        try:
            text = Path(path).read_text(encoding="utf-8")

            # Extract the array block using regex
            array_match = re.search(r'purrgo_poi_icons.*?=\s*\{(.*?)\};', text, re.DOTALL)
            if not array_match:
                raise ValueError("Could not find purrgo_poi_icons array in file.")

            # Extract all hex values
            hex_values = re.findall(r'0x[0-9A-Fa-f]{2}', array_match.group(1))
            if not hex_values:
                raise ValueError("No pixel data found.")

            int_values = [int(v, 16) for v in hex_values]

            icons = [blank_icon() for _ in range(MAX_ICONS)]
            idx = 0
            
            # Map back to 3D array [256][7][7]
            for i in range(MAX_ICONS):
                for y in range(GRID):
                    for x in range(GRID):
                        if idx < len(int_values):
                            val = int_values[idx]
                            if val < 0 or val > 0x07:
                                raise ValueError(f"Invalid pixel value {val} at index {idx}")
                            icons[i][y][x] = val
                            idx += 1

            self.icons = icons
            self._set_current(0)
            messagebox.showinfo("Loaded", f"Loaded:\n{path}")

        except Exception as exc:
            messagebox.showerror("Load error", str(exc))

    def _generate_c(self):
        """
        Generate a simple C header.

        Layout:
            purrgo_poi_icons[icon][y][x]

        One byte per pixel:
            bit 2     alpha
            bits 1:0  grayscale

        Therefore:
            0x00 = transparent black
            0x01 = transparent dark gray
            0x02 = transparent light gray
            0x03 = transparent white
            0x04 = opaque black
            0x05 = opaque dark gray
            0x06 = opaque light gray
            0x07 = opaque white
        """

        lines = [
            "/*",
            " * PurrGO 7x7 POI icons.",
            " * Generated by purrgo_icon_editor.py",
            " *",
            " * Pixel format:",
            " *   bit 2    = alpha (0 transparent, 1 opaque)",
            " *   bits 1:0 = grayscale (0..3)",
            " *",
            " * Layout:",
            " *   purrgo_poi_icons[icon][y][x]",
            " *",
            " * Icon selector is one byte: 0..255.",
            " */",
            "",
            "#ifndef PURRGO_POI_ICONS_H",
            "#define PURRGO_POI_ICONS_H",
            "",
            "#include <stdint.h>",
            "",
            "#define PURRGO_POI_ICON_COUNT 256u",
            "#define PURRGO_POI_ICON_WIDTH  7u",
            "#define PURRGO_POI_ICON_HEIGHT 7u",
            "",
            "#define PURRGO_POI_ALPHA 0x04u",
            "",
            "static const uint8_t purrgo_poi_icons[256][7][7] = {",
        ]

        for icon_index, icon in enumerate(self.icons):
            lines.append(f"    /* Icon {icon_index} */")
            lines.append("    {")

            for row in icon:
                values = ", ".join(f"0x{pixel:02X}" for pixel in row)
                lines.append(f"        {{ {values} }},")

            lines.append("    },")

        lines.extend([
            "};",
            "",
            "#endif /* PURRGO_POI_ICONS_H */",
            "",
        ])

        return "\n".join(lines)


def main():
    root = tk.Tk()
    app = IconEditor(root)

    # Right mouse button erases pixels.
    app.canvas.bind("<Button-3>", app._erase_at_event)
    app.canvas.bind("<B3-Motion>", app._erase_at_event)

    root.mainloop()


if __name__ == "__main__":
    main()