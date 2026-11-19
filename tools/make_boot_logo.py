#!/usr/bin/env python3
"""
Генератор src/core/ui/boot_logo.c - логотип загрузочного баннера в формате
2-bpp framebuffer PurrGO.

    python3 tools/make_boot_logo.py                 # перегенерировать boot_logo.c
    python3 tools/make_boot_logo.py --preview p.png # плюс PNG для проверки глазами

Источник: docs/img/purrgo-logoBW.jpeg. Скрипт обрезает белые поля, вписывает
логотип в квадрат SIZE x SIZE и приводит яркость к четырём уровням E-Ink
(0 чёрный, 1 тёмно-серый, 2 светло-серый, 3 белый). Результат кладётся в
репозиторий, поэтому для обычной сборки Python и Pillow не нужны: скрипт
запускают только при смене картинки (нужен Pillow: pip install pillow).

Раскладка байта та же, что у framebuffer (purrgo/gfx_fb2bpp.h): по четыре
пикселя в байте, первый - в старших битах; каждая строка занимает ровно
SIZE / 4 байт (SIZE кратен 4).
"""
import argparse
import os
import sys

from PIL import Image

SIZE = 160          # сторона логотипа на экране, пикселей (кратна 4; экран 176 x 264)
BG_LEVEL = 200      # ярче - фон при поиске границ логотипа
MARGIN_PX = 6       # поле вокруг логотипа в исходном изображении
# Пороги яркости (0..255) между уровнями 0|1, 1|2, 2|3.
THRESHOLDS = (40, 125, 215)

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "docs", "img", "purrgo-logoBW.jpeg")
DST = os.path.join(ROOT, "src", "core", "ui", "boot_logo.c")


def level(v):
    for i, t in enumerate(THRESHOLDS):
        if v < t:
            return i
    return 3


def build():
    img = Image.open(SRC).convert("L")
    bbox = img.point(lambda v: 255 if v < BG_LEVEL else 0).getbbox()
    if bbox is None:
        sys.exit("логотип не найден в " + SRC)
    x0, y0, x1, y1 = bbox
    x0, y0 = max(0, x0 - MARGIN_PX), max(0, y0 - MARGIN_PX)
    x1, y1 = min(img.width, x1 + MARGIN_PX), min(img.height, y1 + MARGIN_PX)
    crop = img.crop((x0, y0, x1, y1))

    # Квадратный холст цвета фона, логотип по центру.
    side = max(crop.size)
    canvas = Image.new("L", (side, side), 255)
    canvas.paste(crop, ((side - crop.width) // 2, (side - crop.height) // 2))
    small = canvas.resize((SIZE, SIZE), Image.LANCZOS)

    px = small.load()
    return [[level(px[x, y]) for x in range(SIZE)] for y in range(SIZE)]


def pack(levels):
    out = bytearray()
    for row in levels:
        for x in range(0, SIZE, 4):
            b = 0
            for k in range(4):
                b |= row[x + k] << ((3 - k) * 2)
            out.append(b)
    return out


def write_c(data):
    lines = []
    for i in range(0, len(data), 16):
        lines.append("    " + ", ".join("0x%02X" % b for b in data[i:i + 16]) + ",")
    text = (
        "/*\n"
        " * Логотип PurrGO для загрузочного баннера. ФАЙЛ СГЕНЕРИРОВАН\n"
        " * tools/make_boot_logo.py из docs/img/purrgo-logoBW.jpeg - не править вручную.\n"
        " *\n"
        " * %d x %d пикселей, 2 bpp (0 чёрный, 1 тёмно-серый, 2 светло-серый, 3 белый),\n"
        " * по четыре пикселя в байте, первый - в старших битах, строка - %d байт.\n"
        " */\n"
        "#include \"boot_logo.h\"\n"
        "\n"
        "const uint8_t purrgo_boot_logo[PURRGO_BOOT_LOGO_BYTES] = {\n"
        "%s\n"
        "};\n"
    ) % (SIZE, SIZE, SIZE // 4, "\n".join(lines))
    with open(DST, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[1])
    ap.add_argument("--preview", metavar="PNG", help="сохранить 4-уровневое превью")
    args = ap.parse_args()

    levels = build()
    write_c(pack(levels))
    print("записан %s (%d байт данных)" % (os.path.relpath(DST, ROOT), SIZE * SIZE // 4))

    if args.preview:
        shade = (0, 85, 170, 255)
        pv = Image.new("L", (SIZE, SIZE))
        pv.putdata([shade[v] for row in levels for v in row])
        pv.resize((SIZE * 3, SIZE * 3), Image.NEAREST).save(args.preview)
        print("превью:", args.preview)


if __name__ == "__main__":
    main()
