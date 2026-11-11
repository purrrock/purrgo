#include "ui_trip.h"
#include "purrgo/app_fsm.h"
#include "purrgo/gfx_text.h"
#include "purrgo/gfx_rect.h"
#include "purrgo/config.h"
#include "purrgo/purrgo_format.h"
#include "purrgo/pgo_text.h"
#include "purrgo/waypoint.h"
#include "purrgo/gfx_line.h"
#include "purrgo/gfx_circle.h"
#include "purrgo/gfx_polygon.h"
#include "purrgo/hardware_config.h"
#include "purrgo/trip_computer.h"
#include "purrgo/gnss_ubx.h"
#include <string.h>

/* Порядок строк (шаг 12 px, первая строка y = 10). */
#define TRIP_ROW_Y0    10
#define TRIP_ROW_STEP  12
#define TRIP_ROW_Y(i)  ((int16_t)(TRIP_ROW_Y0 + (i) * TRIP_ROW_STEP))

#define TRIP_ROW_LOC   0
#define TRIP_ROW_UTC   1
#define TRIP_ROW_SPD   2
#define TRIP_ROW_CRS   3
#define TRIP_ROW_WPT   4   /* имя цели; DST и BRG - следующие две строки */
#define TRIP_ROW_FIX   7   /* FIX и SAT */
#define TRIP_ROW_LAT   8
#define TRIP_ROW_LON   9
#define TRIP_ROW_ALT   10
#define TRIP_ROW_SUN   11  /* SUN, TO SUNSET/SUNRISE, DAY (блок очищает строки 11..14) */
/* Данные UBX (PURRGO_GNSS_UBX = 1): ниже блока SUN. */
#define TRIP_ROW_TRP   15  /* дистанция с начала записи трека */
#define TRIP_ROW_ACC   16  /* точность hAcc / vAcc */
#define TRIP_ROW_SIG   17  /* глушение и спуфинг */
#define TRIP_ROW_RF    18  /* помехи (CW, AGC) и антенна */

/*
 * Компас на цель (по наброску): кольцо с 8 засечками, буквы N/E/S/W снаружи,
 * длинная стрелка на всю ширину кольца с залитым наконечником. Циферблат
 * вращается: при надёжном курсе вверх экрана направлен курс (буква N стоит там,
 * где север), иначе N вверху. Стрелка показывает цель на этом циферблате.
 * Окно очищается при каждой перерисовке и должно вмещать кольцо и буквы.
 */
#define TRIP_ARROW_CX    152
#define TRIP_ARROW_CY    92
#define TRIP_ARROW_RING  15   /* радиус кольца */
#define TRIP_TICK_LEN    3    /* засечки внутрь от кольца */
#define TRIP_LABEL_R     21   /* расстояние от центра до центра буквы */
#define TRIP_ARROW_TIP   13   /* остриё */
#define TRIP_ARROW_TAIL  11   /* хвост (в противоположную сторону) */
#define TRIP_ARROW_HEAD  7    /* длина наконечника */
#define TRIP_ARROW_WING  4    /* половина ширины наконечника */
/* Окно очистки: центр +-25 px по обеим осям (буквы 5x7 на радиусе 21). */
#define TRIP_ARROW_BOX_X (TRIP_ARROW_CX - 24)
#define TRIP_ARROW_BOX_Y (TRIP_ARROW_CY - 25)
#define TRIP_ARROW_BOX_W 48
#define TRIP_ARROW_BOX_H 51

/* Ширина поля значения в символах (от x = 40 до края экрана 176). */
#define TRIP_VALUE_CHARS 22

/* Дополняет строку пробелами до width символов (стирает прежний текст при перерисовке). */
static void pad_to(char* buf, size_t size, size_t width)
{
    size_t n = strlen(buf);

    if (width + 1 > size) {
        width = size - 1;
    }
    if (n > width) {
        n = width;
        buf[n] = '\0';
    }
    while (n < width) {
        buf[n++] = ' ';
    }
    buf[n] = '\0';
}

