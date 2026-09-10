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
