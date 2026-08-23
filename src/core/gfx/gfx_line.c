#include "purrgo/gfx_line.h"
#include "purrgo/gfx_renderer.h"

// Битовые маски зон для алгоритма Коэна-Сазерленда
#define INSIDE 0 // 0000
#define LEFT   1 // 0001
#define RIGHT  2 // 0010
#define BOTTOM 4 // 0100
#define TOP    8 // 1000

// Побитовая оптимизация модуля без ветвления для архитектуры ARM Cortex
static inline int16_t gfx_abs(int16_t a) {
    int16_t mask = a >> 15;
    return (a + mask) ^ mask;
}

// Вычисление кода зоны для точки относительно границ экрана
static uint8_t compute_outcode(gfx_context_t *ctx, int16_t x, int16_t y) {
    uint8_t code = INSIDE;
    if (x < 0) {
        code |= LEFT;
    } else if (x >= ctx->width) {
        code |= RIGHT;
    }
    
    if (y < 0) {
        code |= BOTTOM;
    } else if (y >= ctx->height) {
        code |= TOP;
    }
    return code;
}

void gfx_draw_line(gfx_context_t *ctx, int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    if (ctx == NULL || ctx->draw_pixel == NULL) return;

    // 1. Отсечение отрезка (Cohen-Sutherland)
    uint8_t outcode0 = compute_outcode(ctx, x0, y0);
    uint8_t outcode1 = compute_outcode(ctx, x1, y1);
    bool accept = false;

    while (true) {
        if (!(outcode0 | outcode1)) {
            accept = true;
            break;
        } else if (outcode0 & outcode1) {
            break;
        } else {
            int16_t x, y;
            uint8_t outcodeOut = outcode0 ? outcode0 : outcode1;

            int32_t dx = x1 - x0;
            int32_t dy = y1 - y0;

            if (outcodeOut & TOP) {
                x = x0 + dx * (ctx->height - 1 - y0) / dy;
                y = ctx->height - 1;
            } else if (outcodeOut & BOTTOM) {
                x = x0 + dx * (0 - y0) / dy;
                y = 0;
            } else if (outcodeOut & RIGHT) {
                y = y0 + dy * (ctx->width - 1 - x0) / dx;
                x = ctx->width - 1;
            } else if (outcodeOut & LEFT) {
                y = y0 + dy * (0 - x0) / dx;
                x = 0;
            }

            if (outcodeOut == outcode0) {
                x0 = x;
                y0 = y;
                outcode0 = compute_outcode(ctx, x0, y0);
            } else {
                x1 = x;
                y1 = y;
                outcode1 = compute_outcode(ctx, x1, y1);
            }
        }
    }

    if (!accept) {
        return; 
    }

    // 2. Отрисовка видимой части (Bresenham optimized)
    int16_t dx = gfx_abs(x1 - x0);
    int16_t sx = x0 < x1 ? 1 : -1;
    int16_t dy = -gfx_abs(y1 - y0);
    int16_t sy = y0 < y1 ? 1 : -1;
    
    int16_t err = dx + dy; 
    int16_t e2;

    while (true) {
        ctx->draw_pixel(ctx->framebuffer, x0, y0, ctx->color_fg);

        if (x0 == x1 && y0 == y1) {
            break;
        }

        e2 = err << 1; 
        
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void gfx_draw_dashed_line(gfx_context_t *ctx, int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    if (ctx == NULL || ctx->draw_pixel == NULL) return;

    // Сохраняем геометрическое начало линии для синхронизации фазы штрихов
    int16_t orig_x0 = x0;
    int16_t orig_y0 = y0;

    // 1. Отсечение отрезка (Cohen-Sutherland)
    uint8_t outcode0 = compute_outcode(ctx, x0, y0);
    uint8_t outcode1 = compute_outcode(ctx, x1, y1);
    bool accept = false;

    while (true) {
        if (!(outcode0 | outcode1)) {
            accept = true;
            break;
        } else if (outcode0 & outcode1) {
            break;
        } else {
            int16_t x, y;
            uint8_t outcodeOut = outcode0 ? outcode0 : outcode1;

            int32_t dx = x1 - x0;
            int32_t dy = y1 - y0;

            if (outcodeOut & TOP) {
                x = x0 + dx * (ctx->height - 1 - y0) / dy;
                y = ctx->height - 1;
            } else if (outcodeOut & BOTTOM) {
                x = x0 + dx * (0 - y0) / dy;
                y = 0;
            } else if (outcodeOut & RIGHT) {
                y = y0 + dy * (ctx->width - 1 - x0) / dx;
                x = ctx->width - 1;
            } else if (outcodeOut & LEFT) {
                y = y0 + dy * (0 - x0) / dx;
                x = 0;
            }

            if (outcodeOut == outcode0) {
                x0 = x;
                y0 = y;
                outcode0 = compute_outcode(ctx, x0, y0);
            } else {
                x1 = x;
                y1 = y;
                outcode1 = compute_outcode(ctx, x1, y1);
            }
        }
    }

    if (!accept) {
        return;
    }

    // Параметры паттерна штриха
    const int16_t dash_len = 4;
    const int16_t gap_len = 4;
    const int16_t pattern_len = dash_len + gap_len; // 8 пикселей

    // Вычисляем количество пропущенных пикселей (Chebyshev distance)
    int16_t dx_clip = gfx_abs(x0 - orig_x0);
    int16_t dy_clip = gfx_abs(y0 - orig_y0);
    int16_t clipped_steps = (dx_clip > dy_clip) ? dx_clip : dy_clip;
    
    // Инициализируем счетчик с учетом отсеченной части.
    // Так как pattern_len = 8 (степень двойки), компилятор оптимизирует 
    // операцию деления с остатком (%) в побитовое И (& 7).
    int16_t dash_counter = clipped_steps & 7;

    // 2. Отрисовка видимой части (Bresenham optimized)
    int16_t dx = gfx_abs(x1 - x0);
    int16_t sx = x0 < x1 ? 1 : -1;
    int16_t dy = -gfx_abs(y1 - y0);
    int16_t sy = y0 < y1 ? 1 : -1;

    int16_t err = dx + dy;
    int16_t e2;

    while (true) {
        if (dash_counter < dash_len) {
            ctx->draw_pixel(ctx->framebuffer, x0, y0, ctx->color_fg);
        }

        dash_counter++;
        if (dash_counter >= pattern_len) {
            dash_counter = 0;
        }

        if (x0 == x1 && y0 == y1) {
            break;
        }

        e2 = err << 1;

        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void gfx_draw_thick_line(gfx_context_t *ctx, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t thickness)
{
    if (ctx == NULL || thickness <= 0) return;

    if (thickness == 1) {
        gfx_draw_line(ctx, x0, y0, x1, y1);
        return;
    }

    // Сохраняем исходные координаты для отрисовки "шапок" на стыках
    int16_t orig_x0 = x0;
    int16_t orig_y0 = y0;
    int16_t orig_x1 = x1;
    int16_t orig_y1 = y1;

    // 1. Отсечение отрезка (Cohen-Sutherland)
    uint8_t outcode0 = compute_outcode(ctx, x0, y0);
    uint8_t outcode1 = compute_outcode(ctx, x1, y1);
    bool accept = false;

    while (true) {
        if (!(outcode0 | outcode1)) {
            accept = true;
            break;
        } else if (outcode0 & outcode1) {
            break;
        } else {
            int16_t x, y;
            uint8_t outcodeOut = outcode0 ? outcode0 : outcode1;

            int32_t dx = x1 - x0;
            int32_t dy = y1 - y0;

            if (outcodeOut & TOP) {
                x = x0 + dx * (ctx->height - 1 - y0) / dy;
                y = ctx->height - 1;
            } else if (outcodeOut & BOTTOM) {
                x = x0 + dx * (0 - y0) / dy;
                y = 0;
            } else if (outcodeOut & RIGHT) {
                y = y0 + dy * (ctx->width - 1 - x0) / dx;
                x = ctx->width - 1;
            } else if (outcodeOut & LEFT) {
                y = y0 + dy * (0 - x0) / dx;
                x = 0;
            }

            if (outcodeOut == outcode0) {
                x0 = x;
                y0 = y;
                outcode0 = compute_outcode(ctx, x0, y0);
            } else {
                x1 = x;
                y1 = y;
                outcode1 = compute_outcode(ctx, x1, y1);
            }
        }
    }

    if (!accept) {
        return;
    }

    int16_t dx = gfx_abs(x1 - x0);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t dy = -gfx_abs(y1 - y0); 
    int16_t sy = (y0 < y1) ? 1 : -1;
    
    int16_t err = dx + dy; 
    int16_t e2;

    // Флаг ориентации линии: если dx больше абсолютного dy, линия пологая
    bool is_horizontal = (dx > -dy);

    // Отрисовка концов ("caps"), если они не были отсечены (чтобы не было пробелов на стыках).
    if (thickness == 2) {
        if (x0 == orig_x0 && y0 == orig_y0) {
            gfx_draw_pixel(ctx, orig_x0, orig_y0);
            gfx_draw_pixel(ctx, orig_x0 + 1, orig_y0);
            gfx_draw_pixel(ctx, orig_x0, orig_y0 + 1);
            gfx_draw_pixel(ctx, orig_x0 + 1, orig_y0 + 1);
        }
        if (x1 == orig_x1 && y1 == orig_y1) {
            gfx_draw_pixel(ctx, orig_x1, orig_y1);
            gfx_draw_pixel(ctx, orig_x1 + 1, orig_y1);
            gfx_draw_pixel(ctx, orig_x1, orig_y1 + 1);
            gfx_draw_pixel(ctx, orig_x1 + 1, orig_y1 + 1);
        }
    } else if (thickness == 3) {
        if (x0 == orig_x0 && y0 == orig_y0) {
            for (int16_t oy = -1; oy <= 1; ++oy) {
                for (int16_t ox = -1; ox <= 1; ++ox) {
                    gfx_draw_pixel(ctx, orig_x0 + ox, orig_y0 + oy);
                }
            }
        }
        if (x1 == orig_x1 && y1 == orig_y1) {
            for (int16_t oy = -1; oy <= 1; ++oy) {
                for (int16_t ox = -1; ox <= 1; ++ox) {
                    gfx_draw_pixel(ctx, orig_x1 + ox, orig_y1 + oy);
                }
            }
        }
    }

    while (true) {
        // Отрисовка с программным отсечением (gfx_draw_pixel)
        if (thickness == 2) {
            gfx_draw_pixel(ctx, x0, y0);
            if (is_horizontal) {
                gfx_draw_pixel(ctx, x0, y0 + 1);
            } else {
                gfx_draw_pixel(ctx, x0 + 1, y0);
            }
        } else if (thickness == 3) {
            gfx_draw_pixel(ctx, x0, y0);
            if (is_horizontal) {
                gfx_draw_pixel(ctx, x0, y0 - 1);
                gfx_draw_pixel(ctx, x0, y0 + 1);
            } else {
                gfx_draw_pixel(ctx, x0 - 1, y0);
                gfx_draw_pixel(ctx, x0 + 1, y0);
            }
        } else {
            int16_t half_thick = thickness >> 1;
            int16_t start_offset = -half_thick;
            int16_t end_offset = start_offset + thickness - 1;
            for (int16_t offset = start_offset; offset <= end_offset; ++offset) {
                if (is_horizontal) {
                    gfx_draw_pixel(ctx, x0, y0 + offset);
                } else {
                    gfx_draw_pixel(ctx, x0 + offset, y0);
                }
            }
        }

        if (x0 == x1 && y0 == y1) {
            break;
        }

        e2 = err << 1;
        
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void gfx_draw_dotted_line(gfx_context_t *ctx, int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    if (ctx == NULL || ctx->draw_pixel == NULL) return;

    // Сохраняем геометрическое начало линии для синхронизации фазы
    int16_t orig_x0 = x0;
    int16_t orig_y0 = y0;

    // 1. Отсечение отрезка (Cohen-Sutherland)
    uint8_t outcode0 = compute_outcode(ctx, x0, y0);
    uint8_t outcode1 = compute_outcode(ctx, x1, y1);
    bool accept = false;

    while (true) {
        if (!(outcode0 | outcode1)) {
            accept = true;
            break;
        } else if (outcode0 & outcode1) {
            break;
        } else {
            int16_t x, y;
            uint8_t outcodeOut = outcode0 ? outcode0 : outcode1;

            int32_t dx = x1 - x0;
            int32_t dy = y1 - y0;

            if (outcodeOut & TOP) {
                x = x0 + dx * (ctx->height - 1 - y0) / dy;
                y = ctx->height - 1;
            } else if (outcodeOut & BOTTOM) {
                x = x0 + dx * (0 - y0) / dy;
                y = 0;
            } else if (outcodeOut & RIGHT) {
                y = y0 + dy * (ctx->width - 1 - x0) / dx;
                x = ctx->width - 1;
            } else if (outcodeOut & LEFT) {
                y = y0 + dy * (0 - x0) / dx;
                x = 0;
            }

            if (outcodeOut == outcode0) {
                x0 = x;
                y0 = y;
                outcode0 = compute_outcode(ctx, x0, y0);
            } else {
                x1 = x;
                y1 = y;
                outcode1 = compute_outcode(ctx, x1, y1);
            }
        }
    }

    if (!accept) {
        return;
    }

    // Вычисляем количество пропущенных пикселей (Chebyshev distance)
    int16_t dx_clip = gfx_abs(x0 - orig_x0);
    int16_t dy_clip = gfx_abs(y0 - orig_y0);
    int16_t clipped_steps = (dx_clip > dy_clip) ? dx_clip : dy_clip;

    // Инициализируем счетчик с учетом отсеченной части (по модулю 2, через & 1)
    int16_t dot_counter = clipped_steps & 1;

    // 2. Отрисовка видимой части (Bresenham optimized)
    int16_t dx = gfx_abs(x1 - x0);
    int16_t sx = x0 < x1 ? 1 : -1;
    int16_t dy = -gfx_abs(y1 - y0);
    int16_t sy = y0 < y1 ? 1 : -1;

    int16_t err = dx + dy;
    int16_t e2;

    while (true) {
        // Чередование: 1 пиксель рисуем, 1 пропускаем
        if (dot_counter == 0) {
            gfx_draw_pixel(ctx, x0, y0);
        }

        dot_counter = (dot_counter + 1) & 1;

        if (x0 == x1 && y0 == y1) {
            break;
        }

        e2 = err << 1;

        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void gfx_draw_railway_line(gfx_context_t *ctx, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color_dark, uint8_t color_light)
{
    if (ctx == NULL || ctx->draw_pixel == NULL) return;

    // Сохраняем геометрическое начало линии для синхронизации фазы
    int16_t orig_x0 = x0;
    int16_t orig_y0 = y0;

    // 1. Отсечение отрезка (Cohen-Sutherland)
    uint8_t outcode0 = compute_outcode(ctx, x0, y0);
    uint8_t outcode1 = compute_outcode(ctx, x1, y1);
    bool accept = false;

    while (true) {
        if (!(outcode0 | outcode1)) {
            accept = true;
            break;
        } else if (outcode0 & outcode1) {
            break;
        } else {
            int16_t x, y;
            uint8_t outcodeOut = outcode0 ? outcode0 : outcode1;

            int32_t dx = x1 - x0;
            int32_t dy = y1 - y0;

            if (outcodeOut & TOP) {
                x = x0 + dx * (ctx->height - 1 - y0) / dy;
                y = ctx->height - 1;
            } else if (outcodeOut & BOTTOM) {
                x = x0 + dx * (0 - y0) / dy;
                y = 0;
            } else if (outcodeOut & RIGHT) {
                y = y0 + dy * (ctx->width - 1 - x0) / dx;
                x = ctx->width - 1;
            } else if (outcodeOut & LEFT) {
                y = y0 + dy * (0 - x0) / dx;
                x = 0;
            }

            if (outcodeOut == outcode0) {
                x0 = x;
                y0 = y;
                outcode0 = compute_outcode(ctx, x0, y0);
            } else {
                x1 = x;
                y1 = y;
                outcode1 = compute_outcode(ctx, x1, y1);
            }
        }
    }

    if (!accept) {
        return;
    }

    // Параметры паттерна железной дороги
    const int16_t dark_len = 4;
    const int16_t light_len = 4;
    const int16_t pattern_len = dark_len + light_len; // 8 пикселей

    // Вычисляем количество пропущенных пикселей (Chebyshev distance)
    int16_t dx_clip = gfx_abs(x0 - orig_x0);
    int16_t dy_clip = gfx_abs(y0 - orig_y0);
    int16_t clipped_steps = (dx_clip > dy_clip) ? dx_clip : dy_clip;

    // Инициализируем счетчик с учетом отсеченной части.
    // Так как pattern_len = 8 (степень двойки), используем & 7.
    int16_t rail_counter = clipped_steps & 7;

    // 2. Отрисовка видимой части (Bresenham optimized)
    int16_t dx = gfx_abs(x1 - x0);
    int16_t sx = x0 < x1 ? 1 : -1;
    int16_t dy = -gfx_abs(y1 - y0);
    int16_t sy = y0 < y1 ? 1 : -1;

    int16_t err = dx + dy;
    int16_t e2;

    while (true) {
        // Используем gfx_draw_pixel через подмену цвета, так как она
        // безопасно делает clipping внутри или обращается напрямую
        gfx_color_t old_color = ctx->color_fg;
        ctx->color_fg = (rail_counter < dark_len) ? color_dark : color_light;
        gfx_draw_pixel(ctx, x0, y0);
        ctx->color_fg = old_color;

        rail_counter = (rail_counter + 1) & 7;

        if (x0 == x1 && y0 == y1) {
            break;
        }

        e2 = err << 1;

        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}