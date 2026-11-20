#!/usr/bin/env python3
"""
Генератор src/core/ui/boot_logo.c и boot_logo.h - логотип загрузочного баннера в формате
2-bpp framebuffer PurrGO.

    python3 tools/make_boot_logo.py                  # перегенерировать boot_logo.c/.h
    python3 tools/make_boot_logo.py --preview p.png  # плюс PNG для проверки глазами
    python3 tools/make_boot_logo.py --render-png     # сперва перерисовать PNG из SVG

Источник: docs/img/purrgo-logo.png - растровый рендер docs/img/purrgo-logo.svg (SVG - первоисточник
логотипа; PNG получается ключом --render-png, ему нужен cairosvg). Скрипт обрезает белые поля,
вписывает логотип в прямоугольник LOGO_H пикселей высотой (ширина по пропорциям, кратна 4) и
приводит яркость к четырём уровням E-Ink (0 чёрный, 1 тёмно-серый, 2 светло-серый, 3 белый).
Результат лежит в репозитории, поэтому для обычной сборки Python и Pillow не нужны: скрипт
запускают только при смене картинки (нужен Pillow: pip install pillow).

Раскладка байта та же, что у framebuffer (purrgo/gfx_fb2bpp.h): по четыре пикселя в байте, первый -
в старших битах; каждая строка занимает ровно W / 4 байт.
"""
import argparse
import os
import sys

from PIL import Image

LOGO_H = 152        # высота логотипа на экране, пикселей (экран 176 x 264; под ним версия и плашка Debug)
BG_LEVEL = 200      # ярче - фон при поиске границ логотипа
MARGIN_PX = 12      # поле вокруг логотипа в исходном изображении (PNG 1016 x 1488)
# Пороги яркости (0..255) между уровнями 0|1, 1|2, 2|3.
THRESHOLDS = (40, 125, 215)

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
IMG = os.path.join(ROOT, "docs", "img")
SRC_SVG = os.path.join(IMG, "purrgo-logo.svg")
SRC = os.path.join(IMG, "purrgo-logo.png")
DST_C = os.path.join(ROOT, "src", "core", "ui", "boot_logo.c")
DST_H = os.path.join(ROOT, "src", "core", "ui", "boot_logo.h")
SRC_REL = "docs/img/purrgo-logo.png"


def level(v):
    for i, t in enumerate(THRESHOLDS):
        if v < t:
            return i
    return 3


def render_png():
    try:
        import cairosvg
    except ImportError:
        sys.exit("для --render-png нужен cairosvg: pip install cairosvg")
    cairosvg.svg2png(url=SRC_SVG, write_to=SRC, output_width=1016, output_height=1488,
                     background_color="white")
    print("перерисован", os.path.relpath(SRC, ROOT))


def build():
    """Возвращает (ширина, высота, строки уровней)."""
    img = Image.open(SRC).convert("L")
    bbox = img.point(lambda v: 255 if v < BG_LEVEL else 0).getbbox()
    if bbox is None:
        sys.exit("логотип не найден в " + SRC)
    x0, y0, x1, y1 = bbox
    x0, y0 = max(0, x0 - MARGIN_PX), max(0, y0 - MARGIN_PX)
    x1, y1 = min(img.width, x1 + MARGIN_PX), min(img.height, y1 + MARGIN_PX)
    crop = img.crop((x0, y0, x1, y1))

    scale = LOGO_H / crop.height
    w_exact = max(1, round(crop.width * scale))
    w = (w_exact + 3) // 4 * 4                       # ширина кратна 4 (байт = 4 пикселя)
    small = crop.resize((w_exact, LOGO_H), Image.LANCZOS)
    canvas = Image.new("L", (w, LOGO_H), 255)        # добивка белым, логотип по центру
    canvas.paste(small, ((w - w_exact) // 2, 0))

    px = canvas.load()
    return w, LOGO_H, [[level(px[x, y]) for x in range(w)] for y in range(LOGO_H)]


def pack(levels, w):
    out = bytearray()
    for row in levels:
        for x in range(0, w, 4):
            b = 0
            for k in range(4):
                b |= row[x + k] << ((3 - k) * 2)
            out.append(b)
    return out


def write_c(data, w, h):
    lines = []
    for i in range(0, len(data), 16):
        lines.append("    " + ", ".join("0x%02X" % b for b in data[i:i + 16]) + ",")
    text = (
        "/*\n"
        " * Логотип PurrGO для загрузочного баннера. ФАЙЛ СГЕНЕРИРОВАН\n"
        " * tools/make_boot_logo.py из %s - не править вручную.\n"
        " *\n"
        " * %d x %d пикселей, 2 bpp (0 чёрный, 1 тёмно-серый, 2 светло-серый, 3 белый),\n"
        " * по четыре пикселя в байте, первый - в старших битах, строка - %d байт.\n"
        " */\n"
        "#include \"boot_logo.h\"\n"
        "\n"
        "const uint8_t purrgo_boot_logo[PURRGO_BOOT_LOGO_BYTES] = {\n"
        "%s\n"
        "};\n"
    ) % (SRC_REL, w, h, w // 4, "\n".join(lines))
    with open(DST_C, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)


def write_h(w, h):
    text = (
        "#ifndef PURRGO_BOOT_LOGO_DATA_H\n"
        "#define PURRGO_BOOT_LOGO_DATA_H\n"
        "\n"
        "#include <stdint.h>\n"
        "\n"
        "/*\n"
        " * Логотип загрузочного баннера (см. purrgo/boot_banner.h). ФАЙЛ СГЕНЕРИРОВАН\n"
        " * tools/make_boot_logo.py из %s - не править вручную.\n"
        " *\n"
        " * 2 bpp в раскладке framebuffer (purrgo/gfx_fb2bpp.h): значения 0..3 - те же\n"
        " * логические цвета, что BLACK..WHITE; по четыре пикселя в байте, первый - в\n"
        " * старших битах; каждая строка занимает PURRGO_BOOT_LOGO_W / 4 байт.\n"
        " */\n"
        "#define PURRGO_BOOT_LOGO_W      %d\n"
        "#define PURRGO_BOOT_LOGO_H      %d\n"
        "#define PURRGO_BOOT_LOGO_BYTES  ((PURRGO_BOOT_LOGO_W / 4) * PURRGO_BOOT_LOGO_H)\n"
        "\n"
        "extern const uint8_t purrgo_boot_logo[PURRGO_BOOT_LOGO_BYTES];\n"
        "\n"
        "#endif /* PURRGO_BOOT_LOGO_DATA_H */\n"
    ) % (SRC_REL, w, h)
    with open(DST_H, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[1])
    ap.add_argument("--preview", metavar="PNG", help="сохранить 4-уровневое превью")
    ap.add_argument("--render-png", action="store_true", help="перерисовать PNG из SVG (cairosvg)")
    args = ap.parse_args()

    if args.render_png:
        render_png()
    w, h, levels = build()
    write_c(pack(levels, w), w, h)
    write_h(w, h)
    print("записаны %s и %s: %d x %d, %d байт данных" % (
        os.path.relpath(DST_C, ROOT), os.path.relpath(DST_H, ROOT), w, h, w * h // 4))

    if args.preview:
        shade = (0, 85, 170, 255)
        pv = Image.new("L", (w, h))
        pv.putdata([shade[v] for row in levels for v in row])
        pv.resize((w * 3, h * 3), Image.NEAREST).save(args.preview)
        print("превью:", args.preview)


if __name__ == "__main__":
    main()
