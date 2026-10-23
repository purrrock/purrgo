#include "purrgo/power.h"
#include "purrgo/app_ui.h"
#include "purrgo/config.h"
#include "purrgo/display_hal.h"
#include "purrgo/logger.h"
#include "purrgo/map.h"
#include "purrgo/track_logger.h"

#include <stdbool.h>

void purrgo_system_power_off(void)
{
    PURRGO_LOG("Powering off...\n\r");

    /*
     * 1. Финальный экран: последняя карта с треком и маркером и инверсные
     * строки "POWER OFF". Рисуется во framebuffer ДО остановки логгера
     * (трек - в его RAM-буфере) и до размонтирования SD (с неё читается
     * карта). На дисплей экран уходит позже, после сохранения данных:
     * полный refresh e-ink занимает секунды, а данные важнее.
     */
    const bool power_off_screen = purrgo_app_ui_render_power_off();

    /* 2. Остановить запись трека: сброс буфера и закрытие файла. */
    purrgo_logger_stop();

    /* 3. Сохранить конфигурацию (временный файл, sync, replace). */
    (void)purrgo_config_save();

    /* 4. Закрыть файлы карты: освободить структуры ФС до размонтирования. */
    purrgo_map_shutdown();

    /* 5. Размонтировать носитель (платформа). */
    purrgo_platform_storage_unmount();

    /*
     * 6. Вывести финальный экран (на e-ink изображение остаётся без питания)
     * и перевести дисплей в спящий режим.
     */
    if (power_off_screen) {
        display_refresh();
        PURRGO_LOG("Power-off screen shown\r\n");
    }
    display_sleep();
    PURRGO_LOG("Display off\r\n");

    /* 7. Низкое энергопотребление (платформа). На железе не возвращается. */
    purrgo_platform_enter_standby();
}
