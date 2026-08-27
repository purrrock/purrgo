# TODO: Roadmap проекта PurrGO

### Ещё не реализовано

## Эталонные карты

- [ ] Создать минимальный набор `.idx/.mlp`.
- [ ] Одна линия.
- [ ] Несколько `parts`.
- [ ] Большое количество точек.
- [ ] Polygon.
- [ ] Polygon с hole.
- [ ] Несколько polygon.
- [ ] Antimeridian geometry.
- [ ] Empty geometry.
- [ ] Unknown feature code.
- [ ] Несколько LOD.
- [ ] Несколько SQT.
- [ ] Большие `v3_jump`.


## Line geometry chunking на стороне PC

- [ ] Реализовать автоматический chunking line geometry в Python converter.
- [ ] Ограничить количество вершин одного chunk.
- [ ] Повторять граничную точку в следующем chunk.
- [ ] Не допускать визуальных разрывов.
- [ ] Сохранять корректную работу `parts[]`.
- [ ] Добавить regression tests для chunking.

## Polygon subdivision на стороне PC

- [ ] Реализовать polygon subdivision/clipping на стороне PC.
- [ ] Не разрезать polygon простым делением массива точек.
- [ ] Сохранять геометрическую целостность каждого ring.
- [ ] Сохранять outer rings.
- [ ] Сохранять holes.
- [ ] Проверить polygon clipping около viewport boundaries.

##  Geometry limits

- [ ] Определить архитектуру `PURRGO_MAP_MAX_POINTS`.
- [ ] Определить архитектуру `PURRGO_MAP_MAX_PARTS`.
- [ ] Сделать значения конфигурационными.
- [ ] Не считать renderer limits ограничениями бинарного формата.
- [ ] **Не фиксировать окончательные числовые значения до появления STM32.**

## Polygon buffer

- [ ] Проверить, можно ли после subdivision отказаться от большого статического polygon buffer.
- [ ] Минимизировать временные RAM buffers.


Map parser / renderer audit

- [ ] Сократить диагностическое logging для production build.


# POI

Реализовать слой POI.

* [ ] Определить, нужен ли отдельный geometry type.
* [ ] Добавить обработку POI feature codes.
* [ ] Использовать `gfx_circle`.
* [ ] Реализовать `POI_BIG` — диаметр 8 px.
* [ ] Реализовать `POI_SMALL` — диаметр 4 px.
* [ ] Не включать POI в polygon/line rendering path.
* [ ] Реализовать POI icons после определения требований к icon storage.

# Waypoint navigation

Маршрутизация исключена. Поддерживается только навигация по прямой к выбранной точке.

* [ ] Реализовать выбор Waypoint.
* [ ] Отображать прямую от текущей GNSS позиции до Waypoint.
* [ ] Рассчитывать расстояние до Waypoint.
* [ ] Рассчитывать азимут на Waypoint.
* [ ] Выводить distance на data panel.
* [ ] Выводить bearing на data panel.
* [ ] Использовать integer/fixed-point arithmetic.
* [ ] Не использовать floating point.

---

# Track logging

## RAM buffering

* [ ] Реализовать RAM buffer для track points.
* [ ] Накапливать точки блоками.
* [ ] Согласовать buffer size с размером SD sector.
* [ ] Минимизировать количество операций записи на SD.

## Track filtering

* [ ] Реализовать режим Standard:

  * запись при смещении от `5 m`;
  * либо не реже одного раза в `5 min`.
* [ ] Реализовать режим Expedition:

  * запись при смещении от `100 m`;
  * либо не реже одного раза в `15 min`.
* [ ] Протестировать фильтрацию независимо от STM32.

## Track rendering

* [ ] Реализовать отображение пройденного пути.
* [ ] Использовать streaming geometry.
* [ ] Не создавать большой полный buffer трека в RAM.


# Planned route

Маршрутизация не входит в PurrGO.

* [ ] Реализовать отображение заранее загруженного/запланированного маршрута.
* [ ] Использовать отдельный style.
* [ ] Не добавлять routing engine.
* [ ] Проверить совместимость с отображением текущего track.


# Text labels

* [ ] Определить набор объектов, для которых нужны labels.
* [ ] Определить font storage.
* [ ] Определить формат label в карте.
* [ ] Реализовать integer-only размещение.
* [ ] Реализовать clipping.
* [ ] Реализовать минимальную систему приоритетов.
* [ ] Не допускать чрезмерного потребления RAM.
* [ ] Не блокировать map parser базового уровня.

---

# E-Ink abstraction

Эти задачи можно проектировать до появления конкретного дисплея, но окончательные параметры зависят от controller.

## Rendering model

* [ ] Реализовать событийно-ориентированный redraw.
* [ ] Не выполнять бессмысленный циклический polling дисплея.
* [ ] Определить dirty-region abstraction.
* [ ] Разделить logical renderer и physical display driver.
* [ ] Поддержать full refresh и partial refresh через abstraction layer.

## Ghosting

* [ ] Добавить счётчик partial updates.
* [ ] Добавить механизм запроса full refresh.
* [ ] Определить критерии полной перерисовки после появления конкретного E-Ink controller.
* [ ] Не фиксировать количество partial updates до аппаратного тестирования.

## 2-bit palette

* [ ] Проверить mapping PurrGO styles → 4 grayscale levels.
* [ ] Проверить читаемость линий.
* [ ] Проверить читаемость polygon fills.
* [ ] Проверить контраст POI/marker/labels.

# Event-driven display updates

