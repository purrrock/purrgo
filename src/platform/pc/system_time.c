/*
 * Системное время PC: монотонные миллисекунды от первого вызова.
 *
 * Раньше использовался clock() - это ВРЕМЯ ПРОЦЕССОРА (на Linux/macOS), которое
 * не идёт, пока приложение спит в SDL_Delay(). Из-за этого отметки времени ядра
 * (последний RMC, перерисовка) расходились с часами, по которым цикл эмулятора
 * вызывает purrgo_app_tick(), и фикс GNSS "истекал" по таймауту ("GNSS fix timeout")
 * при полностью исправном mock-источнике. purrgo_system_time_ms() - единственный
 * источник времени ядра: эмулятор обязан передавать в purrgo_app_tick() именно его.
 */
#define _POSIX_C_SOURCE 199309L

#include "purrgo/system_time.h"
#include <stdint.h>

#if defined(_WIN32)
#include <windows.h>

static uint64_t monotonic_ms(void) {
    return (uint64_t)GetTickCount64();
}
#else
#include <time.h>

static uint64_t monotonic_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}
#endif

uint32_t purrgo_system_time_ms(void) {
    static int started = 0;
    static uint64_t base_ms;

    if (!started) {
        base_ms = monotonic_ms();
        started = 1;
    }
    return (uint32_t)(monotonic_ms() - base_ms);
}
