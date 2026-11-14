# E-Ink driver (epd4g)

Дисплей и его параметры: [`hardware.md`](hardware.md).


Панель управляется библиотекой [`epd4g`](https://github.com/purrrock/eink_part_refresh)
(сабмодуль `third_party/eink_part_refresh`, используется только каталог `epd4g/`). Устройство библиотеки,
API, настройки и подключение к проекту описаны в её [README](https://github.com/purrrock/eink_part_refresh#readme); ниже только то, что относится к PurrGO.

В `stm32/CMakeLists.txt` подключены `add_subdirectory(.../epd4g)` и `epd4g` в
`target_link_libraries` ([как подключать](https://github.com/purrrock/eink_part_refresh#подключение-к-своему-проекту)); метки CubeMX `EINK_CS/DC/RST/BUSY`
и `hspi1` используются штатным портом ([настройка CubeMX](https://github.com/purrrock/eink_part_refresh#настройка-stm32cubemx-для-готового-порта)).
Определение `EPD4G_BW_THRESHOLD=3` сохраняет семантику 1-bit режима: белый → белый, любой серый → чёрный
(по умолчанию порог 2, светло-серый стал бы белым; см. [конфигурацию](https://github.com/purrrock/eink_part_refresh#конфигурация)). Библиотека не входит в
`purrgo_sources_warnings`; она проверена с `-Wall -Wextra -Wconversion -Wshadow`.

| Файл | Роль |
|---|---|
| `include/purrgo/display_hal.h` | `display_set_mode(DISPLAY_MODE_GRAY4 / DISPLAY_MODE_MONO)`; `display_refresh()` и `display_flush()` неблокирующие, `display_sleep()` блокирующая |
| `src/core/app_ui.c` | режим выбирается по экрану: карта и экран выключения — 4 gray, остальное — 1 bit |
| `common/Src/display_eink.c` | очередь обновлений поверх epd4g |
| `common/Src/display_stm32.c` | framebuffer, накопление области, `display_stm32_poll()`, `display_stm32_ready_for_render()` |
| `common/Src/stm32_app.c` | `display_stm32_poll()` на каждом проходе цикла; рендер только при `display_stm32_ready_for_render()` |
| `common/Src/eink_test.c` | самотесты на железе (`PURRGO_EINK_SELFTEST_4GRAY`, `PURRGO_EINK_SELFTEST_PARTIAL`) |
| `common/Src/hw_test.c` | аппаратные замеры вместо навигатора (`-DPURRGO_HW_TEST=SD_SPEED`), см. `docs/hardware.md` |
| `tests/host/test_display_eink.c` | host-тест очереди на модели панели (нужен сабмодуль) |

Framebuffer PurrGO (176×264, 2 бита на пиксель, 0 = чёрный … 3 = белый) совпадает с буфером epd4g
([формат](https://github.com/purrrock/eink_part_refresh#формат-буфера-кадра)): конвертации и поворота нет. Меню в 1-bit режиме выводятся прямо из него
(`EPD4G_SRC_2BPP`, см. [1-bit режим](https://github.com/purrrock/eink_part_refresh#1-bit-режим)), второй буфер не нужен.

Логика `display_eink.c`:

* Одно обновление в работе. `display_refresh()` / `display_flush()` только ставят запрос; пока идёт
  предыдущее, запросы сливаются (полное перекрывает частичное, области объединяются). Запуск из
  очереди — в `display_eink_poll()`. Простого вызова `epd4g_start_*` из `display_flush()` недостаточно:
  `epd4g_data_busy() == 0` не значит `epd4g_is_busy() == 0`, и `epd4g_start_*` вернёт `EPD4G_ERR_BUSY`.
* Рендер запрещён, пока `epd4g_data_busy()`: передача кадра по SPI (десятки мс), а для 1-bit partial
  ещё и до конца волны (~0,3 с: окно повторно пишется в RAM `0x24`, см. ниже). Волна 4 gray
  (секунды) рендеру не мешает.
* Первое обновление, смена режима и потеря картинки (ошибка) дают полное обновление. Смена режима
  панели требует `epd4g_init()` / `epd4g_bw_init()` и полного обновления ([переключение режимов](https://github.com/purrrock/eink_part_refresh#1-bit-режим));
  слой PurrGO делает это автоматически.
* Лимит частичных подряд контролирует сама библиотека: по достижении лимита очередной запрос она выполняет
  как полное обновление того же кадра ([`EPD4G_PARTIALS_BEFORE_FULL`, `EPD4G_BW_PARTIALS_BEFORE_FULL`](https://github.com/purrrock/eink_part_refresh#конфигурация)).
  Слой PurrGO счётчика не ведёт: `display_eink_partial_count()` читает `epd4g_partials_since_full()`, лимиты
  задаются через `epd4g_set_partials_before_full()` в `display_eink_init()` и
  `display_eink_set_partial_limits()`. Для 4 gray `prev` копируется как и раньше: при полном обновлении на
  панели тот же кадр.
* 4 gray: перерисовываются только пиксели, отличающиеся от буфера `prev` (11616 байт, «что на
  панели»). Сравнивается весь кадр, а не накопленная область, и копируется весь буфер:
  `epd4g_fb_copy_rect()` копирует целые байтовые столбцы (по 4 пикселя), поэтому по произвольной
  области `prev` получил бы пиксели, которые на панель не выводились. `prev` обновляется сразу после
  старта задания.
  Если отличий нет (`changed == 0`), библиотека ставит пустое задание ([`epd4g_start_noop()`](https://github.com/purrrock/eink_part_refresh#неблокирующий-режим));
  слой PurrGO обязан дождаться его завершения: иначе библиотека остаётся занятой и следующий
  `epd4g_start_*()` вернёт `EPD4G_ERR_BUSY`.
* Сдвиг карты (кнопки, автоследование, масштаб) на экране карты тоже выводится этим частичным обновлением:
  `ui_render_map()` после полной перерисовки карты регистрирует область (`display_refresh_region()`)
  вместо `purrgo_app_ui_request_full_refresh()`. Вход на экран карты, смена режима панели и лимит
  частичных подряд по-прежнему дают полное обновление. Отключается `PURRGO_MAP_SHIFT_PARTIAL_REFRESH=0`.
* 1 bit: частичное обновление окна накопленной области.
* Питание панели снимает библиотека ([`EPD4G_AUTO_POWEROFF_MS`](https://github.com/purrrock/eink_part_refresh#конфигурация), [как это работает](https://github.com/purrrock/eink_part_refresh#как-это-работает)):
  молчаливое задание power off идёт ~0,3 с с `epd4g_is_busy() == 1`. Поэтому `display_eink_poll()` вызывает
  `epd4g_poll()` на каждом проходе, даже когда своей операции нет, и не запускает новое задание, пока
  библиотека занята.
* Простой 1 с → `epd4g_start_sleep()` (только команда deep sleep: питание уже снято). Следующее обновление будит
  панель: в 4 gray через `epd4g_init()` (картинка на панели сохраняется, обновление остаётся
  частичным), в 1 bit — сам `epd4g_bw_partial()` ([сон в 1-bit режиме](https://github.com/purrrock/eink_part_refresh#1-bit-режим)). Повторный
  `epd4g_start_sleep()` в 4 gray возвращает `ERR_NOT_INIT` (в 1 bit — `OK`), слой PurrGO это учитывает.
* Ошибка (например, таймаут BUSY): панель считается потерянной, запрос ставится заново как полное
  обновление, повтор через 2 с.
* `display_sleep()` (выключение питания): ждёт все поставленные обновления, в том числе экран
  выключения, затем `epd4g_wait()` (доводит до конца автоматическое выключение питания) и `epd4g_sleep()`; ограничение 30 с. Последовательность в ядре:
  `display_refresh()` → `display_sleep()` (`src/core/power.c`), `power_stm32.c` менять не нужно.
  Экран выключения — карта, рисуется без смены экрана, поэтому `purrgo_app_ui_render_power_off()`
  сам вызывает `display_set_mode(DISPLAY_MODE_GRAY4)`.
* Маршрутный компьютер после отрисовки значений регистрирует область всего экрана
  (`display_refresh_region`), иначе на панель уходит только строка подсказок кнопок.

SPI панели: делитель `/8` (`PURRGO_EINK_SPI_PRESCALER`), 10 МГц при ядре SPI1 80 МГц.

Остаточные строки в 1-bit partial (инвертированные строки в меню, копии маркёра на карте) исправлены в
библиотеке (`EPD4G_BW_RESYNC=1`), причина и механизм описаны в [README, 1-bit режим](https://github.com/purrrock/eink_part_refresh#1-bit-режим).
Следствие для PurrGO: `epd4g_data_busy()` для 1-bit partial остаётся 1 до конца повторной записи, рендер
откладывается на ~0,3 с. Для 4 gray проблемы нет (каждый проход пишет обе плоскости целиком). Запись только в
`0x26` не помогает.

Не реализовано (по необходимости):

* прерывание EXTI по BUSY вместо опроса (главный цикл просыпается по SysTick 1 кГц);
* SPI через DMA: не нужен, пока `epd4g_poll()` укладывается в миллисекунду (CS и D/C
  переключаются пачками, а не на каждый байт);
* градации серого в 1-bit режиме (серые заливки `landuse` дают чёрное), см. `TODO.md`.