* [ ] Проверить возможность partial refresh после выбора конкретного E-Ink controller.



# microSD power architecture

До STM32 реализовать только platform-independent architecture.

* [ ] Определить API управления питанием SD.
* [ ] Разделить logical storage state и physical power state.
* [ ] Подготовить поддержку load switch.
* [ ] Определить состояния:

  * OFF;
  * POWERING;
  * READY;
  * ACTIVE;
  * SHUTDOWN.

После появления hardware:

* [ ] Подключить реальный load switch.
* [ ] Проверить ток утечки.
* [ ] Измерить время power-up.
* [ ] Измерить стоимость включения/выключения относительно режима постоянного питания.

---

# GNSS power architecture

До STM32:

* [ ] Определить GNSS power state machine.
* [ ] Определить API start/stop/sleep.
* [ ] Определить условия временного отключения GNSS.
* [ ] Разделить GNSS logic и hardware power control.

После STM32:

* [ ] Реализовать реальное управление питанием GNSS.
* [ ] Проверить hardware wake-up.
* [ ] Проверить GNSS startup time.
* [ ] Измерить выигрыш по энергопотреблению.

---

# STM32 validation

**Не выполнять до появления целевого STM32 hardware.**

## RAM

* [ ] Измерить фактический RAM usage map subsystem.
* [ ] Измерить максимальный размер статических buffers.
* [ ] Измерить stack usage `parse_node()`.
* [ ] Измерить stack usage geometry parser.
* [ ] Проверить worst-case geometry.

## Performance

* [ ] Измерить SD read throughput.
* [ ] Измерить время чтения geometry.
* [ ] Измерить integer projection performance.
* [ ] Измерить line rendering performance.
* [ ] Измерить polygon rendering performance.
* [ ] Измерить полный frame rendering time.
* [ ] Проверить worst-case frame.

## Memory limits

* [ ] После измерений зафиксировать `PURRGO_MAP_MAX_POINTS`.
* [ ] После измерений зафиксировать `PURRGO_MAP_MAX_PARTS`.
* [ ] Зафиксировать окончательные временные buffers.
* [ ] Проверить stack margin.

## Floating point

* [ ] Проверить production map path на отсутствие floating-point operations.
* [ ] Проверить map subsystem через compiler/linker diagnostics.
* [ ] Проверить отсутствие software floating-point helper functions в итоговом firmware, если они не нужны другим подсистемам.

---

# STM32 power management

Цель — обеспечить длительную автономную работу.

* [ ] Реализовать Stop mode.
* [ ] Реализовать Standby mode при необходимости.
* [ ] Определить источники wake-up.
* [ ] Настроить GNSS UART DMA.
* [ ] Принимать GNSS поток без постоянного активного polling CPU.
* [ ] Определить условия wake-up по GNSS data.
* [ ] Интегрировать button interrupts.
* [ ] Измерить ток MCU в active state.
* [ ] Измерить ток MCU в Stop.
* [ ] Измерить ток MCU в Standby.
* [ ] Измерить среднее потребление в типичном navigation workload.

---

# Display power measurements

После появления конкретного E-Ink hardware:

* [ ] Измерить ток полного refresh.
* [ ] Измерить ток partial refresh.
* [ ] Измерить время полного refresh.
* [ ] Измерить время partial refresh.
* [ ] Измерить ghosting.
* [ ] Определить оптимальную частоту full refresh.
* [ ] Определить оптимальную стратегию обновления marker.
* [ ] Определить реальную стоимость перерисовки карты.

---

# Финализация формата

Выполняется только после завершения и проверки формата.

* [ ] Создать окончательный regression dataset.
* [ ] Перегенерировать эталонные карты.
* [ ] Проверить все regression tests.
* [ ] Проверить C reference renderer.
* [ ] Проверить загрузку эталонных карт на STM32.
* [ ] После стабилизации запретить изменения binary structure без увеличения версии формата.

---

# Правила разработки

## Binary format

* Не добавлять поля «на будущее», если они не имеют определённого назначения.
* Размеры и offsets всех полей должны быть явно зафиксированы.
* Python converter и C parser должны изменяться синхронно.
* Любое изменение binary format должно сопровождаться regression tests.
* Renderer limits не должны автоматически становиться ограничениями binary format.

## Embedded C

* Не использовать `float` в map/navigation path.
* Предпочитать integer/fixed-point arithmetic.
* Избегать dynamic allocation.
* Не создавать большие локальные buffers на stack.
* Не полагаться на x86-specific integer sizes.
* Проверять переполнение и границы offsets.
* Не оптимизировать RAM «на глаз» — окончательные ограничения должны подтверждаться измерениями на STM32.

## Power

* Не делать непрерывный polling дисплея.
* Не держать SD/GNSS включёнными без необходимости.
* Использовать event-driven processing.
* Буферизовать операции записи на SD.
* Не принимать окончательные power-management решения без измерений реального hardware.

---

# Текущий ближайший milestone

До появления STM32 выполнить:

10. [ ] Эталонный regression dataset.
11. [ ] Line chunking.
12. [ ] Polygon subdivision/clipping.
13. [ ] PC reference renderer.
14. [ ] Полный float/malloc audit map subsystem.
15. [ ] Regression tests для LOD.
16. [X] Подготовка GNSS marker и auto-follow на platform-independent уровне.

**После этого проект должен иметь стабильный и документированный формат PurrGO и полностью тестируемый на PC map pipeline.**

Только после этого имеет смысл переходить к STM32-specific RAM, performance и power optimization.
