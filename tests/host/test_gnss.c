/*
 * Host-тесты GNSS (патчи 0001, 0009, 0015):
 *   - кольцевой буфер приёма (purrgo/gnss_ring.h), в том числе гонка
 *     «NDTR перезагружен, а счётчик оборотов ещё нет», из-за которой раньше
 *     терялся целый буфер данных;
 *   - побайтовый разбор NMEA на записанном логе (data/gnss_sample.nmea):
 *     тот же путь, что в app_fsm - purrgo_gnss_parser_feed + purrgo_gnss_process_nmea.
 */
#include <stdlib.h>
#include <string.h>
#include "purrgo/gnss.h"
#include "purrgo/gnss_adapter.h"
#include "purrgo/gnss_ring.h"
#include "test_common.h"

#ifndef TEST_DATA_DIR
#define TEST_DATA_DIR "tests/host/data"
#endif

uint32_t purrgo_system_time_ms(void) { return 0; }

/* ---------------------------------------------------------------- ring --- */

#define RING_SIZE 1024U

/* Модель DMA: пишет по кругу, NDTR = сколько байт осталось до конца буфера. */
typedef struct {
    uint8_t buf[RING_SIZE];
    uint32_t wr;   /* индекс следующей записи */
} dma_model_t;

static void dma_write(dma_model_t *d, const uint8_t *src, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        d->buf[d->wr] = src[i];
        d->wr = (d->wr + 1U) % RING_SIZE;
    }
}

static uint32_t dma_ndtr(const dma_model_t *d) { return RING_SIZE - d->wr; }

static void test_ring_init(void)
{
    uint8_t b[16] = {0};
    purrgo_gnss_ring_t r;
    CHECK(purrgo_gnss_ring_init(&r, b, 16));
    CHECK(!purrgo_gnss_ring_init(&r, b, 12));      /* не степень двойки */
    CHECK(!purrgo_gnss_ring_init(&r, b, 0));
    CHECK(!purrgo_gnss_ring_init(&r, b, 1));
    CHECK(!purrgo_gnss_ring_init(&r, NULL, 16));
    CHECK(!purrgo_gnss_ring_init(NULL, b, 16));
}

static void test_ring_head_from_ndtr(void)
{
    dma_model_t d = {0};
    purrgo_gnss_ring_t r;
    CHECK(purrgo_gnss_ring_init(&r, d.buf, RING_SIZE));

    CHECK(purrgo_gnss_ring_head_from_ndtr(&r, RING_SIZE) == 0);       /* только что запущен */
    CHECK(purrgo_gnss_ring_head_from_ndtr(&r, RING_SIZE - 1U) == 1);
    CHECK(purrgo_gnss_ring_head_from_ndtr(&r, 1U) == RING_SIZE - 1U); /* последний байт */
    CHECK(purrgo_gnss_ring_head_from_ndtr(&r, 0U) == 0);              /* кратковременный 0 */
    CHECK(purrgo_gnss_ring_head_from_ndtr(&r, RING_SIZE + 1U) == 0);  /* мусор */
    CHECK(purrgo_gnss_ring_head_from_ndtr(&r, 0xFFFFFFFFU) == 0);
}

/* Потока 200 тыс. байт без потерь: пачки до size-1 байт между опросами. */
static void test_ring_no_loss(void)
{
    enum { TOTAL = 200000 };
    static uint8_t src[TOTAL];
    static uint8_t dst[TOTAL];
    for (int i = 0; i < TOTAL; i++) { src[i] = (uint8_t)((i * 31 + (i >> 8)) & 0xFF); }

    dma_model_t d = {0};
    purrgo_gnss_ring_t r;
    CHECK(purrgo_gnss_ring_init(&r, d.buf, RING_SIZE));

    srand(12345);
    size_t produced = 0, consumed = 0;
    while (produced < TOTAL) {
        size_t burst = 1U + (size_t)(rand() % (int)(RING_SIZE - 1U));   /* 1..size-1 */
        if (burst > TOTAL - produced) { burst = TOTAL - produced; }
        dma_write(&d, src + produced, burst);
        produced += burst;

        uint8_t b;
        while (purrgo_gnss_ring_pop(&r, purrgo_gnss_ring_head_from_ndtr(&r, dma_ndtr(&d)), &b)) {
            if (consumed < TOTAL) { dst[consumed] = b; }
            consumed++;
        }
    }
    CHECK(consumed == TOTAL);
    CHECK(memcmp(src, dst, TOTAL) == 0);
}

