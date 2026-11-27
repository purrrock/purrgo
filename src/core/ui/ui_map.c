#include "purrgo/app_ui.h"
#include "purrgo/ui/ui_dirty.h"
#include "ui_map.h"
#include "purrgo/app_fsm.h"
#include "purrgo/map_controller.h"
#include "purrgo/gfx_rect.h"
#include "purrgo/gfx_text.h"
#include "purrgo/geo.h"
#include "purrgo/map.h"
#include "purrgo/track_renderer.h"
#include "purrgo/route.h"
#include "purrgo/waypoint.h"
#include "purrgo/wpt_layer.h"
#include "purrgo/track_logger.h"
#include "purrgo/config.h"
#include "purrgo/logger.h"
#include "purrgo/hardware_config.h"
#include "purrgo/display_hal.h"
#include "purrgo/purrgo_time.h"
#include "purrgo/battery.h"
#include "purrgo/supply.h"
#include "purrgo/gfx_icon.h"
#include "purrgo/purrgo_poi_icons.h"

/* Номер иконки перекрестия центра карты в purrgo_poi_icons (src/core/map/purrgo_poi_icons.c). */
#ifndef PURRGO_MAP_CROSSHAIR_ICON
#define PURRGO_MAP_CROSSHAIR_ICON PURRGO_UI_ICON_MAP_CROSSHAIR
#endif
_Static_assert(PURRGO_MAP_CROSSHAIR_ICON < PURRGO_POI_ICON_COUNT, "crosshair icon out of range");

typedef struct {
    bool rendered;
    int16_t min_x;
    int16_t max_x;
    int16_t min_y;
    int16_t max_y;
    bool gnss_valid;
    bool course_valid;
    int16_t course_deg;
    int32_t lat_1e7;
    int32_t lon_1e7;
    int16_t cx;     /* центр маркёра на экране, px */
    int16_t cy;

    gfx_point_t pts[8];
    uint8_t point_count;
    bool filled;
} marker_state_t;

static marker_state_t prev_marker_state;
static bool prev_marker_state_valid = false;

/* Номер последней точки трека, отрисованной на экране (purrgo_logger_get_track_serial()). */
static uint32_t s_track_drawn_serial = 0;

typedef struct {
    uint8_t minutes;
    uint8_t hours;
    bool time_valid;        /* время известно: иначе в строке прочерки */
    bool gnss_valid;
    track_logger_state_t rec_state;
    bool valid;
} status_bar_state_t;

static status_bar_state_t prev_status_state = {0};

/*
 * Кэш фона под динамической частью карты (маркёр GNSS и "хвост" трека от последней
 * записанной точки до маркёра), чтобы не перерисовывать всю карту при каждом движении
 * маркёра. Хвост может быть длинным (хорда трека до 250 м, в режиме EXPEDITION до 1 км),
 * поэтому кэш рассчитан на всё окно карты. Пиксель - 2 бита (gfx_color_t: 0..3, четыре
 * градации серого), окно 176 x 246 px занимает около 10,8 КБ.
 */
#define OVERLAY_BG_W PURRGO_HW_DISPLAY_WIDTH_PX
#define OVERLAY_BG_H PURRGO_MAP_VIEWPORT_HEIGHT_PX

static struct {
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
    bool valid;
    uint8_t packed[(OVERLAY_BG_W * OVERLAY_BG_H + 3) / 4];
} marker_bg_cache = {0};

/* Нарисован ли сейчас хвост трека (его нужно стереть при следующем обновлении). */
static bool s_tail_drawn = false;

//static void log_marker_diagnostic(const char* reason, const marker_state_t* m) {
//    if (m->rendered) {
      //  PURRGO_LOG(
      //      "%s | valid: %d | pos: %ld,%ld | course_valid: %d | course: %d | bbox: %d,%d -> %d,%d\n\r",
      //      reason, m->gnss_valid, (long)m->lat_1e7, (long)m->lon_1e7,
      //      m->course_valid, m->course_valid ? m->course_deg : 0,
      //      m->min_x, m->min_y, m->max_x, m->max_y
      //  );
//    } else {
      //  PURRGO_LOG(
      //      "%s | valid: %d | pos: %ld,%ld | course_valid: %d | course: %d | NOT RENDERED\n\r",
      //      reason, m->gnss_valid, (long)m->lat_1e7, (long)m->lon_1e7,
      //      m->course_valid, m->course_valid ? m->course_deg : 0
      //  );
//    }
//}

#include "purrgo/gfx_polygon.h"
#include "purrgo/trig.h"
#include "map/map_projection.h"
#include "purrgo/purrgo_format.h"
#include <string.h>

/*
 * Размер маркёра, px (нечётный: у фигуры есть центральный пиксель). Был 10.
 * Кэш фона (marker_bg_cache) рассчитан на всё окно карты и от размера маркёра не зависит.
 */
#define PURRGO_MAP_POS_MARK_SIZE_PX 13

/*
 * Порог перерисовки маркёра GNSS (E-Ink: каждое обновление области - это
 * частичный refresh дисплея, поэтому мелкие дрожания позиции не рисуем).
 * Маркёр не обновляется, пока его центр на экране сместился относительно
 * последнего НАРИСОВАННОГО положения не более чем на это число пикселей по
 * каждой из осей (сравнивается с нарисованным положением, а не с предыдущим
 * отчётом приёмника, поэтому смещения накапливаются и маркёр не "залипает").
 */
