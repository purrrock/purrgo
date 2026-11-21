#include "gnss/gnss_feed.h"

#include "purrgo/gnss_adapter.h"
#include "purrgo/gnss_ubx.h"
#include "purrgo/logger.h"
#include "purrgo/system_time.h"

/*
 * Возраст метки времени относительно now_ms. Метка может оказаться НОВЕЕ now_ms: главный
 * цикл берёт время такта в начале прохода, а байты GNSS разбираются позже (опрос дисплея,
 * вывод монитора), и кадр получает метку на 1 мс и более больше. Беззнаковая разность тогда
 * даёт ~4,29e9 и ложный таймаут ("GNSS fix timeout"): фикс пропадал на секунду и возвращался
 * со следующим кадром. Такая метка считается свежей (возраст 0).
 */
static uint32_t stamp_age_ms(uint32_t now_ms, uint32_t stamp_ms) {
    const int32_t diff = (int32_t)(now_ms - stamp_ms);

    return (diff > 0) ? (uint32_t)diff : 0U;
}

void purrgo_gnss_feed_init(purrgo_gnss_feed_t *f, uint32_t now_ms) {
    purrgo_gnss_parser_init(&f->nmea);
#if PURRGO_GNSS_UBX
    purrgo_ubx_parser_init(&f->ubx);
#endif
    f->last_rx_ms = now_ms;
    f->last_check_ms = now_ms;
}

void purrgo_gnss_feed_byte(purrgo_gnss_feed_t *f, uint8_t byte) {
#if PURRGO_GNSS_UBX
    /*
     * В потоке вперемешку NMEA-строки и бинарные кадры UBX. Кадр может начаться,
     * только пока NMEA-строка не начата (после её конца разборщик пуст): иначе
     * байт B5 внутри текста сорвал бы разбор. Принятый PVT, как и RMC, означает,
     * что приёмник жив (таймаут фикса).
     */
    if (f->nmea.length == 0U || purrgo_ubx_parser_active(&f->ubx)) {
        const purrgo_ubx_feed_t fed = purrgo_ubx_parser_feed(&f->ubx, byte);

        if (fed == PURRGO_UBX_FEED_FRAME) {
            const uint32_t now = purrgo_system_time_ms();

            if (purrgo_gnss_ubx_process_frame(f->ubx.msg_class, f->ubx.msg_id,
                                              f->ubx.payload, f->ubx.length,
                                              &f->solution, now)) {
                f->last_rx_ms = now;
                f->fix_seq++;
            }
            return;
        }
        if (fed == PURRGO_UBX_FEED_CONSUMED) {
            return;
        }
    }
#endif
    if (purrgo_gnss_parser_feed(&f->nmea, byte)) {
        if (purrgo_gnss_process_nmea(f->nmea.line, &f->solution)) {
            f->last_rx_ms = purrgo_system_time_ms();
            f->fix_seq++;
        }
        purrgo_gnss_parser_init(&f->nmea);
    }
}

void purrgo_gnss_feed_invalidate(purrgo_gnss_feed_t *f) {
    purrgo_gnss_solution_t *s = &f->solution;

    s->valid = false;
    s->course_valid = false;
    s->speed_knots = 0;
    s->fix_quality = 0;
    s->acc_valid = false;   // оценки точности относятся к потерянному решению
    /*
     * fix_type = 0 - "тип фикса неизвестен" (как после старта): значение 1 из GSA
     * ("нет фикса") выставляет только адаптер. Если здесь поставить 1, а приёмник
     * (или мок на PC) не шлёт GSA, то fix_type так и останется 1 и после появления
     * фикса фильтр качества логгера отбросит все точки: файл трека создан, а
     * точек в нём и на экране нет.
     */
    s->fix_type = 0;
}

void purrgo_gnss_feed_restart_silence(purrgo_gnss_feed_t *f, uint32_t now_ms) {
    f->last_rx_ms = now_ms;
}

void purrgo_gnss_feed_restart(purrgo_gnss_feed_t *f, uint32_t now_ms) {
    f->last_rx_ms = now_ms;
    f->last_check_ms = now_ms;
}

void purrgo_gnss_feed_check_timeout(purrgo_gnss_feed_t *f, uint32_t now_ms, uint32_t timeout_ms) {
    /* Главный цикл простоял: молчание приёмника оценить нельзя, даём новый срок. */
    if (stamp_age_ms(now_ms, f->last_check_ms) > (PURRGO_GNSS_FIX_TIMEOUT_MS / 2U)) {
        f->last_rx_ms = now_ms;
    }
    f->last_check_ms = now_ms;

    if (f->solution.valid && stamp_age_ms(now_ms, f->last_rx_ms) > timeout_ms) {
        PURRGO_LOGW("GNSS fix timeout\r\n");
        purrgo_gnss_feed_invalidate(f);
    }
}
