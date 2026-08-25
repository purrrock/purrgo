# TODO: Доработка и оптимизация PurrGO

Roadmap проекта PurrGO: собственный бинарный формат карт, PC-конвертер, map parser/renderer, GNSS-навигация и последующий перенос на энергоэффективный STM32.

## Приоритеты

- **P0** — блокирующие задачи, выполнить в первую очередь.
- **P1** — важные задачи до появления STM32.
- **P2** — функциональное развитие, не блокирует перенос на STM32.
- **P3** — задачи, зависящие от конкретного дисплея/железа.
- **P4** — задачи, требующие фактического STM32 для измерений.
- **P5** — финализация и заморозка формата.

---

# 1. Состояние проекта

## 1.1. Уже реализовано

### Map parser / renderer

- [x] Integer-only чтение координат.
- [x] Integer-only BBox.
- [x] Потоковое чтение line geometry.
- [x] AABB culling.
- [x] Antimeridian handling.
- [x] Feature code → map style.
- [x] Разные стили линий.
- [x] Разные стили polygon fill.
- [x] Clipping renderer.
- [x] Выбор LOD по масштабу:
  - `<= 500 m` → LOD 0.
  - `> 500 m && <= 5 km` → LOD 1.
  - `> 5 km` → LOD 2.
- [x] Обработка нескольких LOD без одновременной загрузки неиспользуемой геометрии.
- [x] Прямой переход к выбранному LOD по offset из PGO global header.

### Map styles

- [x] Road major.
- [x] Road normal.
- [x] Road minor.
- [x] Road unpaved.
- [x] Road path.
- [x] Railway.
- [x] Landuse natural.
- [x] Landuse human.
- [x] Water.
- [x] Таблица feature codes → styles PurrGO.

### Архитектурные ограничения

- [x] Отказ от `float` в основных map coordinate/rendering операциях.
- [x] Потоковое чтение line geometry вместо обязательной загрузки всей линии в RAM.

### Бинарный формат V3

- [x] PurrGO Global Header.
- [x] Зафиксирован размер Global Header — 32 байта.
- [x] Зафиксированы offsets полей Global Header.
- [x] Зафиксированы размеры полей Global Header.
- [x] Зафиксирована структура LOD offsets.
- [x] Python converter генерирует новый Global Header.
- [x] C parser читает новый Global Header.
- [x] Python converter и C parser синхронизированы по Global Header.
- [x] Удалена зависимость от старого `32-byte YZL header`.
- [x] Реализован direct seek к нужному LOD по offset из Global Header.
- [x] Реализована проверка границ выбранного LOD.
- [x] `v3_jump` определён как точный физический размер subtree в байтах.
- [x] C parser проверяет `v3_jump` на выход за границы выбранного LOD.
- [x] C parser распространяет ошибки структурной валидации вверх.
- [x] Добавлены regression tests для direct LOD seek и некорректного `v3_jump`.

### Ещё не реализовано

Все остальные задачи данного документа считаются открытыми, если явно не отмечены `[x]`.

---

# 2. P0 — Собственный бинарный формат PurrGO

Это главный текущий блок работ.

Формат должен быть независим от DT G1 и не должен содержать полей, необходимых только исходному формату.

## 2.1. Global header

- [x] Спроектировать собственный global header PurrGO.
  - [x] Использовать собственную сигнатуру PurrGO — `PGO`.
  - [x] Добавить только необходимые навигатору поля.
  - [x] Добавить размер payload.
  - [x] Добавить offsets LOD.
  - [x] Определить endianess.
  - [x] Определить точный размер каждого поля.
  - [x] Не сохранять DT G1-specific поля без необходимости.
- [x] Зафиксировать окончательную структуру Global Header в спецификации V3.

## 2.2. Спецификация header

- [x] Зафиксировать точный размер header.
- [x] Зафиксировать offsets.
- [x] Зафиксировать размеры всех полей.
- [x] Исключить все скрытые зависимости от старого `32-byte YZL header`.
- [x] Убрать MD5 из нового Global Header.

---

# 3. P0 — Data Node и feature code

## 3.1. Feature code