#ifndef PURRGO_MAP_MARKER_MIN_SHIFT_PX
#define PURRGO_MAP_MARKER_MIN_SHIFT_PX 3
#endif

/*
 * Минимальное изменение курса (градусы), при котором маркёр-стрелка
 * перерисовывается, если его положение не изменилось. 0 - перерисовывать при
 * любом изменении курса на 1 градус.
 */
#ifndef PURRGO_MAP_MARKER_MIN_COURSE_DELTA_DEG
#define PURRGO_MAP_MARKER_MIN_COURSE_DELTA_DEG 10
#endif
#define PURRGO_MAP_POS_MARK_DIRECTION_HALF_WIDTH_PX 5

extern int dbg_map_render_calls;

/* Деление на 10000 с округлением к ближайшему (а не усечением к нулю). */
static int32_t div_round_10k(int32_t v) {
    return (v >= 0) ? ((v + 5000) / 10000) : -((-v + 5000) / 10000);
}

/*
 * Calculates the marker geometry using integer math based on GNSS state,
 * projects the geographical position to screen, checks if the entire shape's
 * bounding box falls inside the viewport, and renders it in black.
 */
/*
 * Render the GNSS position marker.
 *
 * Marker states:
 *   1. Valid position + valid course:
 *      Filled narrow isosceles triangle pointing in the current course.
 *
 *   2. Valid position + invalid course:
 *      Filled black equilateral triangle (pointing up). Same shape as the
 *      no-fix marker, but filled: the position is valid, but the direction
 *      of movement is unknown.
 *
 *   3. Invalid position:
 *      Outline equilateral triangle at the configured last position.
 *
 * All geometry is calculated using integer arithmetic.
 *
 * The marker is rendered only when its complete bounding box fits inside
 * the map viewport. gfx clipping is intentionally not used as a substitute
 * for this visibility test.
 */
static void ui_calc_marker_state(
    const purrgo_gnss_solution_t* gnss,
    const purrgo_viewport_t* map_vp,
    const purrgo_bbox_t* dynamic_cam,
    marker_state_t* out_state)
{
    int32_t lat;
    int32_t lon;

    out_state->gnss_valid = gnss->valid;
    out_state->course_valid = gnss->course_valid;
    out_state->course_deg = (int16_t)((gnss->course_deg_100 / 100) % 360);
    out_state->rendered = false;

    /*
     * Use the current GNSS position when a valid fix exists.
     * Otherwise use the persistent configured fallback position.
     */
    if (gnss->valid) {
        lat = gnss->lat_1e7;
        lon = gnss->lon_1e7;
    } else {
        lat = app_config.last_lat_1e7;
        lon = app_config.last_lon_1e7;
    }

    out_state->lat_1e7 = lat;
    out_state->lon_1e7 = lon;

    /*
     * Project the geographic position using the existing integer projection.
     */
    int16_t cx;
    int16_t cy;

    project_to_screen(
        lon,
        lat,
        dynamic_cam,
        map_vp,
        &cx,
        &cy
    );
    out_state->cx = cx;
    out_state->cy = cy;

    const int32_t size = PURRGO_MAP_POS_MARK_SIZE_PX;
    const int32_t half_size = size / 2;

    /*
     * Every marker shape is a triangle (3 vertices).
     */
    if (gnss->valid && gnss->course_valid) {
        /*
         * Valid position and valid course:
         *
         * Local triangle:
         *       (0, -half_size)
         *          /\
         *         /  \
         *        /    \
         *   (-w, +half_size) (+w, +half_size)
         *
         * The marker is intentionally narrower than before:
         * half width = 4 px instead of 6 px.
         */
        const int32_t half_width =
            PURRGO_MAP_POS_MARK_DIRECTION_HALF_WIDTH_PX;

        const int32_t top_y = -half_size;
        const int32_t bottom_y = half_size;

        const int32_t local_x[3] = {
            0,
            -half_width,
            half_width
        };

        const int32_t local_y[3] = {
            top_y,
            bottom_y,
            bottom_y
        };

        /*
         * course_deg_100 is degrees * 100.
         *
         * The marker is intentionally rotated using whole degrees.
         * At a 12 px marker size, the additional 1/100 degree precision
         * has no meaningful effect on the rasterized result.
         */
        int32_t s = purrgo_sin_10k(out_state->course_deg);
        int32_t c = purrgo_cos_10k(out_state->course_deg);

        for (uint8_t i = 0; i < 3; i++) {
            /*
             * Rotate local coordinates around the marker center.
             *
             * sin/cos are represented as integer values scaled by 10000.
             */
            out_state->pts[i].x = (int16_t)(
                cx + div_round_10k(local_x[i] * c - local_y[i] * s)
            );

            out_state->pts[i].y = (int16_t)(
                cy + div_round_10k(local_x[i] * s + local_y[i] * c)
            );
        }

        out_state->point_count = 3;
        out_state->filled = true;

    } else if (gnss->valid) {
        /*
         * Valid position but invalid course:
         *
         * Filled equilateral triangle pointing up. The geometry is the
         * same as for the no-fix marker below, only filled.
         */
        const int32_t w =
            (size * 5774 + 5000) / 10000;

        const int32_t top_y = -half_size;
        const int32_t bottom_y = half_size;

        out_state->pts[0].x = cx;
        out_state->pts[0].y = (int16_t)(cy + top_y);

        out_state->pts[1].x = (int16_t)(cx - w);
        out_state->pts[1].y = (int16_t)(cy + bottom_y);

        out_state->pts[2].x = (int16_t)(cx + w);
        out_state->pts[2].y = (int16_t)(cy + bottom_y);

        out_state->point_count = 3;
        out_state->filled = true;

    } else {
        /*
         * No valid GNSS fix:
         *
         * Show the configured last position using an outline
         * equilateral triangle. No course information is used.
         */
        const int32_t w =
            (size * 5774 + 5000) / 10000;

        const int32_t top_y = -half_size;
        const int32_t bottom_y = half_size;

        out_state->pts[0].x = cx;
        out_state->pts[0].y = (int16_t)(cy + top_y);

        out_state->pts[1].x = (int16_t)(cx - w);
        out_state->pts[1].y = (int16_t)(cy + bottom_y);

        out_state->pts[2].x = (int16_t)(cx + w);
        out_state->pts[2].y = (int16_t)(cy + bottom_y);

        out_state->point_count = 3;
        out_state->filled = false;
    }

    /*
     * Determine the complete marker bounding box.
     */
    int16_t min_x = (int16_t)out_state->pts[0].x;
    int16_t max_x = (int16_t)out_state->pts[0].x;
    int16_t min_y = (int16_t)out_state->pts[0].y;
    int16_t max_y = (int16_t)out_state->pts[0].y;

    for (uint8_t i = 1; i < out_state->point_count; i++) {
        if (out_state->pts[i].x < min_x) {
            min_x = (int16_t)out_state->pts[i].x;
        }

        if (out_state->pts[i].x > max_x) {
            max_x = (int16_t)out_state->pts[i].x;
        }

        if (out_state->pts[i].y < min_y) {
            min_y = (int16_t)out_state->pts[i].y;
        }

        if (out_state->pts[i].y > max_y) {
            max_y = (int16_t)out_state->pts[i].y;
        }
    }

    out_state->min_x = min_x;
    out_state->max_x = max_x;
    out_state->min_y = min_y;
    out_state->max_y = max_y;

    /*
     * Do not render a partially visible marker.
     *
     * Every vertex must fit completely inside the map viewport.
     */
    if (min_x < map_vp->offset_x ||
        max_x >= map_vp->offset_x + map_vp->width ||
        min_y < map_vp->offset_y ||
        max_y >= map_vp->offset_y + map_vp->height) {
        return;
    }

    out_state->rendered = true;
}