/* Циферблат (кольцо, засечки, N/E/S/W) и стрелка на цель. */
static void ui_trip_draw_compass(gfx_context_t* gfx, const purrgo_gnss_solution_t* gnss,
                                 uint16_t bearing_deg, bool draw_arrow)
{
    static const char k_letters[4] = { 'N', 'E', 'S', 'W' };
    const int16_t cx = TRIP_ARROW_CX;
    const int16_t cy = TRIP_ARROW_CY;
    uint16_t north = 0;   /* угол направления на север на экране */
    purrgo_wpt_pt_t p0, p1;

    /* arrow_angle(bearing = 0) даёт 360 - курс при надёжном курсе, иначе 0. */
    (void)purrgo_wpt_arrow_angle(gnss, 0, &north);

    gfx_draw_circle(gfx, cx, cy, TRIP_ARROW_RING);

    for (int k = 0; k < 8; k++) {
        const uint16_t a = (uint16_t)((north + k * 45) % 360);

        purrgo_wpt_polar(cx, cy, (int16_t)(TRIP_ARROW_RING - TRIP_TICK_LEN), a, &p0);
        purrgo_wpt_polar(cx, cy, (int16_t)(TRIP_ARROW_RING + 1), a, &p1);
        gfx_draw_line(gfx, p0.x, p0.y, p1.x, p1.y);

        if ((k & 1) == 0) {
            char s[2] = { k_letters[k / 2], '\0' };

            purrgo_wpt_polar(cx, cy, TRIP_LABEL_R, a, &p1);
            /* Шрифт 5x7: центрируем символ относительно точки. */
            gfx_draw_string(gfx, (int16_t)(p1.x - 2), (int16_t)(p1.y - 3), s);
        }
    }

    if (draw_arrow) {
        uint16_t angle = 0;
        gfx_point_t head[3];
        purrgo_wpt_pt_t tip, tail, base, wl, wr;

        (void)purrgo_wpt_arrow_angle(gnss, bearing_deg, &angle);

        purrgo_wpt_polar(cx, cy, TRIP_ARROW_TIP, angle, &tip);
        purrgo_wpt_polar(cx, cy, TRIP_ARROW_TAIL, (uint16_t)((angle + 180) % 360), &tail);
        purrgo_wpt_polar(cx, cy, (int16_t)(TRIP_ARROW_TIP - TRIP_ARROW_HEAD), angle, &base);
        purrgo_wpt_polar(base.x, base.y, TRIP_ARROW_WING, (uint16_t)((angle + 270) % 360), &wl);
        purrgo_wpt_polar(base.x, base.y, TRIP_ARROW_WING, (uint16_t)((angle + 90) % 360), &wr);

        gfx_draw_thick_line(gfx, tail.x, tail.y, base.x, base.y, 2);

        head[0].x = tip.x; head[0].y = tip.y;
        head[1].x = wl.x;  head[1].y = wl.y;
        head[2].x = wr.x;  head[2].y = wr.y;
        gfx_fill_polygon(gfx, head, 3);
        gfx_draw_polygon(gfx, head, 3);
    }
}

