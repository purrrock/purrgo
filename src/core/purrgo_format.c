#include "purrgo/purrgo_format.h"

#include <stddef.h>
#include <stdint.h>

static void write_char(char** buf, size_t* size, size_t* count, char c) {
    if (*size > 1) {
        **buf = c;
        (*buf)++;
        (*size)--;
    }
    (*count)++;
}

/*
 * Вывод целого числа.
 *
 *   uval/is_neg  - модуль и знак (модуль считается без переполнения для LLONG_MIN);
 *   precision    - минимальное число цифр (-1 - не задано; при заданной точности
 *                  выравнивание нулями '0' игнорируется, как в printf);
 *   sign_char    - '+' или ' ' для неотрицательных (0 - без знака).
 */
static void write_int(char** buf, size_t* size, size_t* count,
                      unsigned long long uval, int is_neg, int base, int width,
                      int precision, char pad_char, int uppercase,
                      char sign_char, int left_justify) {
    char num_buf[32];
    int pos = 0;

    if (uval == 0) {
        if (precision != 0) {
            num_buf[pos++] = '0';
        }
    } else {
        while (uval > 0) {
            unsigned digit = (unsigned)(uval % (unsigned long long)base);
            if (digit < 10) {
                num_buf[pos++] = (char)('0' + digit);
            } else {
                num_buf[pos++] = (char)((uppercase ? 'A' : 'a') + (digit - 10));
            }
            uval /= (unsigned long long)base;
        }
    }

    int zeros = 0;
    if (precision > pos) {
        zeros = precision - pos;
    }

    char sign = 0;
    if (is_neg) {
        sign = '-';
    } else if (sign_char != 0) {
        sign = sign_char;
    }

    int num_len = pos + zeros + (sign != 0 ? 1 : 0);

    int pad = 0;
    if (width > num_len) {
        pad = width - num_len;
    }

    if (precision >= 0) {
        pad_char = ' ';
    }

    if (!left_justify) {
        if (pad_char == '0' && sign != 0) {
            write_char(buf, size, count, sign);
            sign = 0;
        }
        for (int i = 0; i < pad; i++) {
            write_char(buf, size, count, pad_char);
        }
    }

    if (sign != 0) {
        write_char(buf, size, count, sign);
    }

    for (int i = 0; i < zeros; i++) {
        write_char(buf, size, count, '0');
    }

    for (int i = pos - 1; i >= 0; i--) {
        write_char(buf, size, count, num_buf[i]);
    }

    if (left_justify) {
        for (int i = 0; i < pad; i++) {
            write_char(buf, size, count, ' ');
        }
    }
}

/* Размер аргумента целого спецификатора. */
typedef enum {
    LEN_INT = 0,
    LEN_CHAR,       /* hh */
    LEN_SHORT,      /* h  */
    LEN_LONG,       /* l  */
    LEN_LLONG,      /* ll, j */
    LEN_SIZE        /* z, t */
} len_mod_t;

static unsigned long long fetch_unsigned(va_list* args, len_mod_t len) {
    switch (len) {
        case LEN_LONG:  return (unsigned long long)va_arg(*args, unsigned long);
        case LEN_LLONG: return (unsigned long long)va_arg(*args, unsigned long long);
        case LEN_SIZE:  return (unsigned long long)va_arg(*args, size_t);
        case LEN_CHAR:  return (unsigned char)va_arg(*args, unsigned int);
        case LEN_SHORT: return (unsigned short)va_arg(*args, unsigned int);
        default:        return (unsigned long long)va_arg(*args, unsigned int);
    }
}

static long long fetch_signed(va_list* args, len_mod_t len) {
    switch (len) {
        case LEN_LONG:  return (long long)va_arg(*args, long);
        case LEN_LLONG: return (long long)va_arg(*args, long long);
        case LEN_SIZE:  return (long long)(ptrdiff_t)va_arg(*args, ptrdiff_t);
        case LEN_CHAR:  return (signed char)va_arg(*args, int);
        case LEN_SHORT: return (short)va_arg(*args, int);
        default:        return (long long)va_arg(*args, int);
    }
}

/*
 * Подмножество printf без плавающей точки (ядро целочисленное, soft-float не
 * подключается):
 *
 *   флаги       - 0 + пробел
 *   ширина      - число или *
 *   точность    - .число или .* (для s - максимум символов, для целых - минимум цифр)
 *   длина       - hh h l ll j z t
 *   конверсии   - d i u x X c s p %
 *
 * Плавающие конверсии (f F e E g G a A) не поддерживаются: аргумент double
 * снимается из va_list, чтобы следующие аргументы не сдвигались, и выводится '?'.
 * Прочие неизвестные конверсии выводятся как есть ('%' и символ), аргумент
 * не снимается (его тип неизвестен). Включённый -Wformat (см. атрибут в
 * purrgo_format.h) ловит такие вызовы при сборке.
 */