/*
 * Область под маркёром и хвостом: рамка маркёра с запасом 2 px (антиалиасинг, округление),
 * объединённая с рамкой хвоста (tail - NULL, если хвоста нет), обрезанная окном карты.
 * false - рисовать нечего (маркёр и хвост вне окна).
 */
static bool ui_overlay_rect(const marker_state_t* m, const purrgo_rect_t* tail, purrgo_rect_t* out) {
    const int32_t margin = 2;
    int32_t x0 = INT32_MAX, y0 = INT32_MAX, x1 = INT32_MIN, y1 = INT32_MIN;

    if (m->rendered) {
        x0 = m->min_x - margin;
        y0 = m->min_y - margin;
        x1 = m->max_x + margin;
        y1 = m->max_y + margin;
    }
    if (tail != NULL) {
        if (tail->x - 1 < x0) x0 = tail->x - 1;
        if (tail->y - 1 < y0) y0 = tail->y - 1;
        if (tail->x + tail->w > x1) x1 = tail->x + tail->w;
        if (tail->y + tail->h > y1) y1 = tail->y + tail->h;
    }
    if (x0 > x1 || y0 > y1) {
        return false;
    }

    /* Границы окна карты (offset_y = строка статуса, высота = экран без двух строк статуса). */
    const int32_t vp_y0 = PURRGO_MAP_STATUS_BAR_H_PX;
    const int32_t vp_y1 = PURRGO_MAP_STATUS_BAR_H_PX + PURRGO_MAP_VIEWPORT_HEIGHT_PX - 1;

    if (x0 < 0) x0 = 0;
    if (y0 < vp_y0) y0 = vp_y0;
    if (x1 > PURRGO_HW_DISPLAY_WIDTH_PX - 1) x1 = PURRGO_HW_DISPLAY_WIDTH_PX - 1;
    if (y1 > vp_y1) y1 = vp_y1;
    if (x0 > x1 || y0 > y1) {
        return false;
    }

    out->x = (int16_t)x0;
    out->y = (int16_t)y0;
    out->w = (int16_t)(x1 - x0 + 1);
    out->h = (int16_t)(y1 - y0 + 1);
    return true;
}