/* Блок цели навигации: имя, расстояние, пеленг и стрелка в круге. y_pos - первая из трёх строк. */
static void ui_trip_render_wpt(gfx_context_t* gfx, const purrgo_gnss_solution_t* gnss, int16_t y_pos)
{
    char buf[48];
    const purrgo_waypoint_t* wpt = purrgo_wpt_target();
    const purrgo_wpt_nav_t* nav = purrgo_wpt_nav();
    const int16_t val_x = 40;
    const bool nav_ok = (wpt != NULL && nav->valid);

    /* Окно стрелки очищается при каждой перерисовке. */
    gfx_set_color(gfx, 0, 3);     /* fill_rect заливает цветом фона (белый) */
    gfx_fill_rect(gfx, TRIP_ARROW_BOX_X, TRIP_ARROW_BOX_Y, TRIP_ARROW_BOX_W, TRIP_ARROW_BOX_H);
    gfx_set_color(gfx, 0, 3);

    /* Имя (UTF-8 из GPX -> PurrGO-256). */
    purrgo_snprintf(buf, sizeof(buf), "%s", (wpt != NULL) ? wpt->name : "NONE");
    purrgo_utf8_to_pgo_inplace(buf);
    pad_to(buf, sizeof(buf), TRIP_VALUE_CHARS);
    gfx_draw_string(gfx, val_x, y_pos, buf);
    y_pos = (int16_t)(y_pos + TRIP_ROW_STEP);

    /* Расстояние (правее - компас, поле ограничено 15 символами). */
    if (wpt != NULL && nav->valid) {
        char dist[16];

        purrgo_wpt_format_distance(dist, sizeof(dist), nav->nav.distance_to_wp_m);
        purrgo_snprintf(buf, sizeof(buf), "%s%s", dist, nav->nav.is_arrived ? " ARRIVED" : "");
    } else {
        purrgo_snprintf(buf, sizeof(buf), "%s", (wpt != NULL) ? "---" : "");
    }
    pad_to(buf, sizeof(buf), TRIP_VALUE_CHARS - 7);
    gfx_draw_string(gfx, val_x, y_pos, buf);

    y_pos = (int16_t)(y_pos + TRIP_ROW_STEP);

    /* Пеленг (истинный, от севера). */
    if (wpt != NULL && nav->valid) {
        purrgo_snprintf(buf, sizeof(buf), "%03d deg %s",
                        (int)nav->nav.bearing_to_wp_deg,
                        purrgo_wpt_cardinal(nav->nav.bearing_to_wp_deg));
    } else {
        purrgo_snprintf(buf, sizeof(buf), "%s", (wpt != NULL) ? "---" : "");
    }
    pad_to(buf, sizeof(buf), TRIP_VALUE_CHARS - 7);
    gfx_draw_string(gfx, val_x, y_pos, buf);

    /* Циферблат и стрелка: цель задана; стрелка - когда есть навигационные данные. */
    if (wpt != NULL) {
        ui_trip_draw_compass(gfx, gnss, nav_ok ? (uint16_t)nav->nav.bearing_to_wp_deg : 0, nav_ok);
    }
}

void ui_trip_render_grid(gfx_context_t* gfx) {
    gfx_set_color(gfx, 0, 3);
    gfx_clear(gfx);

    // Подписи строк: LOC, UTC, SPD, CRS, WPT, DST, BRG, FIX/SAT, LAT, LON, ALT.
    // Блок SUN / TO SUNSET / DAY целиком рисуется в ui_trip_render_values().
    gfx_draw_string(gfx, 10, TRIP_ROW_Y(TRIP_ROW_LOC), "LOC:");
    gfx_draw_string(gfx, 10, TRIP_ROW_Y(TRIP_ROW_UTC), "UTC:");
    gfx_draw_string(gfx, 10, TRIP_ROW_Y(TRIP_ROW_SPD), "SPD:");
    gfx_draw_string(gfx, 10, TRIP_ROW_Y(TRIP_ROW_CRS), "CRS:");
    gfx_draw_string(gfx, 10, TRIP_ROW_Y(TRIP_ROW_WPT), "WPT:");
    gfx_draw_string(gfx, 10, TRIP_ROW_Y(TRIP_ROW_WPT + 1), "DST:");
    gfx_draw_string(gfx, 10, TRIP_ROW_Y(TRIP_ROW_WPT + 2), "BRG:");
    gfx_draw_string(gfx, 10, TRIP_ROW_Y(TRIP_ROW_FIX), "FIX:");
    gfx_draw_string(gfx, 70, TRIP_ROW_Y(TRIP_ROW_FIX), "SAT:");
    gfx_draw_string(gfx, 10, TRIP_ROW_Y(TRIP_ROW_LAT), "LAT:");
    gfx_draw_string(gfx, 10, TRIP_ROW_Y(TRIP_ROW_LON), "LON:");
    gfx_draw_string(gfx, 10, TRIP_ROW_Y(TRIP_ROW_ALT), "ALT:");
#if PURRGO_GNSS_UBX
    gfx_draw_string(gfx, 10, TRIP_ROW_Y(TRIP_ROW_TRP), "TRP:");
    gfx_draw_string(gfx, 10, TRIP_ROW_Y(TRIP_ROW_ACC), "ACC:");
    gfx_draw_string(gfx, 10, TRIP_ROW_Y(TRIP_ROW_SIG), "SIG:");
    gfx_draw_string(gfx, 10, TRIP_ROW_Y(TRIP_ROW_RF), "RF:");
#endif
}

