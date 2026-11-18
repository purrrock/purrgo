#include "purrgo/logger.h"
#include <stdio.h>
#include <stdarg.h>

void purrgo_logger_init(void) {
    // No-op for PC
}

void purrgo_logger_write(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fflush(stderr);
}

/* Запись лога на SD на PC не поддерживается: флаг только запоминается (меню, тесты). */
static bool s_sd_log_enabled;

void purrgo_logger_sd_set_enabled(bool enabled) {
    s_sd_log_enabled = enabled;
}

void purrgo_logger_sd_shutdown(void) {
    s_sd_log_enabled = false;
}

bool purrgo_logger_sd_enabled(void) {
    return s_sd_log_enabled;
}

void purrgo_logger_sd_service(uint32_t now_ms) {
    (void)now_ms;
}
