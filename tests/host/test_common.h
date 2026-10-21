/*
 * Общие макросы host-тестов: CHECK / XFAIL и итоговая сводка.
 * Каждый test_*.c собирается в отдельный исполняемый файл (см. run.sh).
 */
#ifndef PURRGO_TEST_COMMON_H
#define PURRGO_TEST_COMMON_H

#include <stdio.h>

static int g_checks, g_failures, g_xfail, g_xpass;

#define CHECK(c) do { g_checks++; if (!(c)) { g_failures++; \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

/*
 * Известный дефект (см. TODO_refact.md): условие пока НЕ выполняется (XFAIL, не
 * считается провалом). Когда дефект исправят и условие станет истинным, тест
 * сообщит XPASS - тогда макрос нужно заменить на CHECK.
 */
#define XFAIL(c, why) do { g_checks++; if (c) { g_xpass++; \
    printf("XPASS %s:%d: %s (%s) - дефект исправлен? замените на CHECK\n", \
           __FILE__, __LINE__, #c, why); } else { g_xfail++; } } while (0)

static inline int test_finish(const char *name)
{
    printf("%s: %d checks, %d failures, %d known issues (xfail), %d xpass\n",
           name, g_checks, g_failures, g_xfail, g_xpass);
    return g_failures ? 1 : 0;
}

#endif /* PURRGO_TEST_COMMON_H */