#if PURRGO_GNSS_UBX
/*
 * Строки данных UBX из снимка путевого компьютера (обновляется раз в секунду).
 * "---" - данных нет (UBX не приходит или нет фикса).
 */
static void ui_trip_render_ubx(gfx_context_t* gfx, int16_t val_x)
{
    static const char* const k_jam[] = { "--", "OK", "WARN!" };
    static const char* const k_spoof[] = { "--", "OK", "IND!", "AFF!!" };
    const purrgo_trip_info_t* t = purrgo_trip_computer_info();
    char buf[48];

    /* TRP: километры с двумя знаками. */
    if (t->dist_valid) {
        purrgo_snprintf(buf, sizeof(buf), "%u.%02u km", (unsigned int)(t->dist_m / 1000U),
                        (unsigned int)((t->dist_m % 1000U) / 10U));
    } else {
        purrgo_snprintf(buf, sizeof(buf), "---");
    }
    pad_to(buf, sizeof(buf), TRIP_VALUE_CHARS);
    gfx_draw_string(gfx, val_x, TRIP_ROW_Y(TRIP_ROW_TRP), buf);

    /* ACC: горизонтальная и вертикальная точность, метры с десятыми. */
    if (t->acc_valid) {
        purrgo_snprintf(buf, sizeof(buf), "H%u.%u m V%u.%u m",
                        (unsigned int)(t->hacc_dm / 10U), (unsigned int)(t->hacc_dm % 10U),
                        (unsigned int)(t->vacc_dm / 10U), (unsigned int)(t->vacc_dm % 10U));
    } else {
        purrgo_snprintf(buf, sizeof(buf), "---");
    }
    pad_to(buf, sizeof(buf), TRIP_VALUE_CHARS);
    gfx_draw_string(gfx, val_x, TRIP_ROW_Y(TRIP_ROW_ACC), buf);

    /* SIG: глушение и спуфинг (UBX-SEC-SIG). */
    if (t->sig_valid) {
        const char* jam = (t->jam_state < (sizeof k_jam / sizeof k_jam[0])) ? k_jam[t->jam_state] : "--";
        const char* spf = (t->spoof_state < (sizeof k_spoof / sizeof k_spoof[0])) ? k_spoof[t->spoof_state] : "--";

        purrgo_snprintf(buf, sizeof(buf), "JAM %s SPF %s", jam, spf);
    } else {
        purrgo_snprintf(buf, sizeof(buf), "---");
    }
    pad_to(buf, sizeof(buf), TRIP_VALUE_CHARS);
    gfx_draw_string(gfx, val_x, TRIP_ROW_Y(TRIP_ROW_SIG), buf);

    /* RF: CW-помеха и AGC в процентах, состояние антенны (UBX-MON-RF). */
    if (t->rf_valid) {
        const char* ant = "--";

        if (t->ant_status == PURRGO_UBX_ANT_OK) {
            ant = "OK";
        } else if (t->ant_status == PURRGO_UBX_ANT_SHORT) {
            ant = "SHORT!";
        } else if (t->ant_status == PURRGO_UBX_ANT_OPEN) {
            ant = "OPEN!";
        }
        purrgo_snprintf(buf, sizeof(buf), "CW%u AGC%u A:%s", (unsigned int)t->cw_percent,
                        (unsigned int)t->agc_percent, ant);
    } else {
        purrgo_snprintf(buf, sizeof(buf), "---");
    }
    pad_to(buf, sizeof(buf), TRIP_VALUE_CHARS);
    gfx_draw_string(gfx, val_x, TRIP_ROW_Y(TRIP_ROW_RF), buf);
}
#endif