/* Сохраняет фон области (NULL - нечего сохранять, кэш становится пустым). */
static void ui_save_overlay_bg(gfx_context_t* gfx, const purrgo_rect_t* area) {
    if (area == NULL || area->w <= 0 || area->h <= 0 ||
        area->w > OVERLAY_BG_W || area->h > OVERLAY_BG_H) {
        marker_bg_cache.valid = false;
        return;
    }

    marker_bg_cache.x = area->x;
    marker_bg_cache.y = area->y;
    marker_bg_cache.w = area->w;
    marker_bg_cache.h = area->h;
    marker_bg_cache.valid = true;

    const size_t total = (size_t)area->w * (size_t)area->h;
    memset(marker_bg_cache.packed, 0, (total + 3U) / 4U);

    /*
     * Область читается напрямую, в обход текущей области отсечения gfx: при частичном
     * обновлении она может быть сужена. Прямоугольник уже ограничен окном карты.
     */
    int16_t old_clip_x = gfx->clip_x;
    int16_t old_clip_y = gfx->clip_y;
    int16_t old_clip_w = gfx->clip_w;
    int16_t old_clip_h = gfx->clip_h;
    gfx_reset_clip(gfx);

    size_t idx = 0;
    for (int16_t cy = 0; cy < area->h; cy++) {
        for (int16_t cx = 0; cx < area->w; cx++, idx++) {
            const uint8_t v = (uint8_t)(gfx_read_pixel(gfx, (int16_t)(area->x + cx),
                                                       (int16_t)(area->y + cy)) & 0x03U);
            marker_bg_cache.packed[idx >> 2] |= (uint8_t)(v << ((idx & 3U) * 2U));
        }
    }

    gfx_set_clip(gfx, old_clip_x, old_clip_y, old_clip_w, old_clip_h);
}

static void ui_restore_marker_bg(gfx_context_t* gfx) {
    if (!marker_bg_cache.valid) return;

    gfx_color_t old_fg = gfx->color_fg;

    int16_t old_clip_x = gfx->clip_x;
    int16_t old_clip_y = gfx->clip_y;
    int16_t old_clip_w = gfx->clip_w;
    int16_t old_clip_h = gfx->clip_h;
    gfx_reset_clip(gfx);

    size_t idx = 0;
    for (int16_t cy = 0; cy < marker_bg_cache.h; cy++) {
        for (int16_t cx = 0; cx < marker_bg_cache.w; cx++, idx++) {
            gfx->color_fg = (gfx_color_t)((marker_bg_cache.packed[idx >> 2] >> ((idx & 3U) * 2U)) & 0x03U);
            gfx_draw_pixel(gfx, (int16_t)(marker_bg_cache.x + cx), (int16_t)(marker_bg_cache.y + cy));
        }
    }

    gfx_set_clip(gfx, old_clip_x, old_clip_y, old_clip_w, old_clip_h);

    gfx->color_fg = old_fg;
}

static void ui_draw_marker(gfx_context_t* gfx, const marker_state_t* state) {
    if (!state->rendered) return;

    /*
     * Preserve the caller's foreground colour.
     */
    gfx_color_t old_fg = gfx->color_fg;

    if (state->filled) {
        /*
         * Чёрная заливка строго внутри белой рамки: обычные fill + outline
         * растеризуют края по-разному и заливка местами выходила за рамку.
         */
        gfx_draw_polygon_bordered(gfx, state->pts, state->point_count, BLACK, WHITE);
    } else {
        gfx_set_color(gfx, BLACK, gfx->color_bg);
        gfx_draw_polygon(gfx, state->pts, state->point_count);
    }

    gfx_set_color(gfx, old_fg, gfx->color_bg);
}
static int16_t get_string_width(const char* str) {
    int len = 0;
    while (str[len] != '\0') len++;
    return (int16_t)(len * 6); // 5x7 font + 1px spacing
}

