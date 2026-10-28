# PurrGO — TODO

- [ ] Закрыть оставшиеся пункты `TODO_refact.md` (разделы 1-4).

# 2. Навигация по Waypoint

Маршрутизация в PurrGO не реализуется.

- [ ] Реализовать выбор Waypoint.
- [ ] Отображать направление на Waypoint.
- [ ] Рассчитывать расстояние до Waypoint.
- [ ] Рассчитывать bearing/азимут на Waypoint.
- [ ] Выводить distance на data panel.
- [ ] Выводить bearing на data panel.

---

# 3. Planned route

Расчёт маршрута не входит в PurrGO.

- [ ] Реализовать отображение заранее подготовленного маршрута.
- [ ] Использовать отдельный style.
- [ ] Проверить совместимость с отображением текущего track.

---

# 4. Release display

Release-дисплей выбран:

**Waveshare 2.7inch e-Paper HAT, 176 × 264, 4 gray levels.**
- [X] Реализовать использование встроеных в дисплей кнопок вместо подключенных временных.
- [ ] Завершить STM32 display driver.
- [ ] Реализовать abstraction между renderer и physical display.
- [ ] Реализовать full refresh.
- [ ] Реализовать partial refresh.
- [ ] Реализовать dirty-region updates.
- [ ] Определить стратегию обновления GNSS marker.
- [ ] Измерить ghosting.
- [ ] Определить необходимость периодического full refresh.
- [ ] Проверить читаемость карты, POI и labels на 4 уровнях серого и на 1-bit режиме.

---

# 5. microSD

- [X] Выбрать low-voltage microSD-модуль для Release.
- [ ] Реализовать управление питанием SD.
- [ ] Проверить power-up/power-down.
- [ ] Измерить ток потребления.

---

# 6. GNSS

## STM32

- [X] Перенести GNSS transport на STM32.
- [ ] переносить GNSS с USART1 на LPUART1.
- [X] Интегрировать GNSS parser.
- [ ] Интегрировать G10A F30.
- [ ] Настроить G10A F30 на минимально необходимый набор предложений и выдачу не чаще одного раза в секунду.
- [ ] Проверить 1PPS.
- [ ] Реализовать управление питанием GNSS.
- [ ] Измерить startup time.
- [ ] Измерить потребление.

---

# 7. STM32 migration

Целевой MCU:

**STM32U585CIU6**

Поддержка STM32F411CEU6 удалена (код сохранён в архивном релизе на GitHub).

## Перенос core

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

# 8. Power management

Цель автономной работы:

| Режим | Цель |
|---|---:|
| Непрерывная навигация | ≥ 24 ч |
| Типичный режим | ≥ 36 ч |
| Экспедиционный режим | ≥ 48 ч |

- [ ] Реализовать мониторинг состояния электропитания и выбор источника Щелочные/NiMH/Литиевые AA / 5V повербанк.
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

# 9. User interface

- [ ] Реализовать отображение состояния батареи.

---

# 10. Встроенный TXT reader

Дополнительная функция после основной навигационной части:

- [ ] Создать отдельный Reader mode.
- [ ] Реализовать чтение TXT.
- [ ] Реализовать разбиение текста на страницы.
- [ ] Использовать существующий PurrGO raster font.
- [ ] Реализовать навигацию по страницам кнопками.

---

# 11. Release validation

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

# 12. После стабилизации Release

- [ ] Запретить изменения Map Format V3 без увеличения версии формата.
- [ ] Зафиксировать Release hardware configuration.
- [ ] Зафиксировать pinout.
- [ ] Зафиксировать power architecture.
- [ ] Обновить документацию после финальных аппаратных измерений.
- [ ] Подготовить Release build.