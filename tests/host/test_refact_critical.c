/*
 * Host-тесты исправлений критичных ошибок (патч 0007):
 *   - таймаут фикса GNSS (app_fsm);
 *   - обработка ошибок записи SD в track_logger;
 *   - атомарное сохранение конфига;
 *   - политика перерисовки и единый владелец dirty-флагов.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "purrgo/app_fsm.h"
#include "purrgo/track_logger.h"
#include "purrgo/config.h"
#include "purrgo/config_controller.h"
#include "purrgo/ui/ui_dirty.h"
#include "fake_fs.h"

static uint32_t g_now;
uint32_t purrgo_system_time_ms(void) { return g_now; }

static int failures, checks;
#define CHECK(c) do { checks++; if (!(c)) { failures++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static int count_str(const char* hay, const char* needle) {
    int c = 0; for (const char* p = hay; (p = strstr(p, needle)); p += strlen(needle)) c++; return c;
}

static purrgo_gnss_solution_t make_fix(int i) {
    purrgo_gnss_solution_t f = {0};
    f.valid = true; f.year = 26; f.month = 10; f.day = 19;
    f.hours = 12; f.minutes = 0; f.seconds = (uint8_t)(i % 60);
    f.lat_1e7 = 537000000 + i * 1000;   /* ~11 м шаг */
    f.lon_1e7 = 284000000; f.alt_m = 100;
    f.speed_knots = 500;
    return f;
}

static void logger_reset(void) {
    purrgo_logger_stop();
    fake_fs_reset();
    app_config.tz_offset_minutes = 0;
    purrgo_logger_set_mode(LOGGER_MODE_STANDARD);
}

static void test_logger_sync_failure_and_resume(void) {
    logger_reset();
    purrgo_gnss_solution_t f = make_fix(0);
    CHECK(purrgo_logger_start(&f));
    int recorded = 0, i;
    for (i = 0; i < 6; i++) { f = make_fix(i); if (purrgo_logger_add_point(&f)) recorded++; }
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_RECORDING);

    g_fault.fail_sync = 1;
    for (; i < 150 && purrgo_logger_get_state() == LOGGER_STATE_RECORDING; i++) {
        f = make_fix(i); if (purrgo_logger_add_point(&f)) recorded++;
    }
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_ERROR);
    CHECK(fake_fs_open_handles() == 0);              /* слот освобождён */
    f = make_fix(i++);
    CHECK(!purrgo_logger_add_point(&f));             /* в ERROR точки не принимаются */

    g_fault.fail_sync = 0;
    f = make_fix(i++);
    CHECK(purrgo_logger_start(&f));                  /* дозапись того же файла */
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_RECORDING);
    char name_before[64]; snprintf(name_before, sizeof name_before, "%s", purrgo_logger_get_active_filename());
    for (int k = 0; k < 5; k++) { f = make_fix(i++); if (purrgo_logger_add_point(&f)) recorded++; }
    purrgo_logger_stop();

    uint32_t n; const uint8_t* d = fake_fs_data(name_before, &n);
    CHECK(d != NULL);
    char* t = (char*)malloc(n + 1); memcpy(t, d, n); t[n] = 0;
    CHECK(strncmp(t, "<?xml", 5) == 0);
    CHECK(strstr(t, "</gpx>") != NULL);
    CHECK(count_str(t, "<trkpt") == count_str(t, "</trkpt>"));   /* нет обрывков тегов */
    CHECK(count_str(t, "<trkpt") >= recorded - 4);                /* потеряно не больше буфера */
    free(t);
}

