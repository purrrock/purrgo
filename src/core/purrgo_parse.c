#include "purrgo/purrgo_parse.h"

int32_t purrgo_parse_int32(const char* str)
{
    int64_t res = 0;
    int32_t sign = 1;

    /*
     * Пропуск пробелов.
     */
    while (*str == ' ' || *str == '\t') {
        str++;
    }

    /*
     * Определение знака.
     */
    if (*str == '-') {
        sign = -1;
        str++;
    }
    else if (*str == '+') {
        str++;
    }

    /*
     * Чтение числовой части с накоплением в int64_t.
     */
    while (*str >= '0' && *str <= '9') {
        int32_t digit = *str - '0';

        if (res > INT32_MAX / 10 || (res == INT32_MAX / 10 && digit > (sign == 1 ? 7 : 8))) {
            return sign == 1 ? INT32_MAX : INT32_MIN;
        }

        res = res * 10 + digit;
        str++;
    }

    if (sign == -1) {
        res = -res;
    }

    return (int32_t)res;
}