- [X] Определить окончательный размер feature code.
- [X Проверить, помещается ли полный диапазон PurrGO feature codes в `uint8_t`.
- [X] Зафиксировать допустимый диапазон.
- [X] Зарезервировать диапазон для будущих расширений.
- [X] Зафиксировать `0 = NO_CLASS`.
- [X] Зафиксировать таблицу feature codes:
  - Roads.
  - Railway.
  - Landuse.
  - Water.
  - POI.
  - Reserved.

## 3.2. Data Node

- [X] Определить окончательную структуру Data Node.
- [X] Определить размер каждого поля.
- [x] Синхронизировать структуру C parser и Python converter.
- [X] Зафиксировать offsets всех последующих полей.

---

# 4. P0 — Nav Node / R-Tree / SQT

## 4.1. `v3_jump`

- [x] Проверить текущую семантику `v3_jump` в существующем traversal.
- [x] Определить, действительно ли `+8` является только DT G1 prefetch compensation.
- [x] Убрать зависимость от старой семантики DT G1.
- [x] Определить окончательную семантику:`v3_jump` = точный физический размер пропускаемого subtree.
- [x] Зафиксировать единицы измерения — байты.
- [x] Проверить прямое использование значения через `purrgo_fs_seek()`.
- [x] Синхронно обновить Python converter и C parser.
- [x] Добавить validation для `v3_jump`.

## 4.2. SQT

- [ ] Определить окончательную структуру SQT block.
- [ ] Удалить неиспользуемые поля.
- [ ] Зафиксировать `mode`.
- [ ] Зафиксировать количество root nodes.
- [ ] Зафиксировать структуру нескольких SQT/LOD.
- [x] Зафиксировать способ поиска нужного SQT для LOD через Global Header offsets.

## 4.3. LOD

- [x] LOD 0 используется при масштабе `<= 500 m`.
- [x] LOD 1 используется при `> 500 m && <= 5 km`.
- [x] LOD 2 используется при `> 5 km`.
- [x] Не обрабатывать одновременно несколько LOD.
- [x] Не загружать geometry неиспользуемого LOD.
- [x] Реализован direct seek к выбранному LOD.
- [x] Проверять границы выбранного LOD при traversal.
- [ ] Добавить regression tests для переходов:
  - `500 m`;
  - `500 m + 1`;
  - `5 km`;
  - `5 km + 1`.
- [X] Проверить переключение LOD при увеличении и уменьшении масштаба.

---

# 5. P0 — Python ↔ C synchronization

Любое изменение бинарного формата должно одновременно отражаться в Python writer и C reader.

## Python converter

- [x] Генерировать новый PurrGO global header.
- [x] Убрать все зависимости от старого `32-byte YZL`.
- [x] Генерировать текущую структуру Data Node.
- [x] Генерировать текущую структуру Nav Node.
- [x] Генерировать новый `v3_jump`.
- [x] Корректно рассчитывать LOD offsets.
- [x] Корректно рассчитывать `v1` относительно нового header.
- [x] Генерировать текущую структуру SQT/LOD.

## C parser

- [x] Читать новый PurrGO header.
- [x] Проверять сигнатуру.
- [x] Использовать LOD offsets из Global Header.
- [x] Выполнять direct seek к выбранному LOD.
- [x] Читать текущий Data Node.
- [x] Читать текущий Nav Node.
- [x] Использовать новую семантику `v3_jump`.
- [x] Проверять границы `v3_jump`.

---

# 6. P0 — Validation и regression tests

Формат должен проверяться независимо от STM32.

## 6.1. Эталонные карты

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

## 6.2. Converter validation

- [ ] Проверять целостность `.idx`.
- [ ] Проверять целостность `.mlp`.
- [ ] Проверять все `v1`.
- [x] Проверять `v3_jump`.
- [x] Проверять LOD offsets.
- [ ] Проверять `parts[]`.
- [ ] Проверять `num_points`.
- [ ] Проверять `num_parts`.
- [ ] Проверять допустимость feature codes.
- [ ] Проверять отсутствие выхода offsets за пределы файла.

## 6.3. C parser regression tests

- [x] Проверить соответствие интерпретации Python converter и C parser для нового Global Header.
- [x] Добавить автоматические regression tests для direct LOD seek.
- [ ] Проверять старые тестовые карты после каждого изменения формата.
- [x] Добавить тесты на некорректный Global Header.
- [x] Добавить тесты на повреждённые LOD offsets.
- [x] Добавить тест на `v3_jump`, выходящий за границу выбранного LOD.

---

# 7. P1 — Geometry и ограничение RAM

Цель — сделать формат удобным для потокового чтения на Cortex-M.

## 7.1. Line geometry chunking

- [ ] Реализовать автоматический chunking line geometry в Python converter.
- [ ] Ограничить количество вершин одного chunk.
- [ ] Повторять граничную точку в следующем chunk.
- [ ] Не допускать визуальных разрывов.
- [ ] Сохранять корректную работу `parts[]`.
- [ ] Добавить regression tests для chunking.

## 7.2. Polygon subdivision

- [ ] Реализовать polygon subdivision/clipping на стороне PC.
- [ ] Не разрезать polygon простым делением массива точек.
- [ ] Сохранять геометрическую целостность каждого ring.
- [ ] Сохранять outer rings.
- [ ] Сохранять holes.
- [ ] Проверить polygon clipping около viewport boundaries.

## 7.3. Geometry limits

- [ ] Определить архитектуру `PURRGO_MAP_MAX_POINTS`.
- [ ] Определить архитектуру `PURRGO_MAP_MAX_PARTS`.
- [ ] Сделать значения конфигурационными.
- [ ] Не считать renderer limits ограничениями бинарного формата.
- [ ] **Не фиксировать окончательные числовые значения до появления STM32.**

## 7.4. Polygon buffer

- [ ] Проверить, можно ли после subdivision отказаться от большого статического polygon buffer.
- [ ] Минимизировать временные RAM buffers.

---

# 8. P1 — Map parser / renderer audit

- [x] Integer-only coordinates.
- [x] Integer-only BBox.
- [x] Streaming line geometry.
- [x] AABB culling.
- [x] Antimeridian handling.
- [x] Feature code → style.
- [x] LOD selection.
- [x] Direct LOD seek.
- [x] LOD boundary validation.
- [ ] Полностью проверить отсутствие `float`.
- [ ] Проверить отсутствие `double`.
- [ ] Проверить отсутствие `malloc`.
- [ ] Проверить отсутствие `calloc`.
- [ ] Проверить отсутствие `realloc`.
- [ ] Проверить отсутствие `free`.
- [ ] Проверить stack-heavy локальные buffers.
- [ ] Проверить переполнение integer координат.
- [ ] Проверить geometry за пределами viewport.
- [ ] Проверить antimeridian regression cases.
- [ ] Сократить диагностическое logging для production build.

---


# 10. P1 — Map styles

* [x] Road major.
* [x] Road normal.
* [x] Road minor.
* [x] Road unpaved.
* [x] Road path.
* [x] Railway.
* [x] Landuse natural.
* [x] Landuse human.
* [x] Water.
* [ ] Проверить соответствие всех styles возможностям целевого 2-bit framebuffer.
* [ ] Проверить отсутствие конфликтов между стилями.

---

# 11. P2 — POI

POI не является частью базового map rendering path и не должен блокировать стабилизацию формата.

* [ ] Определить, нужен ли отдельный geometry type.
* [ ] Добавить обработку POI feature codes.
* [ ] Использовать `gfx_circle`.
* [ ] Реализовать `POI_BIG` — диаметр 8 px.
* [ ] Реализовать `POI_SMALL` — диаметр 4 px.
* [ ] Не включать POI в polygon/line rendering path.
* [ ] Реализовать POI icons после определения требований к icon storage.

---

# 12. P2 — GNSS position marker

* [ ] Реализовать маркер позиции пользователя.
* [ ] Использовать равнобедренный треугольник.
* [ ] Острый конец направить по текущему курсу.
* [ ] Вычислять screen coordinates через существующий integer projection.
* [ ] Отрисовывать marker только при попадании GNSS position в отображаемую область.
* [ ] Проверить поведение при отсутствии valid GNSS fix.
* [ ] Проверить поведение при отсутствии valid course.

---

# 13. P2 — Автоматическое ведение карты

* [ ] Реализовать режим auto-follow.
* [ ] Не менять camera при обычном движении внутри центральной области.
* [ ] Начинать сдвиг при приближении пользователя к границе viewport.
* [ ] Реализовать математический hysteresis.
* [ ] Исключить повторные сдвиги при небольших колебаниях координат.
* [ ] Разделить manual pan и auto-follow.
* [ ] Проверить включение/выключение auto-follow после ручного панорамирования.

---

# 14. P2 — Waypoint navigation

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

# 15. P2 — Track logging

## 15.1. RAM buffering

* [ ] Реализовать RAM buffer для track points.
* [ ] Накапливать точки блоками.
* [ ] Согласовать buffer size с размером SD sector.
* [ ] Минимизировать количество операций записи на SD.

## 15.2. Track filtering

* [ ] Реализовать режим Standard:

  * запись при смещении от `5 m`;
  * либо не реже одного раза в `5 min`.
* [ ] Реализовать режим Expedition:

  * запись при смещении от `100 m`;
  * либо не реже одного раза в `15 min`.
* [ ] Протестировать фильтрацию независимо от STM32.

## 15.3. Track rendering

* [ ] Реализовать отображение пройденного пути.
* [ ] Использовать streaming geometry.
* [ ] Не создавать большой полный buffer трека в RAM.

---

# 16. P2 — Planned route

Маршрутизация не входит в PurrGO.

* [ ] Реализовать отображение заранее загруженного/запланированного маршрута.
* [ ] Использовать отдельный style.
* [ ] Не добавлять routing engine.
* [ ] Проверить совместимость с отображением текущего track.

---

# 17. P2 — Text labels

* [ ] Определить набор объектов, для которых нужны labels.
* [ ] Определить font storage.
* [ ] Определить формат label в карте.
* [ ] Реализовать integer-only размещение.
* [ ] Реализовать clipping.
* [ ] Реализовать минимальную систему приоритетов.
* [ ] Не допускать чрезмерного потребления RAM.
* [ ] Не блокировать map parser базового уровня.

---

# 18. P3 — E-Ink abstraction

Эти задачи можно проектировать до появления конкретного дисплея, но окончательные параметры зависят от controller.

## 18.1. Rendering model

* [ ] Реализовать событийно-ориентированный redraw.
* [ ] Не выполнять бессмысленный циклический polling дисплея.
* [ ] Определить dirty-region abstraction.
* [ ] Разделить logical renderer и physical display driver.
* [ ] Поддержать full refresh и partial refresh через abstraction layer.

## 18.2. Ghosting

* [ ] Добавить счётчик partial updates.
* [ ] Добавить механизм запроса full refresh.
* [ ] Определить критерии полной перерисовки после появления конкретного E-Ink controller.
* [ ] Не фиксировать количество partial updates до аппаратного тестирования.

## 18.3. 2-bit palette

* [ ] Проверить mapping PurrGO styles → 4 grayscale levels.
* [ ] Проверить читаемость линий.
* [ ] Проверить читаемость polygon fills.
* [ ] Проверить контраст POI/marker/labels.

---

# 19. P3 — Event-driven display updates

* [X] Перерисовывать карту только при изменении camera/scale.
* [ ] Перерисовывать marker только при изменении позиции/курса.
* [X] Перерисовывать UI только при изменении данных.
* [X] Исключить постоянный framebuffer refresh.
* [ ] Подготовить dirty-region механизм.
* [ ] Проверить возможность partial refresh после выбора конкретного E-Ink controller.

---

# 20. P3 — microSD power architecture

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

# 21. P3 — GNSS power architecture

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

# 22. P4 — STM32 validation

**Не выполнять до появления целевого STM32 hardware.**

## 22.1. RAM

* [ ] Измерить фактический RAM usage map subsystem.
* [ ] Измерить максимальный размер статических buffers.
* [ ] Измерить stack usage `parse_node()`.
* [ ] Измерить stack usage geometry parser.
* [ ] Проверить worst-case geometry.

## 22.2. Performance

* [ ] Измерить SD read throughput.
* [ ] Измерить время чтения geometry.
* [ ] Измерить integer projection performance.
* [ ] Измерить line rendering performance.
* [ ] Измерить polygon rendering performance.
* [ ] Измерить полный frame rendering time.
* [ ] Проверить worst-case frame.

## 22.3. Memory limits

* [ ] После измерений зафиксировать `PURRGO_MAP_MAX_POINTS`.
* [ ] После измерений зафиксировать `PURRGO_MAP_MAX_PARTS`.
* [ ] Зафиксировать окончательные временные buffers.
* [ ] Проверить stack margin.

## 22.4. Floating point

* [ ] Проверить production map path на отсутствие floating-point operations.
* [ ] Проверить map subsystem через compiler/linker diagnostics.
* [ ] Проверить отсутствие software floating-point helper functions в итоговом firmware, если они не нужны другим подсистемам.

---

# 23. P4 — STM32 power management

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

# 24. P4 — Display power measurements

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

# 25. P5 — Финализация формата

Выполняется только после завершения P0/P1 и проверки формата.

* [ ] Зафиксировать версию бинарного формата.
* [x] Зафиксировать текущую спецификацию Global Header V3.
* [ ] Зафиксировать Python converter как reference implementation.
* [ ] Создать окончательный regression dataset.
* [ ] Перегенерировать эталонные карты.
* [ ] Проверить все regression tests.
* [ ] Проверить C reference renderer.
* [ ] Проверить загрузку эталонных карт на STM32.
* [ ] После стабилизации запретить изменения binary structure без увеличения версии формата.

---

# 26. Правила разработки

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

# 27. Текущий ближайший milestone

До появления STM32 выполнить:

1. [x] PurrGO global header.
2. [X] Финальная структура Data Node.
3. [x] Финальная семантика Nav Node traversal / `v3_jump`.
4. [x] Финальная семантика `v3_jump`.
5. [ ] Финальная структура SQT.
6. [X] Финальный feature code.
7. [x] Обновление Python converter.
8. [x] Обновление C parser.
9. [ ] Validation `.idx/.mlp`.
10. [ ] Эталонный regression dataset.
11. [ ] Line chunking.
12. [ ] Polygon subdivision/clipping.
13. [ ] PC reference renderer.
14. [ ] Полный float/malloc audit map subsystem.
15. [ ] Regression tests для LOD.
16. [ ] Подготовка GNSS marker и auto-follow на platform-independent уровне.

**После этого проект должен иметь стабильный и документированный формат PurrGO и полностью тестируемый на PC map pipeline.**

Только после этого имеет смысл переходить к STM32-specific RAM, performance и power optimization.