static void ui_update_status_bar(gfx_context_t* gfx, const purrgo_gnss_solution_t* gnss, int16_t status_h) {
    if (purrgo_app_ui_is_power_off()) {
        /* Выключение: инверсная строка "POWER OFF", масштаб карты остаётся справа. */
        const char* off_scale_label = map_app_get_map_scale_label();
        int16_t off_scale_x = (int16_t)(PURRGO_HW_DISPLAY_WIDTH_PX - (int)(strlen(off_scale_label) * 6) - 2);

        gfx_set_color(gfx, WHITE, BLACK);
        gfx_fill_rect(gfx, 0, 0, PURRGO_HW_DISPLAY_WIDTH_PX, status_h);
        gfx_draw_string(gfx, 2, 1, "POWER OFF");
        gfx_draw_string(gfx, off_scale_x, 1, off_scale_label);
        gfx_set_color(gfx, BLACK, WHITE);
        prev_status_state.valid = true;
        return;
    }

    /*
     * Местное время. Если время неизвестно (приёмник его не знает или замолчал) или его
     * нельзя пересчитать в местное, показываются прочерки: нули или последнее известное
     * время вводили бы в заблуждение. Показывать UTC вместо местного тоже нельзя.
     */
    purrgo_gnss_solution_t display_fix;
    const bool have_time = gnss->time_valid &&
                           purrgo_time_apply_timezone(gnss, &display_fix, app_config.tz_offset_minutes);

    status_bar_state_t new_state;
    new_state.time_valid = have_time;
    new_state.minutes = have_time ? display_fix.minutes : 0U;
    new_state.hours = have_time ? display_fix.hours : 0U;
    new_state.gnss_valid = gnss->valid;
    new_state.rec_state = purrgo_logger_get_state();
    new_state.valid = true;

    // Clear top status area
    gfx_set_color(gfx, 0, 3);
    gfx_fill_rect(gfx, 0, 0, PURRGO_HW_DISPLAY_WIDTH_PX, status_h);

    gfx_set_color(gfx, 0, 3);

    int16_t next_x = 2;

    char time_str[8];
    if (new_state.time_valid) {
        purrgo_snprintf(time_str, sizeof(time_str), "%02d:%02d", new_state.hours, new_state.minutes);
    } else {
        purrgo_snprintf(time_str, sizeof(time_str), "--:--");
    }
    gfx_draw_string(gfx, next_x, 1, time_str);
    next_x = (int16_t)(next_x + get_string_width(time_str) + 6);

    const char* fix_str = new_state.gnss_valid ? "FIX" : "NO-FIX";
    gfx_draw_string(gfx, next_x, 1, fix_str);
    next_x = (int16_t)(next_x + get_string_width(fix_str) + 6);

    const char* rec_str = "NO-REC";
    if (new_state.rec_state == LOGGER_STATE_RECORDING) rec_str = "REC";
    else if (new_state.rec_state == LOGGER_STATE_ERROR) rec_str = "ERR-REC";

    /*
     * Запись остановлена из-за низкого питания (purrgo/supply.h): вместо статуса
     * записи постоянный инверсный индикатор с причиной - STOP:VDD, STOP:BAT или
     * STOP:B+V. Заметен и пока трек ещё виден на экране.
     */
    char stop_str[12];
    const bool supply_stop = purrgo_supply_storage_blocked();
    if (supply_stop) {
        purrgo_snprintf(stop_str, sizeof(stop_str), "STOP:%s", purrgo_supply_reason_text(true));
        rec_str = stop_str;
        gfx_set_color(gfx, WHITE, BLACK);
        gfx_fill_rect(gfx, (int16_t)(next_x - 2), 0, (int16_t)(get_string_width(rec_str) + 3), status_h);
    }
    gfx_draw_string(gfx, next_x, 1, rec_str);
    if (supply_stop) {
        gfx_set_color(gfx, 0, 3);
    }

    const char* scale_label = map_app_get_map_scale_label();
    int16_t scale_x = (int16_t)(PURRGO_HW_DISPLAY_WIDTH_PX - (int)(strlen(scale_label) * 6) - 2);
    gfx_draw_string(gfx, scale_x, 1, scale_label);

    /*
     * Заряд батареи, %: между статусом записи и масштабом, прижат к масштабу.
     * Если в строке не хватает места (длинный масштаб + ERR-REC + NO-FIX),
     * заряд не рисуется. Текст ("85%", "5V", "--%") даёт общий монитор
     * заряда, тот же, что и в строке статуса читалки (purrgo/battery.h).
     * При остановке записи по питанию заряд не рисуется: его заменяет индикатор STOP.
     */
    const char* bat_str = purrgo_battery_status_text();
    const int16_t rec_end_x = (int16_t)(next_x + get_string_width(rec_str));
    const int16_t bat_x = (int16_t)(scale_x - 6 - get_string_width(bat_str));
    if (!supply_stop && bat_x >= rec_end_x + 4) {
        gfx_draw_string(gfx, bat_x, 1, bat_str);
    }


    if (prev_status_state.valid && !purrgo_ui_dirty_test(UI_DIRTY_MAP)) {
        display_refresh_region(0, 0, PURRGO_HW_DISPLAY_WIDTH_PX, status_h);
    }
    prev_status_state = new_state;
}


static void ui_map_setup_viewport(purrgo_viewport_t* map_vp, purrgo_bbox_t* dynamic_cam) {
    map_vp->width = PURRGO_HW_DISPLAY_WIDTH_PX;
    map_vp->height = PURRGO_MAP_VIEWPORT_HEIGHT_PX;
    map_vp->offset_x = 0;
    map_vp->offset_y = PURRGO_MAP_STATUS_BAR_H_PX;

    int32_t center_lat = map_app_get_map_center_lat();
    int32_t center_lon = map_app_get_map_center_lon();
    uint32_t width_m = map_app_get_map_scale_width_m();

    purrgo_geo_bbox_from_center(center_lat, center_lon, width_m, map_vp, dynamic_cam);
}

/* Параметры рендера карты из конфига и контроллера (ядро карты их не читает). */
static void ui_map_fill_render_params(purrgo_map_render_params_t* params) {
    params->map_dir = app_config.map_dir;
    params->scale = map_app_get_map_zoom_level();
    params->details = app_config.map_details;
    params->poi_mode = app_config.poi_mode;
    params->layer_landuse = purrgo_layer_on(app_config.layers, PURRGO_LAYER_LANDUSE);
    params->layer_landuse_labels = purrgo_layer_on(app_config.layers, PURRGO_LAYER_LANDUSE_LABELS);
    params->layer_water = purrgo_layer_on(app_config.layers, PURRGO_LAYER_WATER);
    params->layer_water_labels = purrgo_layer_on(app_config.layers, PURRGO_LAYER_WATER_LABELS);
    params->layer_roads = purrgo_layer_on(app_config.layers, PURRGO_LAYER_ROADS);
    params->layer_poi = purrgo_layer_on(app_config.layers, PURRGO_LAYER_POI);
    params->layer_poi_labels = purrgo_layer_on(app_config.layers, PURRGO_LAYER_POI_LABELS);
}