static void test_logger_partial_write(void) {
    logger_reset();
    purrgo_gnss_solution_t f = make_fix(0);
    CHECK(purrgo_logger_start(&f));
    char name[64]; snprintf(name, sizeof name, "%s", purrgo_logger_get_active_filename());
    int i = 0;
    g_fault.partial_write_bytes = 100;               /* "карта заполнена" */
    for (; i < 150 && purrgo_logger_get_state() == LOGGER_STATE_RECORDING; i++) {
        f = make_fix(i); purrgo_logger_add_point(&f);
    }
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_ERROR);
    g_fault.fail_write = 0;                          /* место освободили */
    f = make_fix(i++);
    CHECK(purrgo_logger_start(&f));
    for (int k = 0; k < 6; k++) { f = make_fix(i++); purrgo_logger_add_point(&f); }
    purrgo_logger_stop();
    uint32_t n; const uint8_t* d = fake_fs_data(name, &n);
    char* t = (char*)malloc(n + 1); memcpy(t, d, n); t[n] = 0;
    CHECK(strstr(t, "</gpx>") != NULL);
    CHECK(count_str(t, "<trkpt") == count_str(t, "</trkpt>"));
    /* хвост буфера после частичной записи не потерян: заголовок идёт ровно один раз */
    CHECK(count_str(t, "<?xml") == 1);
    free(t);
}

static void test_logger_resume_failure_starts_new_file(void) {
    logger_reset();
    purrgo_gnss_solution_t f = make_fix(0);
    CHECK(purrgo_logger_start(&f));
    char first[64]; snprintf(first, sizeof first, "%s", purrgo_logger_get_active_filename());
    g_fault.fail_sync = 1;
    for (int i = 0; i < 150 && purrgo_logger_get_state() == LOGGER_STATE_RECORDING; i++) { f = make_fix(i); purrgo_logger_add_point(&f); }
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_ERROR);
    g_fault.fail_sync = 0; g_fault.fail_open_append = 1;     /* старый файл не открыть */
    g_now += 5000;
    f = make_fix(50); f.seconds = 59;                        /* другое имя файла */
    CHECK(purrgo_logger_start(&f));
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_RECORDING);
    CHECK(strcmp(first, purrgo_logger_get_active_filename()) != 0);
    purrgo_logger_stop();
}

static void test_logger_stop_in_error_is_clean(void) {
    logger_reset();
    purrgo_gnss_solution_t f = make_fix(0);
    CHECK(purrgo_logger_start(&f));
    g_fault.fail_sync = 1;
    for (int i = 0; i < 150 && purrgo_logger_get_state() == LOGGER_STATE_RECORDING; i++) { f = make_fix(i); purrgo_logger_add_point(&f); }
    purrgo_logger_set_mode(LOGGER_MODE_OFF);                 /* остановка из ERROR */
    CHECK(purrgo_logger_get_state() == LOGGER_STATE_IDLE);
    CHECK(fake_fs_open_handles() == 0);
}

static const char* CFG = "PURRGO.CFG";

static void test_config_atomic_save(void) {
    fake_fs_reset();
    purrgo_config_init();
    app_config.tz_offset_minutes = 180;
    CHECK(purrgo_config_save());
    uint32_t n; const uint8_t* d = fake_fs_data(CFG, &n);
    CHECK(d && strstr((const char*)d, "TZ_MIN=180"));
    CHECK(fake_fs_data("PURRGO.CFG.TMP", &n) == NULL);       /* временный файл не остаётся */

    /* сбой записи: старый конфиг цел */
    app_config.tz_offset_minutes = 60;
    g_fault.fail_write = 1;
    CHECK(!purrgo_config_save());
    g_fault.fail_write = 0;
    d = fake_fs_data(CFG, &n);
    CHECK(d && strstr((const char*)d, "TZ_MIN=180"));

    /* сбой sync / close */
    g_fault.fail_sync = 1; CHECK(!purrgo_config_save()); g_fault.fail_sync = 0;
    g_fault.fail_close = 1; CHECK(!purrgo_config_save()); g_fault.fail_close = 0;
    d = fake_fs_data(CFG, &n);
    CHECK(d && strstr((const char*)d, "TZ_MIN=180"));

    /* сбой замены */
    g_fault.fail_replace = 1; CHECK(!purrgo_config_save()); g_fault.fail_replace = 0;
    d = fake_fs_data(CFG, &n);
    CHECK(d && strstr((const char*)d, "TZ_MIN=180"));

    CHECK(purrgo_config_save());
    d = fake_fs_data(CFG, &n);
    CHECK(d && strstr((const char*)d, "TZ_MIN=60"));
}