int purrgo_vsnprintf(char* buf, size_t size, const char* format, va_list args) {
    size_t count = 0;
    va_list ap;

    va_copy(ap, args);

    if (size == 0) buf = NULL;

    while (*format) {
        if (*format != '%') {
            write_char(&buf, &size, &count, *format);
            format++;
            continue;
        }

        format++;
        if (*format == '\0') break;

        if (*format == '%') {
            write_char(&buf, &size, &count, '%');
            format++;
            continue;
        }

        int left_justify = 0;
        char pad_char = ' ';
        char sign_char = 0;

        for (;;) {
            if (*format == '-') {
                left_justify = 1;
            } else if (*format == '0') {
                pad_char = '0';
            } else if (*format == '+') {
                sign_char = '+';
            } else if (*format == ' ') {
                if (sign_char == 0) sign_char = ' ';
            } else {
                break;
            }
            format++;
        }

        if (left_justify) pad_char = ' ';

        int width = 0;
        if (*format == '*') {
            width = va_arg(ap, int);
            if (width < 0) {
                left_justify = 1;
                pad_char = ' ';
                width = -width;
            }
            format++;
        } else {
            while (*format >= '0' && *format <= '9') {
                width = width * 10 + (*format - '0');
                format++;
            }
        }

        int precision = -1;

        if (*format == '.') {
            format++;
            precision = 0;
            if (*format == '*') {
                precision = va_arg(ap, int);
                if (precision < 0) precision = -1;
                format++;
            } else {
                while (*format >= '0' && *format <= '9') {
                    precision = precision * 10 + (*format - '0');
                    format++;
                }
            }
        }

        len_mod_t len = LEN_INT;
        const char* spec_start = format;

        if (*format == 'h') {
            format++;
            if (*format == 'h') { len = LEN_CHAR; format++; } else { len = LEN_SHORT; }
        } else if (*format == 'l') {
            format++;
            if (*format == 'l') { len = LEN_LLONG; format++; } else { len = LEN_LONG; }
        } else if (*format == 'j') {
            len = LEN_LLONG;
            format++;
        } else if (*format == 'z' || *format == 't') {
            len = LEN_SIZE;
            format++;
        }

        char conv = *format;

        if (conv == 's') {
            const char* s = va_arg(ap, const char*);
            if (!s) s = "(null)";

            int slen = 0;
            while (s[slen] && (precision < 0 || slen < precision)) slen++;

            int pad = 0;
            if (width > slen) {
                pad = width - slen;
            }

            if (!left_justify) {
                for (int i = 0; i < pad; i++) write_char(&buf, &size, &count, ' ');
            }

            for (int i = 0; i < slen; i++) {
                write_char(&buf, &size, &count, s[i]);
            }

            if (left_justify) {
                for (int i = 0; i < pad; i++) write_char(&buf, &size, &count, ' ');
            }
            format++;
        } else if (conv == 'c') {
            char c = (char)va_arg(ap, int);
            if (!left_justify && width > 1) {
                for (int i = 0; i < width - 1; i++) write_char(&buf, &size, &count, ' ');
            }
            write_char(&buf, &size, &count, c);
            if (left_justify && width > 1) {
                for (int i = 0; i < width - 1; i++) write_char(&buf, &size, &count, ' ');
            }
            format++;
        } else if (conv == 'd' || conv == 'i') {
            long long val = fetch_signed(&ap, len);
            int is_neg = val < 0;
            /* Модуль через беззнаковое отрицание: корректно и для LLONG_MIN. */
            unsigned long long uval = is_neg
                ? (0ULL - (unsigned long long)val)
                : (unsigned long long)val;
            write_int(&buf, &size, &count, uval, is_neg, 10, width, precision,
                      pad_char, 0, sign_char, left_justify);
            format++;
        } else if (conv == 'u' || conv == 'x' || conv == 'X') {
            unsigned long long val = fetch_unsigned(&ap, len);
            int base = (conv == 'u') ? 10 : 16;
            write_int(&buf, &size, &count, val, 0, base, width, precision,
                      pad_char, conv == 'X', 0, left_justify);
            format++;
        } else if (conv == 'p') {
            unsigned long long val = (unsigned long long)(uintptr_t)va_arg(ap, void*);
            write_char(&buf, &size, &count, '0');
            write_char(&buf, &size, &count, 'x');
            write_int(&buf, &size, &count, val, 0, 16, 0, 1, '0', 0, 0, 0);
            format++;
        } else if (conv == 'f' || conv == 'F' || conv == 'e' || conv == 'E' ||
                   conv == 'g' || conv == 'G' || conv == 'a' || conv == 'A') {
            /* Не поддерживается: снимаем double, чтобы не сдвигать остальные аргументы. */
            (void)va_arg(ap, double);
            write_char(&buf, &size, &count, '?');
            format++;
        } else {
            /* Неизвестная конверсия: выводим как есть, аргумент не снимаем. */
            write_char(&buf, &size, &count, '%');
            for (const char* q = spec_start; q < format; q++) {
                write_char(&buf, &size, &count, *q);
            }
            if (*format) {
                write_char(&buf, &size, &count, *format);
                format++;
            }
        }
    }

    va_end(ap);

    if (size > 0 && buf != NULL) {
        *buf = '\0';
    }

    return (int)count;
}

int purrgo_snprintf(char* buf, size_t size, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int ret = purrgo_vsnprintf(buf, size, format, args);
    va_end(args);
    return ret;
}