static bool ui_map_render_base_layers(gfx_context_t* gfx, const purrgo_viewport_t* map_vp, const purrgo_bbox_t* dynamic_cam) {
    // Clear map viewport
    gfx_set_color(gfx, 0, 3);
    gfx_fill_rect(gfx, map_vp->offset_x, map_vp->offset_y, (int16_t)map_vp->width, (int16_t)map_vp->height);

    // Limit rendering strictly to the map viewport to protect the status bars
    gfx_set_clip(gfx, map_vp->offset_x, map_vp->offset_y, (int16_t)map_vp->width, (int16_t)map_vp->height);

    purrgo_map_render_params_t params;
    ui_map_fill_render_params(&params);
    return purrgo_map_render_viewport(gfx, map_vp, dynamic_cam, &params);
}

/* Рисуется ли сейчас активный трек: идёт запись и слой трека включён. */
static bool ui_map_track_visible(void) {
    return purrgo_logger_get_active_filename() != NULL &&
           purrgo_layer_on(app_config.layers, PURRGO_LAYER_TRACK);
}

static void ui_map_render_dynamic_data(gfx_context_t* gfx, const purrgo_viewport_t* map_vp, const purrgo_bbox_t* dynamic_cam) {
    /* Порядок слоёв: route, waypoints, track - см. docs/architecture.md, Layer drawing order. */
    if (purrgo_layer_on(app_config.layers, PURRGO_LAYER_ROUTE)) {
        if (purrgo_route_sync_src(app_config.route_file,
                                  app_config.route_from_tracks ? PURRGO_ROUTE_SRC_TRACKS
                                                               : PURRGO_ROUTE_SRC_ROUTES)) {
            purrgo_route_render(gfx, dynamic_cam, map_vp);
        }
    }

    /* Путевые точки: над маршрутом, под треком. */
    if (purrgo_layer_on(app_config.layers, PURRGO_LAYER_WAYPOINT)) {
        /* Файл записанного трека точек <wpt> не содержит: берутся только WAYPTS. */
        if (purrgo_wpt_layer_sync(app_config.route_from_tracks ? "" : app_config.route_file)) {
            purrgo_wpt_layer_render(gfx, dynamic_cam, map_vp);
        }
    }

    if (ui_map_track_visible()) {
        purrgo_track_render(gfx, dynamic_cam, map_vp, purrgo_logger_get_active_filename());
    }

    /* Цель навигации: поверх маршрута и трека, под маркёром GNSS. */
    purrgo_wpt_render(gfx, dynamic_cam, map_vp);

    /*
     * Перекрестие в центре карты - только при выключенном автоследовании (карта
     * двигается кнопками): это точка, которую берёт WAYPOINT > MAP CENTER.
     * Рисуется на полной перерисовке карты; смена режима её запрашивает
     * (purrgo_map_controller_handle_button()).
     */
    if (map_app_is_manual_pan_active()) {
        int16_t cx, cy;
        project_to_screen(map_app_get_map_center_lon(), map_app_get_map_center_lat(),
                          dynamic_cam, map_vp, &cx, &cy);
        gfx_draw_icon_11x11(gfx, cx, cy, purrgo_poi_icons[PURRGO_MAP_CROSSHAIR_ICON]);
    }
    /* Все точки, имеющиеся сейчас, нарисованы (или трек не показывается). */
    s_track_drawn_serial = purrgo_logger_get_track_serial();
    s_tail_drawn = false;
}

/*
 * Нужно ли перерисовывать маркёр: prev - то, что сейчас нарисовано на экране.
 * Смена состояния (виден/не виден, есть фикс/нет, есть курс/нет) - всегда.
 * Иначе - только если центр сместился больше чем на PURRGO_MAP_MARKER_MIN_SHIFT_PX
 * по любой оси либо курс изменился не меньше чем на
 * PURRGO_MAP_MARKER_MIN_COURSE_DELTA_DEG.
 */
static bool ui_marker_needs_redraw(const marker_state_t* prev, const marker_state_t* cur) {
    if (cur->rendered != prev->rendered ||
        cur->gnss_valid != prev->gnss_valid ||
        cur->course_valid != prev->course_valid) {
        return true;
    }
    if (!cur->rendered) {
        return false;   /* оба невидимы: рисовать нечего */
    }

    int32_t dx = (int32_t)cur->cx - (int32_t)prev->cx;
    int32_t dy = (int32_t)cur->cy - (int32_t)prev->cy;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    if (dx > PURRGO_MAP_MARKER_MIN_SHIFT_PX || dy > PURRGO_MAP_MARKER_MIN_SHIFT_PX) {
        return true;
    }

    if (cur->course_valid) {
        int32_t dc = (int32_t)cur->course_deg - (int32_t)prev->course_deg;
        if (dc < 0) dc = -dc;
        if (dc > 180) dc = 360 - dc;
        if (dc > 0 && dc >= PURRGO_MAP_MARKER_MIN_COURSE_DELTA_DEG) {
            return true;
        }
    }
    return false;
}

/*
 * Нужен ли хвост трека: есть фикс (маркёр стоит в текущей позиции), трек рисуется
 * и в нём есть хотя бы одна точка. Без фикса маркёр показывает последнюю известную
 * позицию, а не движение, и к треку его не тянем.
 */
static bool ui_tail_wanted(const purrgo_gnss_solution_t* gnss) {
    return gnss->valid && ui_map_track_visible() &&
           purrgo_logger_get_track_point_count() > 0U;
}