static void test_config_recovery(void) {
    /* питание пропало между unlink и rename: есть только полный .TMP */
    fake_fs_reset();
    fake_fs_put("PURRGO.CFG.TMP", "TZ_MIN=420\nLAYER_ROADS=0\n");
    CHECK(purrgo_config_load());
    CHECK(app_config.tz_offset_minutes == 420);
    CHECK(!purrgo_layer_on(app_config.layers, PURRGO_LAYER_ROADS));

    /* .TMP оборван посреди строки: не используем */
    fake_fs_reset();
    fake_fs_put("PURRGO.CFG.TMP", "TZ_MIN=420\nLAYER_RO");
    purrgo_config_load();
    CHECK(app_config.tz_offset_minutes != 420);

    /* рабочий конфиг есть: устаревший .TMP его не затирает */
    fake_fs_reset();
    fake_fs_put("PURRGO.CFG", "TZ_MIN=100\n");
    fake_fs_put("PURRGO.CFG.TMP", "TZ_MIN=999\n");
    CHECK(purrgo_config_load());
    CHECK(app_config.tz_offset_minutes == 100);
}

/* --- слои карты: маска в памяти, текстовые строки LAYER_<ИМЯ>=0|1 в файле --- */

static bool layer(purrgo_layer_t id) { return purrgo_layer_on(app_config.layers, id); }

static void test_config_layers(void) {
    uint32_t n; const uint8_t* d;

    /* значения по умолчанию */
    fake_fs_reset();
    purrgo_config_init();
    CHECK(layer(PURRGO_LAYER_ROADS) && layer(PURRGO_LAYER_TRACK) && layer(PURRGO_LAYER_POI_LABELS));
    CHECK(!layer(PURRGO_LAYER_WATER) && !layer(PURRGO_LAYER_ROUTE));

    /* файл, отредактированный вручную: пробелы/CRLF допустимы, неизвестный ключ и
     * отсутствующие ключи не мешают, слои из файла имеют приоритет над умолчаниями */
    fake_fs_reset();
    fake_fs_put(CFG, "TZ_MIN=60\r\nLAYER_WATER=1\r\nLAYER_ROADS=0\r\nLAYER_BOGUS=1\r\nLAYER_=1\r\n");
    CHECK(purrgo_config_load());
    CHECK(layer(PURRGO_LAYER_WATER));
    CHECK(!layer(PURRGO_LAYER_ROADS));
    CHECK(layer(PURRGO_LAYER_LANDUSE) && layer(PURRGO_LAYER_TRACK));   /* по умолчанию */
    CHECK(!layer(PURRGO_LAYER_ROUTE));

    /* сохранение: по строке на слой, текстом */
    CHECK(purrgo_config_save());
    d = fake_fs_data(CFG, &n);
    CHECK(d != NULL);
    if (d) {
        const char* t = (const char*)d;
        CHECK(strstr(t, "LAYER_LANDUSE=1\n") && strstr(t, "LAYER_WATER=1\n"));
        CHECK(strstr(t, "LAYER_LANDUSE_LABELS=1\n") && strstr(t, "LAYER_WATER_LABELS=0\n"));
        CHECK(strstr(t, "LAYER_ROADS=0\n") && strstr(t, "LAYER_POI=1\n"));
        CHECK(strstr(t, "LAYER_POI_LABELS=1\n") && strstr(t, "LAYER_ROUTE=0\n"));
        CHECK(strstr(t, "LAYER_TRACK=1\n"));
    }

    /* круговая загрузка даёт ту же маску */
    const uint16_t saved = app_config.layers;
    app_config.layers = 0;
    CHECK(purrgo_config_load());
    CHECK(app_config.layers == saved);

    /* меню MAP LAYERS: OK переключает пункт под курсором, MENU применяет и сохраняет */
    purrgo_state_t next = APP_STATE_MENU_MAP_LAYERS;
    purrgo_config_controller_on_enter(APP_STATE_MENU_MAP_LAYERS);
    CHECK(config_app_get_draft_layers() == saved);
    CHECK(purrgo_config_controller_handle_button(APP_STATE_MENU_MAP_LAYERS, PURRGO_ACTION_OK, &next));   /* Landuse */
    CHECK(purrgo_config_controller_handle_button(APP_STATE_MENU_MAP_LAYERS, PURRGO_ACTION_UP, &next));   /* перенос на последний: Track */
    CHECK(config_app_get_map_layers_cursor() == PURRGO_LAYER_COUNT - 1);
    CHECK(purrgo_config_controller_handle_button(APP_STATE_MENU_MAP_LAYERS, PURRGO_ACTION_OK, &next));
    CHECK(app_config.layers == saved);                                    /* до MENU конфиг не меняется */
    CHECK(purrgo_config_controller_handle_button(APP_STATE_MENU_MAP_LAYERS, PURRGO_ACTION_MENU, &next));
    CHECK(next == APP_STATE_MENU_CONFIG);
    CHECK(!layer(PURRGO_LAYER_LANDUSE) && !layer(PURRGO_LAYER_TRACK));
    CHECK(layer(PURRGO_LAYER_WATER) && !layer(PURRGO_LAYER_ROADS));
    d = fake_fs_data(CFG, &n);
    CHECK(d && strstr((const char*)d, "LAYER_LANDUSE=0\n") && strstr((const char*)d, "LAYER_TRACK=0\n"));
}

