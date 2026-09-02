#!/usr/bin/env python3
"""
PurrGO 6x6 POI Icon Editor

Simple Tkinter editor for a set of up to 256 icons.

Each pixel has 5 possible states:
    TRANSPARENT
    WHITE
    LIGHT_GRAY
    DARK_GRAY
    BLACK

Internal pixel encoding uses 3 bits:
    bit 0..1 = grayscale value
        0 = white
        1 = light gray
        2 = dark gray
        3 = black
    bit 2 = alpha
        0 = transparent
        1 = opaque

Thus 6x6 = 36 pixels = 108 bits = 14 bytes per icon.

Pixels are packed in row-major order, 3 bits per pixel, least-significant
bit first. The generated C header contains:
    PURRGO_ICON_WIDTH       6
    PURRGO_ICON_HEIGHT      6
    PURRGO_ICON_COUNT       actual number of icons
    PURRGO_ICON_BYTES       14
    purrgo_icons[count][14]

Icon selection is one byte (0..255).
"""

import tkinter as tk
from tkinter import filedialog, messagebox, simpledialog
from pathlib import Path

WIDTH = 6
HEIGHT = 6
MAX_ICONS = 256
PIXELS = WIDTH * HEIGHT
BYTES_PER_ICON = (PIXELS * 3 + 7) // 8

# Pixel values:
# -1 = transparent
#  0 = white
#  1 = light gray
#  2 = dark gray
#  3 = black
TRANSPARENT = -1

PALETTE = [
    ("Transparent", TRANSPARENT),
    ("White", 0),
    ("Light gray", 1),
    ("Dark gray", 2),
    ("Black", 3),
]

CELL = 55
GRID_X = 20
GRID_Y = 20

BG = "#d0d0d0"


