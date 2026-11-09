#include "purrgo/battery.h"
#include "purrgo/config.h"
#include "purrgo/hardware_config.h"
#include "purrgo/pgo_text.h"
#include "purrgo/purrgo_format.h"
#include "purrgo/supply.h"
#include "purrgo/system_time.h"

#include <string.h>

/*
 * Мониторинг батареи (см. purrgo/battery.h).
 *
 * Кривая разряда выбирается по источнику питания из app_config.power_source
 * (меню CONFIG, первый пункт). Напряжение на выводе АЦП даёт платформа
 * (purrgo_platform_battery_pin_mv()), здесь оно пересчитывается по делителю.
 * Если напряжение не измерено (0), процент берётся из заглушки
 * PURRGO_BATTERY_STUB_PERCENT; для проверки отображения можно переопределить
 * при сборке: -DPURRGO_BATTERY_STUB_PERCENT=55 (-1 - показывать "неизвестно").
 */
#ifndef PURRGO_BATTERY_STUB_PERCENT
#define PURRGO_BATTERY_STUB_PERCENT 100
#endif

/*
 * Точки кривой разряда: напряжение ОДНОГО элемента под небольшой нагрузкой, мВ
 * (по убыванию) -> заряд, %. Между точками - линейная интерполяция, выше первой
 * - 100 %, ниже последней - 0 %.
 *
 * ЗАГЛУШКА: значения типовые, по справочным кривым производителей; их нужно
 * уточнить по измерениям на устройстве (ток нагрузки у разных источников
 * разный, а у Ni-MH кривая почти плоская).
 */
typedef struct {
    uint16_t cell_mv;
    uint8_t  percent;
} curve_point_t;

typedef struct {
    const curve_point_t* points;
    uint8_t              count;
} curve_t;

static const curve_point_t s_curve_zinc[] = {
    { 1600, 100 }, { 1500, 85 }, { 1400, 65 }, { 1300, 45 },
    { 1200,  25 }, { 1100, 10 }, {  900,  0 },
};

static const curve_point_t s_curve_alkaline[] = {
    { 1600, 100 }, { 1500, 90 }, { 1400, 68 }, { 1300, 42 },
    { 1200,  18 }, { 1100,  6 }, {  900,  0 },
};

static const curve_point_t s_curve_nimh[] = {
    { 1400, 100 }, { 1300, 90 }, { 1250, 60 }, { 1200, 30 },
    { 1150,  10 }, { 1050,  0 },
};

/*
 * Li-ion AA со встроенным понижающим преобразователем (выход ~1,5 В):
 * напряжение почти не меняется до конца разряда, затем резко падает
 * (срабатывает защита). Процент по напряжению невозможен: кривая -
 * "ступенька", 100 % пока напряжение держится и быстрое падение к 0 в конце.
 * Пороги ЗАГЛУШКА, уточнить по логу разряда (pg:vbat в треке).
 */
static const curve_point_t s_curve_liion_aa[] = {
    { 1600, 100 }, { 1400, 100 }, { 1300, 10 }, { 1100, 0 },
};

/*
 * Литиевые батарейки Li-FeS2 (Energizer Ultimate Lithium и аналоги): свежий
 * элемент 1,7-1,8 В, длинное плато 1,5-1,6 В, затем быстрое падение к
 * ~1,0 В. Начальное напряжение выше, чем у щелочных, поэтому блок из трёх
 * элементов может дать до ~5,4 В. Точки ЗАГЛУШКА по справочным кривым,
 * уточнить по логу разряда (pg:vbat в треке).
 */
static const curve_point_t s_curve_lithium[] = {
    { 1750, 100 }, { 1650, 85 }, { 1600, 65 }, { 1550, 45 },
    { 1500,  25 }, { 1400, 10 }, { 1200,  0 },
};

#define CURVE(a) { (a), (uint8_t)(sizeof(a) / sizeof((a)[0])) }

/* Индекс = purrgo_power_source_t; у питания 5 В кривой нет. */
static const curve_t s_curves[PURRGO_POWER_SRC_COUNT] = {
    [PURRGO_POWER_SRC_ZINC_CARBON] = CURVE(s_curve_zinc),
    [PURRGO_POWER_SRC_ALKALINE]    = CURVE(s_curve_alkaline),
    [PURRGO_POWER_SRC_NIMH]        = CURVE(s_curve_nimh),
    [PURRGO_POWER_SRC_LIION_AA]    = CURVE(s_curve_liion_aa),
    [PURRGO_POWER_SRC_LITHIUM]     = CURVE(s_curve_lithium),
};

static const char* const s_titles[PURRGO_POWER_SRC_COUNT] = {
    [PURRGO_POWER_SRC_ZINC_CARBON] = "ZINC-CARBON",
    [PURRGO_POWER_SRC_ALKALINE]    = "ALKALINE",
    [PURRGO_POWER_SRC_NIMH]        = "NIMH",
    [PURRGO_POWER_SRC_LIION_AA]    = "LI-ION AA",
    [PURRGO_POWER_SRC_LITHIUM]     = "LITHIUM",
    [PURRGO_POWER_SRC_EXTERNAL_5V] = "5V",
};

const char* purrgo_power_source_title(purrgo_power_source_t src)
{
    return ((unsigned)src < PURRGO_POWER_SRC_COUNT) ? s_titles[src] : "?";
}