/* Потребитель читает ОДИН байт за проход цикла, DMA пишет параллельно. */
static void test_ring_interleaved(void)
{
    enum { TOTAL = 50000 };
    dma_model_t d = {0};
    purrgo_gnss_ring_t r;
    CHECK(purrgo_gnss_ring_init(&r, d.buf, RING_SIZE));

    size_t produced = 0, consumed = 0;
    int bad = 0;
    srand(777);
    while (consumed < TOTAL) {
        /* DMA: 0..3 новых байта (в среднем быстрее, чем читаем по одному - но
           потребитель успевает, потому что накопление ограничено условием). */
        int k = rand() % 3;
        for (int i = 0; i < k && produced < TOTAL && (produced - consumed) < RING_SIZE - 1U; i++) {
            uint8_t v = (uint8_t)(produced & 0xFF);
            dma_write(&d, &v, 1);
            produced++;
        }
        uint8_t b;
        if (purrgo_gnss_ring_pop(&r, purrgo_gnss_ring_head_from_ndtr(&r, dma_ndtr(&d)), &b)) {
            if (b != (uint8_t)(consumed & 0xFF)) { bad++; }
            consumed++;
        }
    }
    CHECK(bad == 0);
    CHECK(consumed == TOTAL);
}

/*
 * Задокументированное ограничение: если DMA записал ровно size байт между
 * опросами, head == tail и это неотличимо от «данных нет».
 * Поэтому purrgo_gnss_read_byte() должна опрашиваться чаще ~1 с (9600 бод).
 */
static void test_ring_documented_limit(void)
{
    dma_model_t d = {0};
    purrgo_gnss_ring_t r;
    CHECK(purrgo_gnss_ring_init(&r, d.buf, RING_SIZE));
    uint8_t chunk[RING_SIZE];
    memset(chunk, 0x55, sizeof chunk);
    dma_write(&d, chunk, RING_SIZE);
    uint8_t b;
    CHECK(!purrgo_gnss_ring_pop(&r, purrgo_gnss_ring_head_from_ndtr(&r, dma_ndtr(&d)), &b));
    /* size-1 байт - видны все */
    purrgo_gnss_ring_reset(&r);
    d.wr = 0;
    dma_write(&d, chunk, RING_SIZE - 1U);
    int n = 0;
    while (purrgo_gnss_ring_pop(&r, purrgo_gnss_ring_head_from_ndtr(&r, dma_ndtr(&d)), &b)) { n++; }
    CHECK(n == (int)(RING_SIZE - 1U));
}

/* После UART-ошибки DMA стартует с начала: reset возвращает tail в 0. */
static void test_ring_reset_after_error(void)
{
    dma_model_t d = {0};
    purrgo_gnss_ring_t r;
    CHECK(purrgo_gnss_ring_init(&r, d.buf, RING_SIZE));
    uint8_t v[10] = {1,2,3,4,5,6,7,8,9,10};
    dma_write(&d, v, 10);
    uint8_t b;
    for (int i = 0; i < 4; i++) {
        CHECK(purrgo_gnss_ring_pop(&r, purrgo_gnss_ring_head_from_ndtr(&r, dma_ndtr(&d)), &b));
    }
    /* перезапуск DMA: producer снова с нуля, потребитель сбрасывается */
    purrgo_gnss_ring_reset(&r);
    d.wr = 0;
    CHECK(!purrgo_gnss_ring_pop(&r, purrgo_gnss_ring_head_from_ndtr(&r, dma_ndtr(&d)), &b));
    uint8_t w[3] = {0xA, 0xB, 0xC};
    dma_write(&d, w, 3);
    CHECK(purrgo_gnss_ring_pop(&r, purrgo_gnss_ring_head_from_ndtr(&r, dma_ndtr(&d)), &b) && b == 0xA);
}

/* ---------------------------------------------------------------- NMEA --- */