class IconEditor:
    def __init__(self, root):
        self.root = root
        self.root.title("PurrGO 6x6 POI Icon Editor")

        # Start with one icon.
        self.icons = [[TRANSPARENT] * PIXELS]
        self.names = ["icon_00"]
        self.current = 0
        self.selected_value = TRANSPARENT
        self.drawing = False
        self.drag_value = TRANSPARENT

        self.build_ui()
        self.refresh_icon_list()
        self.draw_grid()
        self.update_status()

    def build_ui(self):
        # Left: icon list
        left = tk.Frame(self.root)
        left.pack(side=tk.LEFT, fill=tk.Y, padx=8, pady=8)

        tk.Label(left, text="Icons (0..255)").pack()

        list_frame = tk.Frame(left)
        list_frame.pack(fill=tk.Y, expand=True)

        scrollbar = tk.Scrollbar(list_frame, orient=tk.VERTICAL)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)

        self.icon_list = tk.Listbox(
            list_frame,
            width=22,
            height=22,
            exportselection=False,
            yscrollcommand=scrollbar.set,
        )
        self.icon_list.pack(side=tk.LEFT, fill=tk.Y)
        scrollbar.config(command=self.icon_list.yview)
        self.icon_list.bind("<<ListboxSelect>>", self.on_icon_select)

        buttons = tk.Frame(left)
        buttons.pack(fill=tk.X, pady=8)

        tk.Button(buttons, text="New", command=self.new_icon).pack(fill=tk.X)
        tk.Button(buttons, text="Delete", command=self.delete_icon).pack(fill=tk.X, pady=2)
        tk.Button(buttons, text="Rename", command=self.rename_icon).pack(fill=tk.X)

        # Center: editor
        center = tk.Frame(self.root)
        center.pack(side=tk.LEFT, padx=8, pady=8)

        self.canvas = tk.Canvas(
            center,
            width=GRID_X * 2 + WIDTH * CELL,
            height=GRID_Y * 2 + HEIGHT * CELL,
            bg="white",
            highlightthickness=1,
            highlightbackground="black",
        )
        self.canvas.pack()

        self.canvas.bind("<Button-1>", self.paint_start)
        self.canvas.bind("<B1-Motion>", self.paint_motion)
        self.canvas.bind("<ButtonRelease-1>", self.paint_end)
        self.canvas.bind("<Button-3>", self.erase_at)
        self.canvas.bind("<B3-Motion>", self.erase_motion)

        # Palette
        palette = tk.Frame(center)
        palette.pack(fill=tk.X, pady=8)

        self.palette_var = tk.StringVar(value="Transparent")

        for text, value in PALETTE:
            tk.Radiobutton(
                palette,
                text=text,
                variable=self.palette_var,
                value=text,
                command=self.select_palette,
            ).pack(side=tk.LEFT, padx=3)

        # Commands
        commands = tk.Frame(center)
        commands.pack(fill=tk.X)

        tk.Button(commands, text="Clear", command=self.clear_icon).pack(side=tk.LEFT)
        tk.Button(commands, text="Fill white", command=lambda: self.fill_icon(0)).pack(side=tk.LEFT, padx=3)
        tk.Button(commands, text="Fill transparent", command=lambda: self.fill_icon(TRANSPARENT)).pack(side=tk.LEFT)

        # Right: file operations / status
        right = tk.Frame(self.root)
        right.pack(side=tk.LEFT, fill=tk.Y, padx=8, pady=8)

        tk.Button(right, text="Open C array...", command=self.open_c_array).pack(fill=tk.X)
        tk.Button(right, text="Save C array...", command=self.save_c_array).pack(fill=tk.X, pady=4)

        tk.Label(
            right,
            text=(
                "Pixel format:\n"
                "3 bits / pixel\n"
                "2 bits grayscale\n"
                "1 bit alpha\n\n"
                "36 pixels / icon\n"
                "14 bytes / icon"
            ),
            justify=tk.LEFT,
            anchor="w",
        ).pack(fill=tk.X, pady=15)

        self.status = tk.Label(right, text="", justify=tk.LEFT, anchor="w")
        self.status.pack(fill=tk.X)

        tk.Label(
            right,
            text="Left mouse: draw\nRight mouse: transparent",
            justify=tk.LEFT,
            anchor="w",
        ).pack(fill=tk.X, pady=15)

    def select_palette(self):
        name = self.palette_var.get()
        for text, value in PALETTE:
            if text == name:
                self.selected_value = value
                return

    def refresh_icon_list(self):
        self.icon_list.delete(0, tk.END)
        for i, name in enumerate(self.names):
            self.icon_list.insert(tk.END, f"{i:02X}  {name}")

        if self.names:
            self.icon_list.selection_clear(0, tk.END)
            self.icon_list.selection_set(self.current)
            self.icon_list.see(self.current)

    def on_icon_select(self, _event=None):
        selection = self.icon_list.curselection()
        if not selection:
            return
        self.current = selection[0]
        self.draw_grid()
        self.update_status()

    def new_icon(self):
        if len(self.icons) >= MAX_ICONS:
            messagebox.showwarning("Icon limit", "Maximum is 256 icons.")
            return

        index = len(self.icons)
        self.icons.append([TRANSPARENT] * PIXELS)
        self.names.append(f"icon_{index:02X}")
        self.current = index

        self.refresh_icon_list()
        self.draw_grid()
        self.update_status()

    def delete_icon(self):
        if len(self.icons) <= 1:
            messagebox.showwarning("Delete", "At least one icon must remain.")
            return

        if not messagebox.askyesno(
            "Delete icon",
            f"Delete icon {self.current:02X} ({self.names[self.current]})?",
        ):
            return

        del self.icons[self.current]
        del self.names[self.current]

        self.current = min(self.current, len(self.icons) - 1)
        self.refresh_icon_list()
        self.draw_grid()
        self.update_status()

    def rename_icon(self):
        old = self.names[self.current]
        name = simpledialog.askstring("Rename icon", "C identifier:", initialvalue=old)
        if name is None:
            return

        name = self.sanitize_identifier(name)
        if not name:
            messagebox.showerror("Invalid name", "Name must contain letters, digits or underscore.")
            return

        self.names[self.current] = name
        self.refresh_icon_list()

    @staticmethod
    def sanitize_identifier(name):
        result = []
        for ch in name.strip():
            if ch.isalnum() or ch == "_":
                result.append(ch)
        name = "".join(result)

        if name and name[0].isdigit():
            name = "_" + name
        return name

    def pixel_rect(self, x, y):
        x0 = GRID_X + x * CELL
        y0 = GRID_Y + y * CELL
        return x0, y0, x0 + CELL, y0 + CELL

    def pixel_color(self, value):
        if value == TRANSPARENT:
            return BG
        if value == 0:
            return "#ffffff"
        if value == 1:
            return "#aaaaaa"
        if value == 2:
            return "#555555"
        return "#000000"

    def draw_grid(self):
        self.canvas.delete("all")
        pixels = self.icons[self.current]

        for y in range(HEIGHT):
            for x in range(WIDTH):
                value = pixels[y * WIDTH + x]
                x0, y0, x1, y1 = self.pixel_rect(x, y)
                fill = self.pixel_color(value)

                # Checkerboard for transparent pixels.
                if value == TRANSPARENT:
                    self.canvas.create_rectangle(
                        x0, y0, x1, y1,
                        fill="#eeeeee",
                        outline="#777777",
                    )
                    s = CELL // 2
                    self.canvas.create_rectangle(
                        x0, y0, x0 + s, y0 + s,
                        fill="#cccccc", outline=""
                    )
                    self.canvas.create_rectangle(
                        x0 + s, y0 + s, x1, y1,
                        fill="#cccccc", outline=""
                    )
                    self.canvas.create_rectangle(
                        x0, y0, x1, y1,
                        outline="#777777",
                    )
                else:
                    self.canvas.create_rectangle(
                        x0, y0, x1, y1,
                        fill=fill,
                        outline="#777777",
                    )

    def canvas_pixel(self, event):
        x = (event.x - GRID_X) // CELL
        y = (event.y - GRID_Y) // CELL

        if 0 <= x < WIDTH and 0 <= y < HEIGHT:
            return x, y
        return None

    def set_pixel(self, x, y, value):
        self.icons[self.current][y * WIDTH + x] = value
        self.draw_grid()

    def paint_start(self, event):
        p = self.canvas_pixel(event)
        if p is None:
            return

        self.drawing = True
        self.drag_value = self.selected_value
        self.set_pixel(*p, self.drag_value)

    def paint_motion(self, event):
        if not self.drawing:
            return

        p = self.canvas_pixel(event)
        if p is not None:
            self.set_pixel(*p, self.drag_value)

    def paint_end(self, _event):
        self.drawing = False

    def erase_at(self, event):
        p = self.canvas_pixel(event)
        if p is not None:
            self.set_pixel(*p, TRANSPARENT)

    def erase_motion(self, event):
        p = self.canvas_pixel(event)
        if p is not None:
            self.set_pixel(*p, TRANSPARENT)

    def clear_icon(self):
        self.fill_icon(TRANSPARENT)

    def fill_icon(self, value):
        self.icons[self.current] = [value] * PIXELS
        self.draw_grid()

    def update_status(self):
        self.status.config(
            text=(
                f"Current: {self.current:02X}\n"
                f"Name: {self.names[self.current]}\n"
                f"Icons: {len(self.icons)}/{MAX_ICONS}"
            )
        )

    # ------------------------------------------------------------------
    # C array encoding
    # ------------------------------------------------------------------

    @staticmethod
    def encode_icon(pixels):
        """
        Pack 36 pixels, 3 bits each, into 14 bytes.

        Pixel bit layout:
            transparent -> 0b000
            white       -> 0b001
            light gray  -> 0b011
            dark gray   -> 0b101
            black       -> 0b111

        The encoding deliberately keeps alpha as the MSB:
            bit 2 = alpha
            bits 1..0 = gray

        All pixels are packed consecutively, starting with pixel 0
        at the least significant bit of byte 0.
        """
        value = 0

        for i, pixel in enumerate(pixels):
            if pixel == TRANSPARENT:
                encoded = 0
            else:
                encoded = 0x4 | (pixel & 0x03)
            value |= encoded << (i * 3)

        return value.to_bytes(BYTES_PER_ICON, "little")

    @staticmethod
    def decode_icon(data):
        pixels = []
        value = int.from_bytes(data[:BYTES_PER_ICON], "little")

        for i in range(PIXELS):
            encoded = (value >> (i * 3)) & 0x07

            alpha = encoded & 0x04
            gray = encoded & 0x03

            if not alpha:
                pixels.append(TRANSPARENT)
            else:
                pixels.append(gray)

        return pixels

    def save_c_array(self):
        filename = filedialog.asksaveasfilename(
            title="Save PurrGO icon C array",
            defaultextension=".h",
            filetypes=[
                ("C header", "*.h"),
                ("C source", "*.c"),
                ("All files", "*.*"),
            ],
        )

        if not filename:
            return

        try:
            text = self.generate_c_header()
            Path(filename).write_text(text, encoding="utf-8")
        except OSError as exc:
            messagebox.showerror("Save error", str(exc))
            return

        messagebox.showinfo(
            "Saved",
            f"Saved {len(self.icons)} icons to:\n{filename}",
        )

    def generate_c_header(self):
        guard = "PURRGO_POI_ICONS_H"
        lines = []

        lines.append("/*")
        lines.append(" * PurrGO POI icons")
        lines.append(" * Generated by PurrGO 6x6 POI Icon Editor.")
        lines.append(" *")
        lines.append(" * Pixel format: 3 bits/pixel")
        lines.append(" *   bit 2   = alpha (0 transparent, 1 opaque)")
        lines.append(" *   bits 1:0 = grayscale")
        lines.append(" *              0 white")
        lines.append(" *              1 light gray")
        lines.append(" *              2 dark gray")
        lines.append(" *              3 black")
        lines.append(" *")
        lines.append(" * Packing: row-major, 3 bits/pixel, LSB first.")
        lines.append(" * 6x6 pixels = 36 pixels = 108 bits = 14 bytes/icon.")
        lines.append(" */")
        lines.append("")
        lines.append(f"#ifndef {guard}")
        lines.append(f"#define {guard}")
        lines.append("")
        lines.append("#include <stdint.h>")
        lines.append("")
        lines.append(f"#define PURRGO_ICON_WIDTH       {WIDTH}u")
        lines.append(f"#define PURRGO_ICON_HEIGHT      {HEIGHT}u")
        lines.append(f"#define PURRGO_ICON_COUNT       {len(self.icons)}u")
        lines.append(f"#define PURRGO_ICON_BYTES       {BYTES_PER_ICON}u")
        lines.append("")
        lines.append(
            "/* One byte selects an icon: 0x00..0xFF. "
            "PURRGO_ICON_COUNT contains the number actually defined. */"
        )
        lines.append("")

        lines.append(
            f"static const uint8_t purrgo_icons[{len(self.icons)}][{BYTES_PER_ICON}] = {{"
        )

        for index, (name, pixels) in enumerate(zip(self.names, self.icons)):
            data = self.encode_icon(pixels)
            lines.append(f"    /* 0x{index:02X}: {name} */")

            # 14 bytes split over two lines for readability.
            first = ", ".join(f"0x{b:02X}" for b in data[:7])
            second = ", ".join(f"0x{b:02X}" for b in data[7:])

            lines.append(f"    {{ {first},")
            lines.append(f"      {second} }},")

        lines.append("};")
        lines.append("")
        lines.append(f"#endif /* {guard} */")
        lines.append("")

        return "\n".join(lines)

    # ------------------------------------------------------------------
    # Loading generated C array
    # ------------------------------------------------------------------

    def open_c_array(self):
        filename = filedialog.askopenfilename(
            title="Open PurrGO icon C array",
            filetypes=[
                ("C files", "*.h *.c"),
                ("All files", "*.*"),
            ],
        )

        if not filename:
            return

        try:
            text = Path(filename).read_text(encoding="utf-8")
            data = self.parse_c_array(text)
        except (OSError, ValueError) as exc:
            messagebox.showerror("Open error", str(exc))
            return

        self.icons = []
        self.names = []

        for index, (name, icon_data) in enumerate(data):
            self.icons.append(self.decode_icon(icon_data))
            self.names.append(name or f"icon_{index:02X}")

        if not self.icons:
            self.icons = [[TRANSPARENT] * PIXELS]
            self.names = ["icon_00"]

        self.current = 0
        self.refresh_icon_list()
        self.draw_grid()
        self.update_status()

    @staticmethod
    def parse_c_array(text):
        """
        Parse the generated format without requiring a C parser.

        The parser intentionally accepts only byte literals inside the
        purrgo_icons initializer. This is sufficient for files generated
        by this editor.
        """
        marker = "purrgo_icons"
        pos = text.find(marker)
        if pos < 0:
            raise ValueError("purrgo_icons array was not found.")

        start = text.find("{", pos)
        if start < 0:
            raise ValueError("purrgo_icons initializer was not found.")

        end = text.find("};", start)
        if end < 0:
            raise ValueError("End of purrgo_icons array was not found.")

        body = text[start + 1:end]

        import re

        # Each icon has a comment immediately before its initializer.
        pattern = re.compile(
            r"/\*\s*0x([0-9A-Fa-f]{2}):\s*([^*]*?)\s*\*/\s*"
            r"\{\s*([^{}]*?)\}",
            re.DOTALL,
        )

        result = []

        for match in pattern.finditer(body):
            name = match.group(2).strip()
            byte_text = match.group(3)

            byte_tokens = re.findall(
                r"0[xX][0-9A-Fa-f]+|\b\d+\b",
                byte_text,
            )

            values = []
            for token in byte_tokens:
                value = int(token, 0)
                if not 0 <= value <= 255:
                    raise ValueError("Invalid byte value in C array.")
                values.append(value)

            if len(values) != BYTES_PER_ICON:
                raise ValueError(
                    f"Icon {match.group(1)} has {len(values)} bytes; "
                    f"expected {BYTES_PER_ICON}."
                )

            result.append((name, bytes(values)))

            if len(result) > MAX_ICONS:
                raise ValueError("More than 256 icons are not supported.")

        if not result:
            raise ValueError("No icon records found.")

        return result


def main():
    root = tk.Tk()
    IconEditor(root)
    root.mainloop()


if __name__ == "__main__":
    main()
