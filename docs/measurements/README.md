# Результаты замеров и логов

Здесь лежат результаты измерений на железе и логи. Описания процедур - в
[`../development.md`](../development.md) (раздел «Аппаратные тесты»), открытые замеры - в [`../../TODO.md`](../../TODO.md).

| Файл или папка | Что внутри |
|---|---|
| [`SD-speed-test-result.txt`](SD-speed-test-result.txt) | Лог `PURRGO_HW_TEST=SD_SPEED`: чтение microSD при SPI 10 и 20 МГц (до внедрения быстрого обмена `xfer_fast`, см. [`../hardware.md`](../hardware.md)) |
| [`purrgo-stack-ram-monitor.txt`](purrgo-stack-ram-monitor.txt) | Журнал монитора стека и RAM (строки `STKMON`): статическая память, пик стека |
| [`Batt-discharge/`](Batt-discharge/) | Кривые разряда батарей: `LIION.txt`, `ZINK-CARBON.txt` (время и напряжение на элемент) |
| [`Current/`](Current/) | Потребляемый ток, мА (`timestamp;value mA`): `Debug, 10.11.2026` и `Release, 12.11.2026`, по файлу на сценарий (книга, карта, выключение и т. д.) |
| [`tracks/`](tracks/) | Записанные треки: `bicycle.gpx`, `indoor.gpx`, `pedestian.gpx` и архивы `LIION.zip`, `ZINK-CARBON.zip` |
| [`tracks/filter-tuning.md`](tracks/filter-tuning.md) | Подбор фильтра трека по `bicycle/indoor/pedestian.gpx`: что показали данные, варианты и выбранные пороги |

Правило: новый замер - новый файл с датой и сборкой (`PURRGO_HW_TEST`, ключевые `-D`, плата, источник питания) в начале.

## Итоги самотестов E-Ink

Процедура: [`../development.md`](../development.md), раздел «Проверка дисплея на железе».

Результаты на железе: 10 частичных подряд в 1 bit и в 4 gray без остаточного изображения и залипших
строк; в S7/S8 следов нет даже после 40 частичных. Самотест S1-S6 даёт 12 полных обновлений за прогон
(6 сцен × стартовое и эталонное), лишних нет. Длительность: 1-bit partial ~0,57 с, 4 gray partial ~1,24 с,
полное 1-bit ~2,9 с, полное 4 gray FAST ~3,0 с.
