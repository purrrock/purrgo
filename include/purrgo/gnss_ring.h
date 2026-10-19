#ifndef PURRGO_GNSS_RING_H
#define PURRGO_GNSS_RING_H

/*
 * Логика потребителя кольцевого DMA-буфера GNSS, не зависящая от платформы.
 *
 * DMA (producer) пишет байты в массив по кругу, потребитель держит индекс
 * tail. Положение producer определяется ТОЛЬКО по счётчику NDTR DMA-канала
 * (число байт, оставшихся до конца буфера): отдельный счётчик «оборотов» не
 * нужен, и поэтому не возникает рассогласования пары (wrap, NDTR).
 *
 * Модуль вынесен из apps/stm32/.../gnss_io.c, чтобы его можно было проверять
 * host-тестами (tests/host/test_gnss.c) без железа.
 *
 * Условие корректности: purrgo_gnss_ring_pop() должна вызываться чаще, чем
 * DMA заполняет весь буфер (при 9600 бод и 1024 байтах это ~1,07 с).
 * Переполнение (потребитель отстал на буфер и более) обнаружить нельзя:
 * данные при этом молча теряются.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    const uint8_t *buf;  /* буфер, в который пишет DMA */
    uint16_t size;       /* размер буфера, степень двойки (2..32768) */
    uint16_t tail;       /* индекс следующего байта для чтения */
} purrgo_gnss_ring_t;

/* Инициализация. false, если buf == NULL или size не степень двойки. */
static inline bool purrgo_gnss_ring_init(purrgo_gnss_ring_t *r,
                                         const uint8_t *buf, uint16_t size)
{
    if (r == NULL || buf == NULL || size < 2U || (size & (uint16_t)(size - 1U)) != 0U) {
        return false;
    }
    r->buf = buf;
    r->size = size;
    r->tail = 0U;
    return true;
}

/* Сбросить потребителя (после перезапуска DMA, когда producer снова с нуля). */
static inline void purrgo_gnss_ring_reset(purrgo_gnss_ring_t *r)
{
    r->tail = 0U;
}

/*
 * Индекс, в который DMA запишет СЛЕДУЮЩИЙ байт, по значению NDTR.
 *
 * В circular-режиме NDTR меняется в диапазоне 1..size и затем перезагружается
 * значением size. Значение 0 (кратковременное) и значения вне диапазона
 * трактуются как «буфер полностью свободен» (head = 0).
 */
static inline uint16_t purrgo_gnss_ring_head_from_ndtr(const purrgo_gnss_ring_t *r,
                                                       uint32_t ndtr)
{
    if (ndtr == 0U || ndtr > r->size) {
        ndtr = r->size;
    }
    return (uint16_t)((uint32_t)(r->size - ndtr) & (uint32_t)(r->size - 1U));
}

/* Забрать один байт. false, если новых данных нет (tail == head). */
static inline bool purrgo_gnss_ring_pop(purrgo_gnss_ring_t *r, uint16_t head,
                                        uint8_t *out)
{
    if (head == r->tail) {
        return false;
    }
    *out = r->buf[r->tail];
    r->tail = (uint16_t)((r->tail + 1U) & (uint16_t)(r->size - 1U));
    return true;
}

#endif /* PURRGO_GNSS_RING_H */
