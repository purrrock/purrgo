#ifndef PURRGO_BOOT_BANNER_H
#define PURRGO_BOOT_BANNER_H

#include <stdbool.h>
#include <purrgo/gfx_renderer.h>

/*
 * Загрузочный баннер: логотип PurrGO, номер версии и (в Debug-сборке) плашка
 * "DEBUG BUILD". Ядро только рисует в framebuffer; когда показывать баннер и как
 * долго держать, решает платформа:
 *
 *   - прошивка (stm32_app.c): рисует баннер сразу после инициализации дисплея,
 *     дожидается окончания полного обновления E-Ink и держит картинку ещё
 *     PURRGO_BOOT_BANNER_MS;
 *   - эмулятор: показывает баннер первые PURRGO_BOOT_BANNER_MS мс.
 *
 * Версия: cmake/PurrgoVersion.cmake (PURRGO_VERSION); ревизия git и признак Debug
 * подставляются при каждой сборке (см. там же).
 */

/* Сколько держать баннер на экране, мс (на E-Ink - после окончания обновления панели). */
#ifndef PURRGO_BOOT_BANNER_MS
#define PURRGO_BOOT_BANNER_MS 500u
#endif

/*
 * Нарисовать баннер на весь экран (белый фон) и больше ничего не делать: режим
 * дисплея и обновление панели - дело платформы. Рисует во весь контекст gfx,
 * область отсечения сбрасывает, цвета контекста не сохраняет.
 */
void purrgo_boot_banner_render(gfx_context_t *gfx);

/* Версия прошивки, например "0.1.0". Строка статическая, не NULL. */
const char *purrgo_boot_version(void);

/*
 * Ревизия git, например "3c144dc" или "3c144dc-dirty" (есть незакоммиченные
 * изменения); "nogit", если репозитория нет. Строка статическая, не NULL.
 */
const char *purrgo_boot_revision(void);

/* true в Debug-сборке (тип сборки CMake Debug): на баннере появляется "DEBUG BUILD". */
bool purrgo_boot_is_debug(void);

#endif /* PURRGO_BOOT_BANNER_H */