void ui_trip_render_values(gfx_context_t* gfx, const purrgo_gnss_solution_t* gnss, const purrgo_sun_info_t* sun) {
    char buf[64];
    gfx_set_color(gfx, 0, 3);

    const int16_t val_x = 40; // x offset for values after 4-char label like "UTC: " (10 + 4 * 6 + 6)
    const int16_t lat_lon_x = 40;

    // LOC
    int32_t total_mins = (int32_t)gnss->hours * 60 + (int32_t)gnss->minutes + app_config.tz_offset_minutes;
    while (total_mins < 0) total_mins += 1440;
    while (total_mins >= 1440) total_mins -= 1440;
    uint8_t loc_hours = (uint8_t)(total_mins / 60);
    uint8_t loc_minutes = (uint8_t)(total_mins % 60);
    purrgo_snprintf(buf, sizeof(buf), "%02d:%02d", loc_hours, loc_minutes);
    gfx_draw_string(gfx, val_x, TRIP_ROW_Y(TRIP_ROW_LOC), buf);

    // UTC
    purrgo_snprintf(buf, sizeof(buf), "%02d:%02d", gnss->hours, gnss->minutes);
    gfx_draw_string(gfx, val_x, TRIP_ROW_Y(TRIP_ROW_UTC), buf);

    // SPD
    int speed_kmh = (gnss->speed_knots * 1852) / 100000;
    purrgo_snprintf(buf, sizeof(buf), "%d km/h     ", speed_kmh);
    gfx_draw_string(gfx, val_x, TRIP_ROW_Y(TRIP_ROW_SPD), buf);

    // CRS
    if (gnss->course_valid) {
        int course_deg = gnss->course_deg_100 / 100;
        int course_frac = (gnss->course_deg_100 > 0 ? gnss->course_deg_100 : -gnss->course_deg_100) % 100;
        purrgo_snprintf(buf, sizeof(buf), "%d.%02d     ", course_deg, course_frac);
    } else {
        purrgo_snprintf(buf, sizeof(buf), "N/A         ");
    }
    gfx_draw_string(gfx, val_x, TRIP_ROW_Y(TRIP_ROW_CRS), buf);

    // WPT / DST / BRG и стрелка на цель (три строки)
    ui_trip_render_wpt(gfx, gnss, TRIP_ROW_Y(TRIP_ROW_WPT));

    // FIX & SAT
    purrgo_snprintf(buf, sizeof(buf), "%-3s", gnss->valid ? "3D" : "NO");
    gfx_draw_string(gfx, val_x, TRIP_ROW_Y(TRIP_ROW_FIX), buf);

    purrgo_snprintf(buf, sizeof(buf), "%-2d", gnss->satellites_tracked);
    gfx_draw_string(gfx, 100, TRIP_ROW_Y(TRIP_ROW_FIX), buf); // "SAT: " is at 70, len 4 is 24px -> 94 + 6px space = 100

    // LAT
    int lat_deg = gnss->lat_1e7 / 10000000;
    int lat_frac = (gnss->lat_1e7 > 0 ? gnss->lat_1e7 : -gnss->lat_1e7) % 10000000;
    purrgo_snprintf(buf, sizeof(buf), "%d.%07d   ", lat_deg, lat_frac);
    gfx_draw_string(gfx, lat_lon_x, TRIP_ROW_Y(TRIP_ROW_LAT), buf);

    // LON
    int lon_deg = gnss->lon_1e7 / 10000000;
    int lon_frac = (gnss->lon_1e7 > 0 ? gnss->lon_1e7 : -gnss->lon_1e7) % 10000000;
    purrgo_snprintf(buf, sizeof(buf), "%d.%07d   ", lon_deg, lon_frac);
    gfx_draw_string(gfx, lat_lon_x, TRIP_ROW_Y(TRIP_ROW_LON), buf);

    // ALT
    purrgo_snprintf(buf, sizeof(buf), "%d m        ", (int)gnss->alt_m);
    gfx_draw_string(gfx, val_x, TRIP_ROW_Y(TRIP_ROW_ALT), buf);

#if PURRGO_GNSS_UBX
    ui_trip_render_ubx(gfx, val_x);
#endif

    int16_t y_pos = TRIP_ROW_Y(TRIP_ROW_SUN);

    // SUN INFO (No grid text split here, as this part varies completely based on state.
    // Usually, we would split it, but it's easier to just draw it whole here, overwriting
    // the area. To make sure it clears properly, we'll pad with spaces.)
    if (sun != NULL) {
        // Clear remaining lines
        for (int i = 0; i < 4; i++) {
             gfx_draw_string(gfx, 10, (int16_t)(y_pos + i*12), "                              ");
        }

        if (sun->status == SUN_STATUS_NORMAL) {
            purrgo_snprintf(buf, sizeof(buf), "SUN: %02d:%02d-%02d:%02d",
                sun->sunrise_hour, sun->sunrise_minute,
                sun->sunset_hour, sun->sunset_minute);
            gfx_draw_string(gfx, 10, y_pos, buf);
            y_pos = (int16_t)(y_pos + 12);

            int remain_h = sun->time_to_event_min / 60;
            int remain_m = sun->time_to_event_min % 60;
            if (sun->is_daytime) {
                purrgo_snprintf(buf, sizeof(buf), "TO SUNSET: %02dh %02dm", remain_h, remain_m);
            } else {
                purrgo_snprintf(buf, sizeof(buf), "TO SUNRISE: %02dh %02dm", remain_h, remain_m);
            }
            gfx_draw_string(gfx, 10, y_pos, buf);
            y_pos = (int16_t)(y_pos + 12);

            int start_min = sun->sunrise_hour * 60 + sun->sunrise_minute;
            int end_min = sun->sunset_hour * 60 + sun->sunset_minute;
            int total_day_min = end_min - start_min;
            if (total_day_min < 0) total_day_min += 1440;

            purrgo_snprintf(buf, sizeof(buf), "DAY: %02dh %02dm", total_day_min / 60, total_day_min % 60);
            gfx_draw_string(gfx, 10, y_pos, buf);
            y_pos = (int16_t)(y_pos + 12);

        } else if (sun->status == SUN_STATUS_POLAR_DAY) {
            gfx_draw_string(gfx, 10, y_pos, "POLAR DAY");
            y_pos = (int16_t)(y_pos + 12);
            gfx_draw_string(gfx, 10, y_pos, "NO SUNSET DAY: 24h 00m");
            y_pos = (int16_t)(y_pos + 12);
        } else if (sun->status == SUN_STATUS_POLAR_NIGHT) {
            gfx_draw_string(gfx, 10, y_pos, "POLAR NIGHT");
            y_pos = (int16_t)(y_pos + 12);
            gfx_draw_string(gfx, 10, y_pos, "NO SUNRISE DAY: 00h 00m");
            y_pos = (int16_t)(y_pos + 12);
        }
    }
}
