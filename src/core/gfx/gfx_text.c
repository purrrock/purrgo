#include "purrgo/gfx_text.h"
#include "purrgo/font5x7.h"

void gfx_draw_char(gfx_context_t *ctx, int16_t x, int16_t y, char c)
{
    if (ctx == NULL) return;

    unsigned char uc = (unsigned char)c;
    const unsigned char* bitmap = font5x7[uc];

    // Save current colors
    gfx_color_t fg = ctx->color_fg;
    gfx_color_t bg = ctx->color_bg;

    for (int col = 0; col < 5; col++) {
        for (int row = 0; row < 8; row++) {
            if ((bitmap[col] >> row) & 1) {
                ctx->color_fg = fg;
            } else {
                ctx->color_fg = bg;
            }
            gfx_draw_pixel(ctx, x + col, y + row);
        }
    }

    // Draw the 6th spacing column
    ctx->color_fg = bg;
    for (int row = 0; row < 8; row++) {
        gfx_draw_pixel(ctx, x + 5, y + row);
    }

    // Restore foreground color
    ctx->color_fg = fg;
}

void gfx_draw_string(gfx_context_t *ctx, int16_t x, int16_t y, const char *str)
{
    if (ctx == NULL || str == NULL) return;

    int16_t cur_x = x;
    int16_t cur_y = y;

    while (*str) {
        if (*str == '\n') {
            cur_x = x;
            cur_y += 8;
        } else {
            gfx_draw_char(ctx, cur_x, cur_y, *str);
            cur_x += 6;
            if (cur_x + 6 > ctx->width) {
                cur_x = 0; // or x to wrap aligned?
                cur_y += 8;
            }
        }
        str++;
    }
}