/*
 * Рисует хвост трека и маркёр поверх уже восстановленного фона: сохраняет фон под ними,
 * потом рисует хвост (от последней точки трека до центра маркёра) и маркёр. Маркёр
 * идёт последним: его белая рамка закрывает стык хвоста с центром маркёра.
 * area_out - сохранённая область (false - ничего не сохранено).
 */
static bool ui_draw_marker_and_tail(
    gfx_context_t* gfx,
    const marker_state_t* marker,
    bool tail_wanted,
    const purrgo_viewport_t* map_vp,
    const purrgo_bbox_t* dynamic_cam,
    purrgo_rect_t* area_out)
{
    purrgo_rect_t tail_rect;
    const bool tail_ok = tail_wanted &&
        purrgo_track_tail_rect(dynamic_cam, map_vp, marker->lat_1e7, marker->lon_1e7, &tail_rect);

    const bool area_ok = ui_overlay_rect(marker, tail_ok ? &tail_rect : NULL, area_out);
    ui_save_overlay_bg(gfx, area_ok ? area_out : NULL);

    if (tail_ok) {
        purrgo_rect_t drawn;
        (void)purrgo_track_render_tail(gfx, dynamic_cam, map_vp,
                                       marker->lat_1e7, marker->lon_1e7, &drawn);
    }
    ui_draw_marker(gfx, marker);

    s_tail_drawn = tail_wanted;
    return area_ok;
}

static void ui_map_render_gnss_marker(
    gfx_context_t* gfx,
    const purrgo_gnss_solution_t* gnss,
    const purrgo_viewport_t* map_vp,
    const purrgo_bbox_t* dynamic_cam,
    bool full_redraw)
{
    marker_state_t new_marker_state;
    ui_calc_marker_state(gnss, map_vp, dynamic_cam, &new_marker_state);
    const bool tail_wanted = ui_tail_wanted(gnss);

    if (full_redraw) {
        prev_marker_state_valid = true;

        purrgo_rect_t area;
        (void)ui_draw_marker_and_tail(gfx, &new_marker_state, tail_wanted, map_vp, dynamic_cam, &area);
        prev_marker_state = new_marker_state;

        if (new_marker_state.rendered) {
            purrgo_app_notify_marker_rendered(gnss);
        }
    } else {
        const bool track_pending = purrgo_ui_dirty_test(UI_DIRTY_TRACK);
        const bool changed = ui_marker_needs_redraw(&prev_marker_state, &new_marker_state);
        const bool tail_changed = (tail_wanted != s_tail_drawn);

        /*
         * Хвост (отрезок от последней записанной точки до маркёра) и новые отрезки трека
         * ложатся на область вокруг маркёра. Порядок обязателен: сначала снять старые
         * маркёр и хвост (вернуть сохранённый фон), потом дорисовать новые отрезки трека,
         * и только потом сохранить фон, нарисовать хвост и маркёр. Иначе возврат фона
         * затирает только что нарисованный отрезок (короткие отрезки пропадали целиком),
         * а рисование поверх маркёра портит его рамку. Поэтому при новых точках трека
         * маркёр перерисовывается всегда; если он при этом сместился меньше порога, он
         * остаётся на прежнем месте (рисуется прежнее состояние), чтобы порог смещения
         * продолжал работать. Конец хвоста - это центр нарисованного маркёра.
         */
        if (changed || track_pending || tail_changed) {
            const marker_state_t draw_state = changed ? new_marker_state : prev_marker_state;

            /* Область, занятая прежними маркёром и хвостом: её тоже надо обновить на экране. */
            const bool old_valid = marker_bg_cache.valid;
            const purrgo_rect_t old_area = { marker_bg_cache.x, marker_bg_cache.y,
                                             marker_bg_cache.w, marker_bg_cache.h };

            // Restore the old background
            ui_restore_marker_bg(gfx);

            if (track_pending) {
                if (ui_map_track_visible()) {
                    purrgo_rect_t rect;
                    if (purrgo_track_render_new_segments(gfx, dynamic_cam, map_vp,
                                                         &s_track_drawn_serial, &rect)) {
                        display_refresh_region(rect.x, rect.y, rect.w, rect.h);
                    }
                } else {
                    s_track_drawn_serial = purrgo_logger_get_track_serial();
                }
                purrgo_ui_dirty_clear(UI_DIRTY_TRACK);
            }

            purrgo_rect_t new_area;
            const bool new_valid = ui_draw_marker_and_tail(gfx, &draw_state, tail_wanted,
                                                           map_vp, dynamic_cam, &new_area);

            int32_t min_x = INT32_MAX, min_y = INT32_MAX, max_x = INT32_MIN, max_y = INT32_MIN;
            if (old_valid) {
                min_x = old_area.x;
                min_y = old_area.y;
                max_x = old_area.x + old_area.w - 1;
                max_y = old_area.y + old_area.h - 1;
            }
            if (new_valid) {
                if (new_area.x < min_x) min_x = new_area.x;
                if (new_area.y < min_y) min_y = new_area.y;
                if (new_area.x + new_area.w - 1 > max_x) max_x = new_area.x + new_area.w - 1;
                if (new_area.y + new_area.h - 1 > max_y) max_y = new_area.y + new_area.h - 1;
            }

            if (min_x <= max_x && min_y <= max_y) {
                display_refresh_region((int16_t)min_x, (int16_t)min_y,
                                       (int16_t)(max_x - min_x + 1), (int16_t)(max_y - min_y + 1));
            }
            prev_marker_state = draw_state;

            if (changed && draw_state.rendered) {
                purrgo_app_notify_marker_rendered(gnss);
            }
        }
    }
}

