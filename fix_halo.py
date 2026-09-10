content = open("src/core/gfx/gfx_text.c").read()

bad = """static void gfx_draw_char_halo(gfx_context_t *ctx, int16_t x, int16_t y, char c) {
    if (!ctx || !ctx->draw_pixel) return;

    const unsigned char* bitmap = font5x7[(unsigned char)c];
    for (int col = 0; col < 5; col++) {
        for (int row = 0; row < 8; row++) {
            if ((bitmap[col] >> row) & 1) {
                for (int dx = -1; dx <= 1; dx++) {
                    for (int dy = -1; dy <= 1; dy++) {
                        if (dx != 0 || dy != 0) {
                            gfx_draw_pixel(ctx, x + col + dx, y + row + dy);
                        }
                    }
                }
            }
        }
    }
}"""
good = """static void gfx_draw_char_halo(gfx_context_t *ctx, int16_t x, int16_t y, char c) {
    if (!ctx || !ctx->draw_pixel) return;

    const unsigned char* bitmap = font5x7[(unsigned char)c];

    // We compute a 7x10 mask to draw the halo. The font is 5x7.
    // The halo adds 1 pixel to each side, so 5+2=7 columns, 8+2=10 rows.
    // 10 rows means we need 10 bits, which fits in a uint16_t array.
    uint16_t halo[7] = {0};

    for (int col = 0; col < 5; col++) {
        uint8_t column_data = bitmap[col];
        if (!column_data) continue;

        // Expand vertically
        uint16_t expanded_y = (uint16_t)column_data | ((uint16_t)column_data << 1) | ((uint16_t)column_data << 2);

        // Expand horizontally
        halo[col] |= expanded_y;
        halo[col+1] |= expanded_y;
        halo[col+2] |= expanded_y;
    }

    for (int col = 0; col < 7; col++) {
        uint16_t column_data = halo[col];
        if (!column_data) continue;

        for (int row = 0; row < 10; row++) {
            // We subtract 1 from x and y because the halo starts 1 pixel before the character
            // And we omit the center text pixels (which will be drawn in black over it, but skipping them might save calls?)
            // Actually wait, it's easier to just draw the whole mask except where text is.
            if ((column_data >> row) & 1) {
                // If it's a font pixel, we shouldn't skip it because we want the halo to be under the font,
                // but wait, if it's a font pixel, we are going to draw black over it anyway.
                // However, the original code draws around it. We can just draw the halo pixels.

                // Let's check if it's a font pixel
                int is_font = 0;
                if (col >= 1 && col <= 5 && row >= 1 && row <= 8) {
                    if ((bitmap[col-1] >> (row-1)) & 1) {
                        is_font = 1;
                    }
                }

                if (!is_font) {
                    gfx_draw_pixel(ctx, x + col - 1, y + row - 1);
                }
            }
        }
    }
}"""
content = content.replace(bad, good)
open("src/core/gfx/gfx_text.c", "w").write(content)
