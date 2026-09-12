#include "purrgo/purrgo_format.h"

static void write_char(char** buf, size_t* size, size_t* count, char c) {
    if (*size > 1) {
        **buf = c;
        (*buf)++;
        (*size)--;
    }
    (*count)++;
}

static void write_int(char** buf, size_t* size, size_t* count, long long val, int base, int width, char pad_char, int uppercase, int is_signed, int left_justify) {
    char num_buf[32];
    int pos = 0;
    unsigned long long uval;
    int is_neg = 0;

    if (is_signed && val < 0) {
        is_neg = 1;
        uval = (unsigned long long)(-val);
    } else {
        uval = (unsigned long long)val;
    }

    if (uval == 0) {
        num_buf[pos++] = '0';
    } else {
        while (uval > 0) {
            int digit = uval % base;
            if (digit < 10) {
                num_buf[pos++] = '0' + digit;
            } else {
                num_buf[pos++] = (uppercase ? 'A' : 'a') + (digit - 10);
            }
            uval /= base;
        }
    }

    int num_len = pos;
    if (is_neg) num_len++;

    int pad = 0;
    if (width > num_len) {
        pad = width - num_len;
    }

    if (!left_justify) {
        if (pad_char == '0' && is_neg) {
            write_char(buf, size, count, '-');
            is_neg = 0;
        }
        for (int i = 0; i < pad; i++) {
            write_char(buf, size, count, pad_char);
        }
    }

    if (is_neg) {
        write_char(buf, size, count, '-');
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

int purrgo_vsnprintf(char* buf, size_t size, const char* format, va_list args) {
    size_t count = 0;

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

        if (*format == '-') {
            left_justify = 1;
            format++;
        }

        if (*format == '0') {
            if (!left_justify) pad_char = '0';
            format++;
        }

        int width = 0;
        while (*format >= '0' && *format <= '9') {
            width = width * 10 + (*format - '0');
            format++;
        }

        int precision = -1;
        int is_long = 0;

        if (*format == '.') {
            format++;
            precision = 0;
            if (*format == '*') {
                precision = va_arg(args, int);
                format++;
            } else {
                while (*format >= '0' && *format <= '9') {
                    precision = precision * 10 + (*format - '0');
                    format++;
                }
            }
        }

        if (*format == 'l') {
            is_long = 1;
            format++;
        }

        if (*format == 's') {
            const char* s = va_arg(args, const char*);
            if (!s) s = "(null)";

            int len = 0;
            while (s[len] && (precision < 0 || len < precision)) len++;

            int pad = 0;
            if (width > len) {
                pad = width - len;
            }

            if (!left_justify) {
                for (int i = 0; i < pad; i++) write_char(&buf, &size, &count, ' ');
            }

            for (int i = 0; i < len; i++) {
                write_char(&buf, &size, &count, s[i]);
            }

            if (left_justify) {
                for (int i = 0; i < pad; i++) write_char(&buf, &size, &count, ' ');
            }
            format++;
        } else if (*format == 'c') {
            char c = (char)va_arg(args, int);
            if (!left_justify && width > 1) {
                for (int i = 0; i < width - 1; i++) write_char(&buf, &size, &count, ' ');
            }
            write_char(&buf, &size, &count, c);
            if (left_justify && width > 1) {
                for (int i = 0; i < width - 1; i++) write_char(&buf, &size, &count, ' ');
            }
            format++;
        } else if (*format == 'd' || *format == 'i') {
            long val = is_long ? va_arg(args, long) : va_arg(args, int);
            write_int(&buf, &size, &count, val, 10, width, pad_char, 0, 1, left_justify);
            format++;
        } else if (*format == 'u') {
            unsigned long val = is_long ? va_arg(args, unsigned long) : va_arg(args, unsigned int);
            write_int(&buf, &size, &count, val, 10, width, pad_char, 0, 0, left_justify);
            format++;
        } else if (*format == 'x') {
            unsigned long val = is_long ? va_arg(args, unsigned long) : va_arg(args, unsigned int);
            write_int(&buf, &size, &count, val, 16, width, pad_char, 0, 0, left_justify);
            format++;
        } else if (*format == 'X') {
            unsigned long val = is_long ? va_arg(args, unsigned long) : va_arg(args, unsigned int);
            write_int(&buf, &size, &count, val, 16, width, pad_char, 1, 0, left_justify);
            format++;
        } else {
            write_char(&buf, &size, &count, '%');
            if (is_long) {
                write_char(&buf, &size, &count, 'l');
            }
            if (*format) {
                write_char(&buf, &size, &count, *format);
                format++;
            }
        }
    }

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