static void ui_map_render_overlays(gfx_context_t* gfx, const purrgo_gnss_solution_t* gnss, const purrgo_viewport_t* map_vp) {
    if (purrgo_ui_dirty_test(UI_DIRTY_STATUS) || purrgo_ui_dirty_test(UI_DIRTY_MAP) || !prev_status_state.valid) {
        ui_update_status_bar(gfx, gnss, map_vp->offset_y);
        purrgo_ui_dirty_clear(UI_DIRTY_STATUS);
    }

    // Clear bottom status area
    gfx_set_color(gfx, 0, 3);
    gfx_fill_rect(gfx, 0, (int16_t)(map_vp->offset_y + map_vp->height), PURRGO_HW_DISPLAY_WIDTH_PX, (int16_t)(PURRGO_HW_DISPLAY_HEIGHT_PX - (map_vp->offset_y + map_vp->height)));
}

/* Неудачная отрисовка карты: одна повторная попытка через 3 с. */
#define PURRGO_MAP_RENDER_MAX_ATTEMPTS  2U
#define PURRGO_MAP_RENDER_RETRY_DELAY_MS 3000U

static uint8_t s_map_render_attempts = 0;

/* Частичных обновлений от сдвигов карты подряд после последнего полного обновления экрана. */
static uint8_t s_shift_partials = 0;

void ui_render_map(gfx_context_t* gfx, const purrgo_gnss_solution_t* gnss, const purrgo_sun_info_t* sun) {
    (void)sun;  // Currently unused in map view

    static bool map_screen_logged = false;
    if (!map_screen_logged) {
        // PURRGO_LOG("EMU: APP_STATE_MAP rendering started\n\r");
        map_screen_logged = true;
    }

    purrgo_viewport_t map_vp;
    purrgo_bbox_t dynamic_cam;
    ui_map_setup_viewport(&map_vp, &dynamic_cam);

    ui_map_render_overlays(gfx, gnss, &map_vp);

    if (purrgo_ui_dirty_test(UI_DIRTY_MAP)) {
        bool map_success = ui_map_render_base_layers(gfx, &map_vp, &dynamic_cam);

        ui_map_render_dynamic_data(gfx, &map_vp, &dynamic_cam);

        ui_map_render_gnss_marker(gfx, gnss, &map_vp, &dynamic_cam, true);

        // Restore clipping so status UI can be drawn
        gfx_reset_clip(gfx);

        if (map_success) {
            purrgo_ui_dirty_clear(UI_DIRTY_MAP);
            s_map_render_attempts = 0;
        } else if (++s_map_render_attempts >= PURRGO_MAP_RENDER_MAX_ATTEMPTS) {
            /*
             * Карта не рисуется (нет файлов, ошибка чтения SD). Каждый проход -
             * полный refresh e-ink, поэтому повторы ограничены; следующая
             * попытка будет при очередном действии пользователя.
             */
            PURRGO_LOGE("Map render failed, giving up until next change\n\r");
            purrgo_ui_dirty_clear(UI_DIRTY_MAP);
            s_map_render_attempts = 0;
        } else {
            PURRGO_LOGE("Map render failed, retrying\n\r");
            purrgo_app_ui_defer_render(PURRGO_MAP_RENDER_RETRY_DELAY_MS);
        }
        purrgo_ui_dirty_clear(UI_DIRTY_TRACK);
#if PURRGO_MAP_SHIFT_PARTIAL_REFRESH
        if (s_shift_partials >= PURRGO_MAP_SHIFT_PARTIALS_BEFORE_FULL) {
            /* Сдвиги меняют почти весь кадр: следы копятся быстрее общего лимита драйвера. */
            purrgo_app_ui_request_full_refresh();
        } else {
            /*
             * Карта перерисована целиком, но на E-Ink уходят только изменившиеся пиксели
             * (4 gray сравнивает кадр с предыдущим, границы области драйвер игнорирует).
             * Область нужна, чтобы поставить запрос на обновление и для эмулятора:
             * строка статуса + окно карты (подсказки кнопок на этом проходе не меняются).
             */
            display_refresh_region(0, 0, PURRGO_HW_DISPLAY_WIDTH_PX,
                                   (int16_t)(map_vp.offset_y + map_vp.height));
            s_shift_partials++;
        }
#else
        purrgo_app_ui_request_full_refresh();
#endif
        dbg_map_render_calls++;
    } else {
        /* Новые отрезки трека дорисовываются внутри обновления маркёра (порядок слоёв). */
        ui_map_render_gnss_marker(gfx, gnss, &map_vp, &dynamic_cam, false);
    }
}

void ui_map_full_refresh_done(void) {
    s_shift_partials = 0;
}

void ui_map_reset(void) {
    s_shift_partials = 0;
    memset(&prev_marker_state, 0, sizeof(prev_marker_state));
    prev_marker_state_valid = false;
    memset(&prev_status_state, 0, sizeof(prev_status_state));
    s_map_render_attempts = 0;
    s_track_drawn_serial = purrgo_logger_get_track_serial();
    s_tail_drawn = false;
}