/* --- PURRGO.CFG: длинный MAP_DIR и файл с комментариями при ручной правке --- */
static void test_config_size_limits(void) {
    uint32_t n; const uint8_t* d;

    /* MAP_DIR максимальной допустимой длины (240) + все слои сохраняются и читаются */
    char dir[260];
    memset(dir, 'a', 240); dir[0] = '/'; dir[240] = '\0';
    fake_fs_reset();
    purrgo_config_init();
    strcpy(app_config.map_dir, dir);
    app_config.tz_offset_minutes = -720;
    app_config.last_lat_1e7 = -2147483647 - 1;
    app_config.last_lon_1e7 = -2147483647 - 1;
    CHECK(purrgo_config_save());
    d = fake_fs_data(CFG, &n);
    CHECK(d != NULL && n > 480);
    CHECK(d && strstr((const char*)d, "LAYER_TRACK=1\n") != NULL);   /* хвост не обрезан */
    app_config.map_dir[0] = '\0';
    app_config.layers = 0;
    CHECK(purrgo_config_load());
    CHECK(strcmp(app_config.map_dir, dir) == 0);
    CHECK(layer(PURRGO_LAYER_TRACK) && layer(PURRGO_LAYER_ROADS));

    /* файл с комментариями и пустыми строками (~800 байт): слои в конце читаются */
    char txt[1100]; size_t k = 0;
    for (int i = 0; i < 12; i++) {
        k += (size_t)snprintf(txt + k, sizeof(txt) - k, "; comment line number %02d ................................\n\n", i);
    }
    k += (size_t)snprintf(txt + k, sizeof(txt) - k, "LAYER_ROADS=0\nLAYER_ROUTE=1\n");
    CHECK(k > 600 && k < 1000);
    fake_fs_reset();
    fake_fs_put(CFG, txt);
    CHECK(purrgo_config_load());
    CHECK(!layer(PURRGO_LAYER_ROADS) && layer(PURRGO_LAYER_ROUTE));
}

/* --- GNSS timeout + redraw policy --- */

static const char* RMC_A = "$GPRMC,120000.00,A,5342.0000,N,02825.0000,E,0.50,90.00,191026,,,A*";
static const char* RMC_V = "$GPRMC,120000.00,V,,,,,,,191026,,,N*";

static void feed_sentence(const char* body_with_star) {
    /* body: от '$' до '*' включительно; считаем контрольную сумму */
    unsigned cs = 0;
    for (const char* p = body_with_star + 1; *p && *p != '*'; p++) cs ^= (unsigned char)*p;
    char line[128];
    snprintf(line, sizeof line, "%s%02X\r\n", body_with_star, cs);
    for (const char* p = line; *p; p++) purrgo_app_feed_gnss_byte((uint8_t)*p);
}

static void run_ticks(uint32_t ms) {
    for (uint32_t t = 0; t < ms; t += 100) { g_now += 100; purrgo_app_tick(g_now); }
}

