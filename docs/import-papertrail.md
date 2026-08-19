# Import from Papertrail

Документ фиксирует, какие идеи, алгоритмы и архитектурные решения из проекта
[mmolhoek/papertrail](https://github.com/mmolhoek/papertrail) имеет смысл
перенести в PurrGo.

Это **не план портирования Papertrail целиком**. PurrGo — автономный
GNSS navigator/logger на STM32 без маршрутизации, поэтому Raspberry Pi,
TypeScript, web UI, SVG, OSRM и сетевые сервисы Papertrail в PurrGo
переноситься не должны.

Документ основан на текущем состоянии `purrgo` и на исходниках/документации
Papertrail, просмотренных при подготовке этого файла.

---

## 1. Контекст PurrGo

PurrGo уже имеет основу для следующего уровня map/rendering pipeline.

Текущий `include/purrgo/map.h` отделяет:

- filesystem abstraction;
- graphics output;
- geographic bounding box камеры;
- viewport.

Графический интерфейс пока минимален: `purrgo_gfx_t` предоставляет
`draw_line()`. `purrgo_map_render_layer()` получает filesystem, graphics,
camera и viewport. Это уже правильная граница между картой и конкретным
устройством отображения.

Текущий `src/core/map.c` уже реализует существенную часть нижнего уровня:

- чтение Little-Endian integer values;
- преобразование координат;
- проекцию geographic coordinates → screen coordinates;
- 64-битную промежуточную арифметику при масштабировании;
- потоковый разбор geometry;
- чтение частей multipart geometry;
- обход SQT/R-tree структуры;
- spatial filtering по camera bounding box;
- передачу геометрии в `gfx->draw_line()`.

Поэтому импорт из Papertrail должен **расширять существующую архитектуру**, а
не заменять `map.c` другим renderer'ом.

---

# 2. Что именно брать из Papertrail

## Приоритет P0 — обязательно изучить и адаптировать

### 2.1. 1-bit framebuffer

Papertrail использует собственный packed 1-bit bitmap:

```text
8 pixels = 1 byte
bit 7 = leftmost pixel
0xFF = white
0x00 = black
```

Для PurrGo это естественный framebuffer для монохромного e-paper.

Для дисплея 296 × 128:

```text
bytes_per_row = ceil(296 / 8) = 37
framebuffer_size = 37 × 128 = 4736 bytes
```

То есть framebuffer занимает 4736 байт.

Для другого дисплея размер должен вычисляться от его фактического разрешения.

### Что импортировать

Ввести отдельную abstraction:

```c
typedef struct {
    uint16_t width;
    uint16_t height;
    uint16_t stride;
    uint8_t *data;
} purrgo_framebuffer_t;
```

Названия могут быть изменены при реализации, но принцип должен сохраниться:

```text
map / renderer
       ↓
framebuffer
       ↓
display driver
```

### Что не импортировать

Не переносить TypeScript `Bitmap1Bit` буквально.

В STM32 нужен C API без:

- heap allocation на каждый кадр;
- объектов;
- metadata с timestamp;
- runtime type system.

---

## 2.2. Низкоуровневые bitmap primitives

Papertrail имеет отдельный `BitmapUtils` с примитивами:

- `setPixel`;
- `drawLine`;
- `drawCircle`;
- `drawFilledCircle`;
- `drawHorizontalLine`;
- `drawVerticalLine`;
- `fillTriangle`;
- заполнение горизонтального span.

Основные алгоритмы:

- Bresenham для линий;
- Midpoint Circle Algorithm для окружностей;
- scan-line filling для заполненных фигур;
- bit-level operations для 1-bit bitmap.

### Для PurrGo

Создать отдельный graphics/framebuffer слой, например:

```text
include/purrgo/gfx.h
src/core/gfx.c
```

или использовать уже существующую организацию graphics-модулей, если она
есть в текущей ветке.

Минимальный API:

```c
void purrgo_gfx_set_pixel(...);
void purrgo_gfx_draw_line(...);
void purrgo_gfx_draw_circle(...);
void purrgo_gfx_fill_circle(...);
void purrgo_gfx_draw_hline(...);
void purrgo_gfx_draw_vline(...);
void purrgo_gfx_fill_triangle(...);
```

### Важное ограничение

Не следует механически переносить оптимизации JavaScript.

Например:

```text
x >> 3
x & 7
```

действительно естественны и в C, но каждую оптимизацию необходимо оценивать
для конкретного compiler/MCU.

Основная ценность Papertrail здесь — **алгоритмы и структура**, а не
дословный код.

---

# 3. P0 — отделить renderer от e-paper driver

В Papertrail `EPaperService` не является самим bitmap renderer'ом.

Он работает поверх:

```text
EPaperService
    ↓
IEpaperDriver
    ↓
hardware adapter
    ↓
SPI / GPIO
```

В `EPaperService` отдельно присутствуют состояния:

- initialized;
- sleeping;
- busy;
- last update;
- refresh counters.

Есть отдельные операции:

- initialize;
- display bitmap;
- clear;
- full refresh;
- sleep;
- wake;
- wait until ready.

### Что импортировать в PurrGo

Не сам service layer, а принцип разделения:

```text
rendering
    ↓
framebuffer
    ↓
e-paper HAL/driver
    ↓
SPI/GPIO
```

Map renderer не должен напрямую знать:

- SPI;
- BUSY pin;
- RESET;
- DC;
- команды контроллера e-paper.

---

# 4. P0 — event-driven обновление e-paper

Это один из самых важных принципов Papertrail для PurrGo.

Нельзя делать архитектуру:

```text
GNSS update
    ↓
render entire map
    ↓
send framebuffer
    ↓
refresh e-paper
```

на каждом GNSS fix.

Для e-paper обновление должно быть отдельным событием.

Рекомендуемая модель:

```text
GNSS
 │
 ▼
navigation state
 │
 ▼
render decision
 │
 ├── no visual change
 │       └── no display update
 │
 └── visual change
         ↓
      render
         ↓
      framebuffer
         ↓
      e-paper update
```

---

# 5. P0 — отделить GNSS update от display update

Papertrail разделяет получение GPS данных и orchestration rendering pipeline.

Для PurrGo это особенно важно из-за энергопотребления.

GNSS может выдавать position update чаще, чем экран должен физически
обновляться.

Нужно иметь состояние:

```c
typedef struct {
    ...
    bool position_changed;
    bool map_recenter_required;
    bool display_update_required;
} purrgo_render_state_t;
```

Точные поля определить после завершения navigation/application layer.

---

# 6. P0 — политика перемещения карты

Для PurrGo следует использовать не постоянное центрирование карты, а
политику, которую мы уже определили для устройства:

```text
                   экран
        ┌──────────────────────┐
        │                      │
        │    safe zone         │
        │   ┌──────────────┐   │
        │   │              │   │
        │   │      ●       │   │
        │   │   position   │   │
        │   │              │   │
        │   └──────────────┘   │
        │                      │
        └──────────────────────┘
```

Пока marker находится внутри safe zone:

```text
GNSS position changes
        ↓
move marker
```

Когда marker достигает границы safe zone:

```text
GNSS position
        ↓
recenter camera
        ↓
redraw map
```

Это должно быть частью rendering/navigation policy, а не e-paper driver.

---

# 7. P0 — rendering layers

Papertrail явно использует back-to-front layer composition.

Для PurrGo это следует адаптировать к нашему отсутствию routing.

Предлагаемый порядок:

```text
1. background
2. landuse
3. water
4. minor roads
5. major roads
6. map labels
7. recorded track
8. waypoints
9. current position
10. compass
11. scale bar
12. status/UI
```

Если конкретный слой пока не реализован, он просто отсутствует.

### Важное правило

Каждый renderer должен рисовать только свой слой.

Например:

```text
map renderer
    ├── roads
    ├── water
    └── landuse

track renderer
    └── recorded track

navigation renderer
    └── current position / waypoint

ui renderer
    ├── compass
    ├── scale
    └── status
```

Это не означает, что нужно копировать классы Papertrail один-в-один.

В embedded C лучше сделать небольшие функции/модули без service explosion.

---

# 8. P1 — road rendering priority

Papertrail сортирует дороги по визуальному приоритету:

```text
minor
 ↓
residential
 ↓
tertiary
 ↓
secondary
 ↓
primary
 ↓
trunk
 ↓
motorway
```

Причина правильная: важные дороги должны рисоваться поверх второстепенных.

Для PurrGo это особенно полезно при маленьком 1-bit дисплее.

Если наши map records уже содержат road class/type, renderer должен использовать
этот тип как draw priority.

Например:

```text
ROAD_CLASS_PATH       → 1
ROAD_CLASS_RESIDENTIAL → 2
ROAD_CLASS_TERTIARY    → 3
ROAD_CLASS_SECONDARY   → 4
ROAD_CLASS_PRIMARY     → 5
ROAD_CLASS_TRUNK       → 6
```

Точные соответствия должны быть определены по текущему map format PurrGo.
Нельзя вводить значения, которых в формате нет.

---

# 9. P1 — line width как часть rendering style

Papertrail связывает тип дороги с толщиной линии.

Для PurrGo полезно иметь отдельную функцию:

```c
uint8_t purrgo_map_line_width(uint8_t feature_type);
```

Но конкретные толщины нужно подбирать после появления реального framebuffer и
тестовых карт.

Не следует автоматически переносить значения Papertrail вроде 4 px или 6 px:
у Papertrail другой дисплей — 800 × 480.

---

# 10. P1 — projection как самостоятельный слой

Papertrail разделяет:

```text
geographic coordinates
        ↓
ProjectionService
        ↓
pixel coordinates
        ↓
renderer
```

У PurrGo projection уже находится внутри `map.c`.

Это рабочая основа, но в дальнейшем желательно отделить:

```text
map data
    ↓
projection
    ↓
screen coordinates
    ↓
graphics primitives
```

### Целевая архитектура

```text
map.c
  │
  │ geographic geometry
  ▼
map_projection.c
  │
  │ pixel geometry
  ▼
map_renderer.c
  │
  ▼
gfx/framebuffer
```

Разделение следует делать только тогда, когда оно действительно упрощает
код. Не нужно создавать отдельный модуль только ради соответствия
Papertrail.

---

# 11. P1 — clipping

В Papertrail primitives умеют работать с ограниченной областью вывода.

Это особенно заметно в `drawLine(..., maxX)` и area-specific rendering.

Для PurrGo clipping важен по двум причинам:

1. viewport;
2. будущие UI areas/split-screen layouts.

Минимально нужен clipping относительно framebuffer:

```text
0 <= x < width
0 <= y < height
```

Для линий желательно иметь более эффективное clipping, чем проверка каждой
точки, если profiling покажет необходимость.

Не следует преждевременно усложнять алгоритм.

---

# 12. P1 — position marker как отдельный renderer

Papertrail имеет отдельную функцию position marker.

Его схема:

```text
outer circle
+
filled inner circle
```

Для PurrGo это хороший базовый marker.

Рекомендуется:

```c
void purrgo_render_position_marker(
    purrgo_framebuffer_t *fb,
    int16_t x,
    int16_t y
);
```

Размер marker должен зависеть от реального разрешения дисплея.

Не переносить значение `radius = 8` без проверки.

---

# 13. P1 — waypoint marker

Papertrail отдельно рисует waypoint marker двумя окружностями.

Это подходит PurrGo, если waypoint layer будет частью application/map UI.

Рекомендуемый принцип:

```text
map
 +
waypoint
 +
current position
```

Waypoint не должен быть частью низкоуровневого map parser.

---

# 14. P1 — bitmap font

Papertrail использует заранее подготовленные 1-bit glyphs.

Это правильный подход для STM32.

Не нужно:

- SVG text;
- font rasterizer;
- image processing library;
- dynamic font engine.

Нужен компактный bitmap font:

```text
glyph bitmap
     ↓
bitmap framebuffer
```

API может выглядеть так:

```c
void purrgo_gfx_draw_char(...);
void purrgo_gfx_draw_text(...);
```

Размер шрифта должен быть выбран для фактического дисплея.

---

# 15. P1 — UI тоже должен рисоваться в framebuffer

Papertrail использует те же bitmap primitives для:

- compass;
- scale bar;
- divider;
- markers;
- arrows;
- information panel.

Для PurrGo это следует сохранить.

То есть UI не должен обходить framebuffer и напрямую обращаться к e-paper.

Правильно:

```text
UI
 ↓
gfx
 ↓
framebuffer
 ↓
e-paper
```

Неправильно:

```text
UI
 ↓
SPI
 ↓
e-paper
```

---

# 16. P1 — mock/test graphics backend

Papertrail имеет `MockEpaperService` и отдельные тесты graphics utilities.

Для PurrGo это особенно полезно, потому что emulator/host build уже позволяет
тестировать код без STM32.

Целевая схема:

```text
                ┌── STM32 gfx backend
map renderer ───┤
                └── host/test framebuffer
```

Один и тот же renderer должен работать с host framebuffer.

Это позволит проверять:

- projection;
- clipping;
- lines;
- circles;
- map layers;
- marker placement;

без физического e-paper.

---

# 17. P1 — тестировать graphics primitives отдельно

Papertrail имеет отдельные тесты для `BitmapUtils`.

В PurrGo следует создать unit tests для:

```text
set_pixel
draw_line
draw_circle
fill_circle
draw_hline
draw_vline
fill_triangle
```

Минимальные проверки:

### set_pixel

```text
(0,0)
(7,0)
(8,0)
last pixel
out-of-bounds
```

### line

Проверить:

```text
horizontal
vertical
diagonal
reverse diagonal
single pixel
out-of-bounds
```

### circle

Проверить:

```text
radius 0
radius 1
normal radius
partially clipped circle
```

---

# 18. P2 — coordinate pooling / memory reuse

Papertrail использует `CoordinatePool`, чтобы не создавать новые массивы
координат при каждом rendering pass.

В STM32 эта идея ещё важнее.

Но реализация должна быть другой.

Не использовать динамическую allocation/release модель JavaScript.

Вместо этого рассмотреть:

```c
static purrgo_point_t point_buffer[MAX_POINTS];
```

или передавать scratch buffer от вызывающего кода:

```c
purrgo_render_track(
    ...,
    purrgo_point_t *scratch,
    size_t scratch_count
);
```

Какой вариант выбрать — определить по реальным ограничениям RAM и размерам
map/track.

---

# 19. P2 — не создавать временные объекты на каждый feature

Papertrail может позволить себе:

```text
objects
arrays
closures
temporary structures
```

STM32 — нет.

Rendering должен стремиться к:

```text
read feature
    ↓
project
    ↓
draw
    ↓
discard
```

а не:

```text
read all features
    ↓
allocate objects
    ↓
create projected arrays
    ↓
sort
    ↓
render
```

Если sorting действительно понадобится, следует определить bounded storage
и предельное количество элементов.

---

# 20. Что НЕ импортировать

## 20.1. SVG pipeline

Papertrail документирует SVG как часть rendering architecture, но его конечная
цель всё равно 1-bit bitmap.

Для STM32 SVG не нужен.

Не переносить:

```text
SVGService
SVG DOM
SVG parsing
SVG → bitmap conversion
```

Нужен непосредственный bitmap rendering.

---

## 20.2. TypeScript service architecture

Не переносить:

```text
RenderingOrchestrator
GPSCoordinator
TrackDisplayCoordinator
DriveCoordinator
```

как отдельные классы.

В PurrGo это приведёт к лишней архитектурной сложности.

Нужно перенести **ответственность компонентов**, а не их TypeScript
структуру.

---

## 20.3. Routing

Papertrail содержит:

```text
offlineRouting
DriveNavigation
maneuvers
route geometry
turn screens
off-route handling
arrival screen
```

В PurrGo это не переносится.

PurrGo — navigator/logger без routing.

---

## 20.4. Network services

Не переносить:

- Overpass;
- reverse geocoding;
- speed limit services;
- online POI fetching;
- Wi-Fi;
- HTTP APIs.

Карта PurrGo читается из локального map format.

---

## 20.5. Raspberry Pi hardware abstraction

Не переносить Linux GPIO/SPI implementation.

Нужен собственный STM32 HAL/driver.

---

# 21. Предлагаемая архитектура PurrGo после импорта идей

```text
                    APPLICATION
                         │
              ┌──────────┴──────────┐
              │                     │
             GNSS                  logger
              │                     │
              └──────────┬──────────┘
                         │
                  navigation state
                         │
                         ▼
                  render decision
                         │
              ┌──────────┴──────────┐
              │                     │
       camera/recenter          UI state
              │                     │
              └──────────┬──────────┘
                         ▼
                    MAP RENDERER
                         │
             ┌───────────┼───────────┐
             │           │           │
           roads       water       landuse
             │           │           │
             └───────────┼───────────┘
                         │
                       track
                         │
                     position
                         │
                         ▼
                    GFX PRIMITIVES
                         │
                         ▼
                    FRAMEBUFFER
                         │
                         ▼
                  E-PAPER DRIVER
                         │
                         ▼
                    SPI / GPIO
```

Ключевая граница:

```text
map / renderer
       X
       │
       │ НЕ знает
       │
SPI / GPIO / BUSY / RESET
```

---

# 22. Как это соотносится с текущим map.c

Текущий код PurrGo уже делает:

```text
filesystem
   ↓
SQT/R-tree
   ↓
bbox filtering
   ↓
MLP geometry
   ↓
coordinate projection
   ↓
gfx->draw_line()
```

Это не нужно переписывать ради Papertrail.

Следующий этап:

```text
SQT/R-tree
   ↓
geometry
   ↓
projection
   ↓
map feature renderer
   ↓
gfx
   ↓
framebuffer
```

Главное изменение — `gfx` должен перестать быть просто абстракцией
`draw_line()` и стать framebuffer-oriented graphics API.

---

# 23. Рекомендуемая последовательность реализации

## Stage A — framebuffer

Создать:

```text
framebuffer structure
clear
set_pixel
get_pixel (если нужен для tests)
```

Проверить размер и packing.

---

## Stage B — primitives

Реализовать и протестировать:

```text
line
circle
filled circle
horizontal line
vertical line
triangle
```

Основные алгоритмы можно взять по аналогии с Papertrail:

```text
Bresenham
Midpoint Circle
scan-line fill
```

---

## Stage C — text

Добавить:

```text
bitmap font
draw_char
draw_text
```

---

## Stage D — map renderer

Разделить:

```text
map parsing
projection
feature rendering
```

настолько, насколько это оправдано текущим кодом.

---

## Stage E — layers

Ввести фиксированный порядок:

```text
background
landuse
water
roads
track
waypoints
position
UI
```

---

## Stage F — display decision

Добавить:

```text
GNSS update
    ↓
determine whether visual state changed
    ↓
recenter if necessary
    ↓
render
    ↓
display update
```

---

## Stage G — e-paper driver

После завершения framebuffer/rendering API:

```text
framebuffer
    ↓
Waveshare driver
```

Драйвер должен быть отдельным слоем.

---

# 24. Что считать результатом импорта

Импорт идей Papertrail считается завершённым, когда PurrGo имеет:

- [ ] packed 1-bit framebuffer;
- [ ] независимые graphics primitives;
- [ ] bitmap font;
- [ ] renderer → framebuffer separation;
- [ ] framebuffer → e-paper separation;
- [ ] map rendering layers;
- [ ] road rendering priorities;
- [ ] position marker;
- [ ] waypoint marker;
- [ ] clipping;
- [ ] host-testable graphics backend;
- [ ] отдельное решение о необходимости e-paper refresh;
- [ ] camera recenter policy;
- [ ] unit tests для graphics primitives.

---

# 25. Приоритеты

| Возможность | Приоритет | Причина |
|---|---:|---|
| 1-bit framebuffer | P0 | Основа e-paper rendering |
| Graphics primitives | P0 | Основа всей отрисовки |
| Renderer → framebuffer | P0 | Критическое разделение слоёв |
| E-paper driver isolation | P0 | Hardware independence |
| Event-driven refresh | P0 | Энергопотребление и специфика e-paper |
| Camera recenter policy | P0 | Основной UX карты |
| Rendering layers | P0 | Контроль порядка отрисовки |
| Bitmap font | P1 | UI и labels |
| Road priorities | P1 | Читаемость карты |
| Clipping | P1 | Корректный viewport |
| Position marker | P1 | Основной navigation UI |
| Waypoints | P1 | Map/navigation UI |
| Host graphics backend | P1 | Тестируемость |
| Primitive unit tests | P1 | Надёжность |
| Scratch buffer / pooling | P2 | Оптимизация RAM |
| Advanced clipping | P2 | Оптимизация CPU |
| Более сложные UI layers | P2 | После базового renderer |

---

# 26. Ключевой вывод

Papertrail не нужно портировать в PurrGo.

Нужно перенести из него **модель rendering pipeline**:

```text
DATA
 ↓
PROJECTION
 ↓
LAYERED RENDERING
 ↓
1-BIT FRAMEBUFFER
 ↓
E-PAPER DRIVER
```

При этом PurrGo уже находится в середине этого пути:

```text
DATA
 ↓
SQT/R-tree
 ↓
GEOMETRY
 ↓
PROJECTION
 ↓
gfx->draw_line()
```

Следовательно, наиболее правильный следующий шаг — не переписывать
`map.c`, а построить отсутствующий framebuffer/graphics слой и постепенно
расширить `gfx` от единственного `draw_line()` до полноценного 1-bit renderer.

---

## Источники Papertrail

Основные материалы, использованные при подготовке:

- `docs/rendering-pipeline.md`
- `src/services/epaper/EPaperService.ts`
- `src/services/svg/BitmapUtils.ts`
- `src/services/svg/TrackRenderer.ts`
- `src/services/svg/RoadRenderer.ts`
- `src/services/svg/WaterRenderer.ts`
- `src/services/svg/LanduseRenderer.ts`
- тесты `BitmapUtils` и `TrackRenderer`

Papertrail:

https://github.com/mmolhoek/papertrail

PurrGo:

https://github.com/purrrock/purrgo