static uint8_t *load_log(size_t *len)
{
    FILE *f = fopen(TEST_DATA_DIR "/gnss_sample.nmea", "rb");
    if (!f) { return NULL; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc((size_t)n);
    if (buf && fread(buf, 1, (size_t)n, f) != (size_t)n) { free(buf); buf = NULL; }
    fclose(f);
    *len = (size_t)n;
    return buf;
}

typedef struct {
    int rmc_ok;                     /* фраз RMC, принятых парсером */
    purrgo_gnss_solution_t after_epoch5;   /* состояние после 6-й эпохи (RMC 'A') */
    purrgo_gnss_solution_t after_void;     /* сразу после RMC со статусом V */
    purrgo_gnss_solution_t after_epoch9;
    purrgo_gnss_solution_t final;
} nmea_result_t;

/* Прогон лога тем же путём, что app_fsm: побайтно. Источник байт - callback. */
static void run_parser(const uint8_t *log, size_t len, size_t chunk_max, nmea_result_t *res,
                       bool via_ring)
{
    purrgo_gnss_adapter_reset();
    purrgo_gnss_parser_t p;
    purrgo_gnss_parser_init(&p);
    purrgo_gnss_solution_t sol;
    memset(&sol, 0, sizeof sol);
    memset(res, 0, sizeof *res);

    dma_model_t d = {0};
    purrgo_gnss_ring_t r;
    (void)purrgo_gnss_ring_init(&r, d.buf, RING_SIZE);

    int rmc_seq = 0;
    size_t pos = 0;
    srand(4242);
    while (pos < len) {
        size_t n = 1U + (size_t)rand() % chunk_max;
        if (n > len - pos) { n = len - pos; }

        uint8_t tmp[RING_SIZE];
        size_t got = 0;
        if (via_ring) {
            dma_write(&d, log + pos, n);
            uint8_t b;
            while (purrgo_gnss_ring_pop(&r, purrgo_gnss_ring_head_from_ndtr(&r, dma_ndtr(&d)), &b)) {
                tmp[got++] = b;
            }
        } else {
            memcpy(tmp, log + pos, n);
            got = n;
        }
        pos += n;

        for (size_t i = 0; i < got; i++) {
            if (purrgo_gnss_parser_feed(&p, tmp[i])) {
                if (purrgo_gnss_process_nmea(p.line, &sol)) {
                    res->rmc_ok++;
                    rmc_seq++;
                    if (rmc_seq == 6) { res->after_epoch5 = sol; }
                    if (rmc_seq == 7) { res->after_void = sol; }
                    if (rmc_seq == 10) { res->after_epoch9 = sol; }
                }
                purrgo_gnss_parser_init(&p);
            }
        }
    }
    res->final = sol;
}

static void check_results(const nmea_result_t *r)
{
    /* 10 эпох (одна со статусом V) + RMC южного/западного полушарий.
       Не должны считаться: неверная контрольная сумма, обрывок строки, мусор. */
    CHECK(r->rmc_ok == 11);

    CHECK(r->after_epoch5.valid);
    CHECK(r->after_epoch5.lat_1e7 == 537140943);
    CHECK(r->after_epoch5.lon_1e7 == 284020575);

    /* RMC со статусом V: валидность сброшена, прошлые координаты не затёрты нулями. */
    CHECK(!r->after_void.valid);
    CHECK(r->after_void.lat_1e7 == 537140943);

    CHECK(r->after_epoch9.valid);
    CHECK(r->after_epoch9.lat_1e7 == 537144943);
    CHECK(r->after_epoch9.lon_1e7 == 284020575);
    CHECK(r->after_epoch9.speed_knots == 159);
    CHECK(r->after_epoch9.course_valid);
    CHECK(r->after_epoch9.course_deg_100 == 9000);
    CHECK(r->after_epoch9.alt_m == 187);
    CHECK(r->after_epoch9.satellites_tracked == 9);
    CHECK(r->after_epoch9.fix_quality == 1);
    CHECK(r->after_epoch9.fix_type == 3);
    CHECK(r->after_epoch9.hdop_100 == 90);
    CHECK(r->after_epoch9.pdop_100 == 180);
    CHECK(r->after_epoch9.vdop_100 == 150);
    CHECK(r->after_epoch9.year == 26 && r->after_epoch9.month == 10 && r->after_epoch9.day == 19);
    CHECK(r->after_epoch9.hours == 10 && r->after_epoch9.minutes == 0 && r->after_epoch9.seconds == 9);

    /* Южное и западное полушария: знак координаты; курса нет. */
    CHECK(r->final.valid);
    CHECK(r->final.lat_1e7 == -338688000);
    CHECK(r->final.lon_1e7 == -1512050000);
    CHECK(!r->final.course_valid);
    CHECK(r->final.minutes == 1 && r->final.seconds == 0);
}

static void test_nmea_recorded_log(void)
{
    size_t len = 0;
    uint8_t *log = load_log(&len);
    CHECK(log != NULL);
    if (!log) { return; }

    nmea_result_t res;
    run_parser(log, len, 1U, &res, false);          /* по одному байту */
    check_results(&res);

    nmea_result_t res2;
    run_parser(log, len, 100U, &res2, true);        /* через кольцевой буфер, пачками */
    check_results(&res2);
    CHECK(memcmp(&res.final, &res2.final, sizeof res.final) == 0);
    free(log);
}

static void test_nmea_robustness(void)
{
    purrgo_gnss_solution_t sol;
    memset(&sol, 0, sizeof sol);

    /* NULL/пустые/мусорные строки не меняют решение и не падают. */
    CHECK(!purrgo_gnss_process_nmea("", &sol));
    CHECK(!purrgo_gnss_process_nmea("$", &sol));
    CHECK(!purrgo_gnss_process_nmea("$GNRMC", &sol));
    CHECK(!purrgo_gnss_process_nmea("$GNRMC,,,,,,,,,,,*", &sol));
    CHECK(!purrgo_gnss_process_nmea("$GNRMC,100000.00,A,5342.81566,N,02824.12345,E,1.50,90.00,191026,,,A*00", &sol));
    CHECK(!sol.valid && sol.lat_1e7 == 0);

    /* Переполнение строки (> 128 символов): парсер не выходит за буфер,
       хвост уже не пройдёт проверку контрольной суммы. */
    purrgo_gnss_parser_t p;
    purrgo_gnss_parser_init(&p);
    int done = 0;
    for (int i = 0; i < 1000; i++) { done += purrgo_gnss_parser_feed(&p, (uint8_t)'A'); }
    CHECK(done == 0);
    CHECK(p.length < sizeof p.line);
    CHECK(purrgo_gnss_parser_feed(&p, '\n'));
    CHECK(!purrgo_gnss_process_nmea(p.line, &sol));

    /* Усечение валидной RMC в любой точке: ни падений, ни принятия. */
    const char *good = "$GNRMC,100000.00,A,5342.81566,N,02824.12345,E,1.50,90.00,191026,,,A*70";
    size_t n = strlen(good);
    for (size_t k = 0; k < n; k++) {
        char cut[128];
        memcpy(cut, good, k);
        cut[k] = '\0';
        purrgo_gnss_solution_t s2;
        memset(&s2, 0, sizeof s2);
        CHECK(!purrgo_gnss_process_nmea(cut, &s2));
        CHECK(!s2.valid);
    }
    purrgo_gnss_solution_t s3;
    memset(&s3, 0, sizeof s3);
    CHECK(purrgo_gnss_process_nmea(good, &s3));
    CHECK(s3.valid && s3.lat_1e7 == 537135943);
}

/* GSA от разных систем: сводка по системам, а не "последний победил" (раздел 2, GNSS). */
static bool feed(const char *body, purrgo_gnss_solution_t *s)
{
    char line[160];
    unsigned c = 0;
    for (const char *q = body; *q; q++) { c ^= (unsigned char)*q; }
    snprintf(line, sizeof line, "$%s*%02X", body, c);
    return purrgo_gnss_process_nmea(line, s);
}

static const char *RMC_OK = "GNRMC,120000.00,A,5342.1234,N,02824.5678,E,0.5,90.0,191026,,,A";

static void test_multi_gsa(void)
{
    purrgo_gnss_solution_t s;

    /* GP: 3D, GL: нет фикса (99.0) - фикс не пропадает, DOP берётся из GP. */
    purrgo_gnss_adapter_reset();
    memset(&s, 0, sizeof s);
    (void)feed("GPGSA,A,3,01,02,03,04,05,06,07,08,09,,,,1.8,0.9,1.5", &s);
    CHECK(s.fix_type == 3 && s.hdop_100 == 90 && s.pdop_100 == 180 && s.vdop_100 == 150);
    (void)feed("GLGSA,A,1,,,,,,,,,,,,,99.0,99.0,99.0", &s);
    CHECK(s.fix_type == 3 && s.hdop_100 == 90);
    /* Порядок обратный: GL приходит первой. */
    purrgo_gnss_adapter_reset();
    memset(&s, 0, sizeof s);
    (void)feed("GLGSA,A,1,,,,,,,,,,,,,99.0,99.0,99.0", &s);
    CHECK(s.fix_type == 1);
    (void)feed("GPGSA,A,3,01,02,03,04,05,06,07,08,09,,,,1.8,0.9,1.5", &s);
    CHECK(s.fix_type == 3 && s.hdop_100 == 90);

    /* GN с идентификатором системы NMEA 4.1: GPS 3D, BDS без спутников. */
    purrgo_gnss_adapter_reset();
    memset(&s, 0, sizeof s);
    (void)feed("GNGSA,A,3,01,02,03,04,05,06,,,,,,,1.8,1.2,1.3,1", &s);
    (void)feed("GNGSA,A,1,,,,,,,,,,,,,99.0,99.0,99.0,4", &s);
    CHECK(s.fix_type == 3 && s.hdop_100 == 120);
    /* То же, другой порядок. */
    purrgo_gnss_adapter_reset();
    memset(&s, 0, sizeof s);
    (void)feed("GNGSA,A,1,,,,,,,,,,,,,99.0,99.0,99.0,4", &s);
    (void)feed("GNGSA,A,3,01,02,03,04,05,06,,,,,,,1.8,1.2,1.3,1", &s);
    CHECK(s.fix_type == 3 && s.hdop_100 == 120);

    /* 2D у одной системы, 3D у другой -> 3D; при равенстве берётся меньший HDOP. */
    purrgo_gnss_adapter_reset();
    memset(&s, 0, sizeof s);
    (void)feed("GPGSA,A,2,01,02,03,,,,,,,,,,3.0,2.0,2.5", &s);
    CHECK(s.fix_type == 2);
    (void)feed("BDGSA,A,3,01,02,03,04,,,,,,,,,2.0,1.5,1.6", &s);
    CHECK(s.fix_type == 3 && s.hdop_100 == 150);
    (void)feed("GAGSA,A,3,01,02,03,04,,,,,,,,,1.7,1.1,1.2", &s);
    CHECK(s.fix_type == 3 && s.hdop_100 == 110);

    /* Все системы сообщают "нет фикса" - фикс потерян. */
    (void)feed("GPGSA,A,1,,,,,,,,,,,,,99.0,99.0,99.0", &s);
    (void)feed("BDGSA,A,1,,,,,,,,,,,,,99.0,99.0,99.0", &s);
    (void)feed("GAGSA,A,1,,,,,,,,,,,,,99.0,99.0,99.0", &s);
    CHECK(s.fix_type == 1);

    /* Система замолчала: через GSA_STALE_EPOCHS (3) фраз RMC её GSA устаревает. */
    purrgo_gnss_adapter_reset();
    memset(&s, 0, sizeof s);
    (void)feed("GPGSA,A,3,01,02,03,04,05,06,07,08,09,,,,1.8,0.9,1.5", &s);
    CHECK(feed(RMC_OK, &s) && s.fix_type == 3);
    CHECK(feed(RMC_OK, &s) && s.fix_type == 3);
    CHECK(feed(RMC_OK, &s) && s.fix_type == 1);

    /* Без единой GSA fix_type не трогается (приёмник их не шлёт). */
    purrgo_gnss_adapter_reset();
    memset(&s, 0, sizeof s);
    s.fix_type = 2;
    CHECK(feed(RMC_OK, &s) && s.fix_type == 2);
}

int main(void)
{
    test_ring_init();
    test_ring_head_from_ndtr();
    test_ring_no_loss();
    test_ring_interleaved();
    test_ring_documented_limit();
    test_ring_reset_after_error();
    test_nmea_recorded_log();
    test_nmea_robustness();
    test_multi_gsa();
    return test_finish("test_gnss");
}
