#ifndef PURRGO_FORMAT_H
#define PURRGO_FORMAT_H

#include <stddef.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Атрибут format: компилятор (-Wformat=2) проверяет соответствие спецификаторов
 * и типов аргументов в вызовах. На MinGW (GCC) нужен gnu_printf: у ms_printf
 * другие правила для %zu и %lld.
 */
#if defined(__GNUC__) || defined(__clang__)
#  if defined(__MINGW32__) && !defined(__clang__)
#    define PURRGO_PRINTF_FORMAT(fmt_idx, first_arg) \
        __attribute__((format(gnu_printf, fmt_idx, first_arg)))
#  else
#    define PURRGO_PRINTF_FORMAT(fmt_idx, first_arg) \
        __attribute__((format(printf, fmt_idx, first_arg)))
#  endif
#else
#  define PURRGO_PRINTF_FORMAT(fmt_idx, first_arg)
#endif

/*
 * printf без плавающей точки: см. комментарий к purrgo_vsnprintf() в purrgo_format.c.
 * Возвращает длину полной строки без '\0' (как C99 snprintf), даже если она не
 * поместилась в буфер.
 */
int purrgo_vsnprintf(char* buf, size_t size, const char* format, va_list args)
    PURRGO_PRINTF_FORMAT(3, 0);
int purrgo_snprintf(char* buf, size_t size, const char* format, ...)
    PURRGO_PRINTF_FORMAT(3, 4);

#ifdef __cplusplus
}
#endif

#endif // PURRGO_FORMAT_H