static void test_gnss_timeout(void) {
    fake_fs_reset();
    g_now = 10000;
    purrgo_app_init();
    run_ticks(1000);
    feed_sentence(RMC_A);
    CHECK(purrgo_app_get_gnss_solution()->valid);

    /* приёмник жив: RMC каждую секунду - фикс держится */
    for (int i = 0; i < 10; i++) { run_ticks(1000); feed_sentence(RMC_A); }
    CHECK(purrgo_app_get_gnss_solution()->valid);

    /* приёмник замолчал */
    run_ticks(4000);
    CHECK(purrgo_app_get_gnss_solution()->valid);            /* ещё в пределах таймаута */
    run_ticks(2000);
    CHECK(!purrgo_app_get_gnss_solution()->valid);           /* сброшен */
    CHECK(!purrgo_app_get_gnss_solution()->course_valid);

    /* возвращается с очередной RMC */
    feed_sentence(RMC_A);
    CHECK(purrgo_app_get_gnss_solution()->valid);

    /* RMC со статусом V: приёмник жив, но фикса нет - valid=false сразу */
    feed_sentence(RMC_V);
    CHECK(!purrgo_app_get_gnss_solution()->valid);
}

static void test_gnss_stall_grace(void) {
    fake_fs_reset();
    g_now = 50000;
    purrgo_app_init();
    run_ticks(1000);
    feed_sentence(RMC_A);
    /* главный цикл простоял 4 с (блокирующий refresh), тики не вызывались */
    g_now += 4000;
    purrgo_app_tick(g_now);
    CHECK(purrgo_app_get_gnss_solution()->valid);            /* ложного сброса нет */
}

static void test_redraw_policy(void) {
    fake_fs_reset();
    g_now = 100000;
    purrgo_app_init();
    run_ticks(1000);
    feed_sentence(RMC_A);
    run_ticks(1000);
    /* дождаться первой отрисовки и забрать флаги */
    while (purrgo_app_ui_needs_render(g_now)) { purrgo_ui_dirty_clear(UI_DIRTY_SCREEN); purrgo_ui_dirty_clear(UI_DIRTY_MAP); }

    /* нераспознанная кнопка перерисовки не вызывает */
    purrgo_app_handle_button((purrgo_btn_t)99);
    CHECK(!purrgo_app_ui_needs_render(g_now));

    /* нажатие кнопки: сразу */
    purrgo_app_handle_button(PURRGO_BTN_KEY4_SHORT);          /* MAP -> next screen? (долгая = MENU) */
    purrgo_app_handle_button(PURRGO_BTN_KEY4_LONG);
    CHECK(purrgo_app_ui_needs_render(g_now));
    purrgo_ui_dirty_clear(UI_DIRTY_SCREEN);
    CHECK(purrgo_app_get_state() == APP_STATE_TRIP_COMPUTER);

    /* шум спутников/высоты внутри порога и без смены цифр: перерисовки нет */
    for (int i = 0; i < 20; i++) {
        run_ticks(1000);
        feed_sentence(RMC_A);
        while (purrgo_app_ui_needs_render(g_now)) { purrgo_ui_dirty_clear(UI_DIRTY_SCREEN); }
    }
    int redraws = 0;
    for (int i = 0; i < 20; i++) {
        run_ticks(1000);
        feed_sentence(RMC_A);
        if (purrgo_app_ui_needs_render(g_now)) { redraws++; purrgo_ui_dirty_clear(UI_DIRTY_SCREEN); }
    }
    CHECK(redraws == 0);

    /* неудачный рендер: пауза до повтора */
    purrgo_ui_dirty_mark(UI_DIRTY_SCREEN);
    purrgo_ui_dirty_clear(UI_DIRTY_SCREEN);
    purrgo_ui_dirty_mark(UI_DIRTY_SCREEN);
    purrgo_app_ui_defer_render(3000);
    CHECK(!purrgo_app_ui_needs_render(g_now));
    g_now += 3000;
    CHECK(purrgo_app_ui_needs_render(g_now));
}

int main(void) {
    test_logger_sync_failure_and_resume();
    test_logger_partial_write();
    test_logger_resume_failure_starts_new_file();
    test_logger_stop_in_error_is_clean();
    test_config_atomic_save();
    test_config_recovery();
    test_config_layers();
    test_config_size_limits();
    test_gnss_timeout();
    test_gnss_stall_grace();
    test_redraw_policy();
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