int purrgo_battery_percent_from_mv(purrgo_power_source_t src, uint32_t pack_mv)
{
    if ((unsigned)src >= PURRGO_POWER_SRC_COUNT || s_curves[src].points == NULL) {
        return -1;
    }

    const curve_point_t* p = s_curves[src].points;
    const unsigned n = s_curves[src].count;

    /* Напряжение одного элемента с округлением. */
    const uint32_t cell_mv = (pack_mv + PURRGO_BATTERY_CELLS / 2u) / PURRGO_BATTERY_CELLS;

    if (cell_mv >= p[0].cell_mv) {
        return p[0].percent;
    }
    if (cell_mv <= p[n - 1u].cell_mv) {
        return p[n - 1u].percent;
    }

    for (unsigned i = 1; i < n; i++) {
        if (cell_mv >= p[i].cell_mv) {
            const uint32_t hi_mv = p[i - 1u].cell_mv;
            const uint32_t lo_mv = p[i].cell_mv;
            const uint32_t hi_pc = p[i - 1u].percent;
            const uint32_t lo_pc = p[i].percent;
            return (int)(lo_pc + (cell_mv - lo_mv) * (hi_pc - lo_pc) / (hi_mv - lo_mv));
        }
    }
    return p[n - 1u].percent;
}

uint32_t purrgo_battery_pack_mv_from_pin(uint32_t pin_mv)
{
    const uint64_t div = (uint64_t)pin_mv
        * (PURRGO_HW_BATT_DIV_R_TOP_OHM + PURRGO_HW_BATT_DIV_R_BOTTOM_OHM)
        / PURRGO_HW_BATT_DIV_R_BOTTOM_OHM;
    return (uint32_t)(div * PURRGO_HW_BATT_CAL_PERMILLE / 1000u);
}

bool purrgo_battery_read(purrgo_battery_t* out)
{
    if (!out) {
        return false;
    }

    const purrgo_power_source_t src = app_config.power_source;

    out->percent = -1;
    out->millivolts = 0;
    out->external = (src == PURRGO_POWER_SRC_EXTERNAL_5V);

    if (out->external) {
        return false;
    }

    uint32_t pack_mv = purrgo_battery_pack_mv_from_pin(purrgo_platform_battery_pin_mv());
    if (pack_mv > UINT16_MAX) {
        pack_mv = UINT16_MAX;
    }
    out->millivolts = (uint16_t)pack_mv;
    if (out->millivolts != 0) {
        out->percent = (int8_t)purrgo_battery_percent_from_mv(src, out->millivolts);
    } else {
        out->percent = (int8_t)PURRGO_BATTERY_STUB_PERCENT;
    }
    return out->percent >= 0;
}

void purrgo_battery_format(char* buf, size_t size)
{
    if (buf == NULL || size == 0u) {
        return;
    }

    /* Запись на SD остановлена из-за низкого питания (supply.h): заметный индикатор вместо процента. */
    if (purrgo_supply_storage_blocked()) {
        purrgo_snprintf(buf, size, "LOW");
        return;
    }

    purrgo_battery_t bat;
    if (purrgo_battery_read(&bat) && bat.percent >= 0 && bat.percent <= 100) {
        purrgo_snprintf(buf, size, "%d%%", (int)bat.percent);
    } else if (bat.external) {
        purrgo_snprintf(buf, size, "5V");
    } else {
        purrgo_snprintf(buf, size, "--%%");
    }
}

/* ---- Монитор для строки статуса ----------------------------------------------- */

static struct {
    bool                  sampled;
    bool                  blocked;      /* состояние блокировки записи на SD при замере */
    uint32_t              sampled_ms;
    purrgo_power_source_t src;          /* источник питания, для которого сделан замер */
    char                  text[PURRGO_BATTERY_STATUS_TEXT_SIZE];
} s_mon;

/* Замер. true - текст изменился. */
static bool monitor_sample(uint32_t now_ms)
{
    char text[sizeof(s_mon.text)];

    purrgo_battery_format(text, sizeof(text));
    s_mon.sampled = true;
    s_mon.sampled_ms = now_ms;
    s_mon.src = app_config.power_source;
    s_mon.blocked = purrgo_supply_storage_blocked();
    if (strcmp(text, s_mon.text) == 0) {
        return false;
    }
    memcpy(s_mon.text, text, sizeof(s_mon.text));
    return true;
}

bool purrgo_battery_monitor_tick(uint32_t now_ms)
{
    const bool first = !s_mon.sampled;

    if (!first && s_mon.src == app_config.power_source &&
        s_mon.blocked == purrgo_supply_storage_blocked() &&
        (uint32_t)(now_ms - s_mon.sampled_ms) < PURRGO_BATTERY_STATUS_PERIOD_MS) {
        return false;
    }
    return monitor_sample(now_ms) && !first;
}

const char* purrgo_battery_status_text(void)
{
    if (!s_mon.sampled || s_mon.src != app_config.power_source ||
        s_mon.blocked != purrgo_supply_storage_blocked()) {
        (void)monitor_sample(purrgo_system_time_ms());
    }
    return s_mon.text;
}

void purrgo_battery_monitor_reset(void)
{
    memset(&s_mon, 0, sizeof(s_mon));
}
