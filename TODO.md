# PurrGO — TODO

Текущий статус проекта: **PC-версия выходит на финальную стадию. Следующий основной этап — перенос на STM32U585CIU6.**

---

# 1. Завершение PC-версии

# 2. Навигация по Waypoint

Маршрутизация в PurrGO не реализуется.

- [ ] Реализовать выбор Waypoint.
- [ ] Отображать направление на Waypoint.
- [ ] Рассчитывать расстояние до Waypoint.
- [ ] Рассчитывать bearing/азимут на Waypoint.
- [ ] Выводить distance на data panel.
- [ ] Выводить bearing на data panel.
- [ ] Использовать integer/fixed-point arithmetic.
- [ ] Не использовать floating point.

---

# 4. Planned route

Расчёт маршрута не входит в PurrGO.

- [ ] Реализовать отображение заранее подготовленного маршрута.
- [ ] Использовать отдельный style.
- [ ] Проверить совместимость с отображением текущего track.

---

# 5. Release display

Release-дисплей выбран:

**Waveshare 2.7inch e-Paper HAT, 176 × 264, 4 gray levels.**

- [ ] Завершить STM32 display driver.
- [ ] Реализовать abstraction между renderer и physical display.
- [ ] Реализовать full refresh.
- [ ] Реализовать partial refresh.
- [ ] Реализовать dirty-region updates.
- [ ] Определить стратегию обновления GNSS marker.
- [ ] Измерить ghosting.
- [ ] Определить необходимость периодического full refresh.
- [ ] Проверить читаемость карты, POI и labels на 4 уровнях серого.

---

# 6. microSD

- [ ] Выбрать low-voltage microSD-модуль для Release.
- [ ] Реализовать STM32 SPI driver.
- [ ] Реализовать файловую систему.
- [ ] Реализовать структуру `/PURRGO/`.
- [ ] Реализовать управление питанием SD.
- [ ] Проверить power-up/power-down.
- [ ] Измерить ток потребления.
- [ ] Проверить надёжность записи track при выключении питания.

---

# 7. GNSS

## STM32

- [ ] Перенести GNSS transport на STM32.
- [ ] Реализовать UART + DMA.
- [ ] Интегрировать GNSS parser.
- [ ] Интегрировать G10A F30.
- [ ] Проверить 1PPS.
- [ ] Реализовать управление питанием GNSS.
- [ ] Измерить startup time.
- [ ] Измерить потребление.

---

# 8. STM32 migration

Целевой MCU Release:

**STM32U585CIU6**

Для разработки используются:

- NUCLEO-F446RE;
- STM32F411CEU6.

Используется та плата, которая доступна первой.

## Перенос core

- [ ] Собрать portable core без PC-specific dependencies.
- [ ] Проверить map parser на STM32.
- [ ] Проверить map renderer на STM32.
- [ ] Проверить GNSS processing.
- [ ] Проверить track processing.
- [ ] Проверить Waypoint navigation.

## RAM

- [ ] Измерить фактическое RAM usage.
- [ ] Определить максимальные buffers.
- [ ] Измерить stack usage `parse_node()`.
- [ ] Измерить stack usage geometry parser.
- [ ] Проверить worst-case geometry.
- [ ] Проверить stack margin.

## Performance

- [ ] Измерить SD read throughput.
- [ ] Измерить время чтения geometry.
- [ ] Измерить integer projection performance.
- [ ] Измерить line rendering performance.
- [ ] Измерить polygon rendering performance.
- [ ] Измерить полный frame rendering time.
- [ ] Проверить worst-case frame.

## Floating point

- [ ] Проверить production map/navigation path на отсутствие floating-point operations.
- [ ] Проверить итоговый firmware на наличие software floating-point helpers.

---

# 9. Power management

Цель автономной работы:

| Режим | Цель |
|---|---:|
| Непрерывная навигация | ≥ 24 ч |
| Типичный режим | ≥ 36 ч |
| Экспедиционный режим | ≥ 48 ч |

- [ ] Реализовать Stop mode.
- [ ] Реализовать Standby mode при необходимости.
- [ ] Определить источники wake-up.
- [ ] Интегрировать button interrupts.
- [ ] Определить GNSS power states.
- [ ] Определить SD power states.
- [ ] Определить display power states.
- [ ] Измерить ток MCU active.
- [ ] Измерить ток MCU Stop.
- [ ] Измерить ток MCU Standby.
- [ ] Измерить потребление GNSS.
- [ ] Измерить потребление SD.
- [ ] Измерить потребление дисплея.
- [ ] Измерить среднее потребление PurrGO в типичном navigation workload.

---

# 10. User interface

- [ ] Определить окончательное количество и расположение кнопок.
- [ ] Проверить, достаточно ли четырёх встроенных кнопок Waveshare для основного интерфейса.
- [ ] Реализовать основные экраны PurrGO.
- [ ] Реализовать переключение режимов.
- [ ] Реализовать data panel.
- [ ] Реализовать отображение состояния GNSS.
- [ ] Реализовать отображение состояния батареи.

---

# 11. Встроенный TXT reader

Дополнительная функция после основной навигационной части:

- [ ] Создать отдельный Reader mode.
- [ ] Реализовать чтение TXT.
- [ ] Реализовать разбиение текста на страницы.
- [ ] Использовать существующий PurrGO raster font.
- [ ] Реализовать навигацию по страницам кнопками.

---

# 12. Release validation

Перед Release:

- [ ] Полностью проверить карты на STM32.
- [ ] Проверить GNSS на реальном G10A F30.
- [ ] Проверить запись и чтение microSD.
- [ ] Проверить track logging.
- [ ] Проверить Waypoint navigation.
- [ ] Проверить display refresh.
- [ ] Проверить ghosting.
- [ ] Проверить кнопки.
- [ ] Проверить cold start.
- [ ] Проверить восстановление после выключения питания.
- [ ] Измерить фактическое время автономной работы.
- [ ] Выполнить длительный тест непрерывной навигации.
- [ ] Выполнить длительный тест записи track.
- [ ] Выполнить тест заполненной microSD.
- [ ] Проверить поведение при повреждённых/неполных файлах карт.

---

# 13. После стабилизации Release

- [ ] Запретить изменения Map Format V3 без увеличения версии формата.
- [ ] Зафиксировать Release hardware configuration.
- [ ] Зафиксировать pinout.
- [ ] Зафиксировать power architecture.
- [ ] Обновить документацию после финальных аппаратных измерений.
- [ ] Подготовить Release build.