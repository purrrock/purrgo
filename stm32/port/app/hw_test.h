#ifndef HW_TEST_H
#define HW_TEST_H

/*
 * Режимы аппаратного тестирования (замеры из TODO.md на реальном железе).
 *
 * Выбираются при сборке одним параметром CMake (см. stm32/CMakeLists.txt):
 *
 *     cmake --preset stm32u5-release -DPURRGO_HW_TEST=SD_SPEED
 *
 * Допустимые значения:
 *     (пусто)       обычная прошивка, главный цикл навигатора;
 *     SD_SPEED      скорость чтения microSD (сырые сектора и FatFs);
 *     SD_POWER      циклы выключения/включения питания microSD и повторной инициализации;
 *     GNSS_BBR      сохраняет ли GNSS-модуль BBR, часы и эфемериды при EN = Low и в PSMOO
 *                   (hw_test_gnss.c; вывод в UART1 и на дисплей);
 *     EINK_4GRAY    самотест 4-gray    (PURRGO_EINK_SELFTEST_4GRAY);
 *     EINK_PARTIAL  самотест partial   (PURRGO_EINK_SELFTEST_PARTIAL).
 *
 * В тестовой сборке main() после инициализации периферии CubeMX вызывает
 * purrgo_hw_test_run() и в главный цикл навигатора не попадает.
 * Результаты выводятся в UART1 (как лог) строками с префиксом "HWTEST" и
 * НЕ зависят от PURRGO_LOG_LEVEL: в Release-сборке тест тоже печатает.
 *
 * Чтобы добавить тест: имя в списке PURRGO_HW_TEST_NAMES в CMakeLists.txt,
 * ветка #elif в hw_test.c.
 */

/*
 * Выбор теста прямо в исходниках (без CMake): раскомментировать ОДНУ строку.
 * Именно здесь, а не в main.c: заголовок включают и main.c, и hw_test.c, так что
 * макрос виден обоим. Определения в main.c не работают: hw_test.c их не видит.
 * Если тест задан параметром CMake -DPURRGO_HW_TEST=..., строки ниже не трогать.
 */
// #define PURRGO_HW_TEST_SD_SPEED

#if defined(PURRGO_HW_TEST_SD_SPEED) && !defined(PURRGO_HW_TEST_ENABLED)
#define PURRGO_HW_TEST_ENABLED
#endif

#ifdef PURRGO_HW_TEST_ENABLED

/* Запускает выбранный тест. Не возвращает управление. */
void purrgo_hw_test_run(void) __attribute__((noreturn));

#ifdef PURRGO_HW_TEST_GNSS_BBR
/* Тест GNSS_BBR (hw_test_gnss.c); не возвращает управление. */
void purrgo_hw_test_gnss_bbr(void) __attribute__((noreturn));
#endif

#endif /* PURRGO_HW_TEST_ENABLED */

#endif /* HW_TEST_H */
