# PurrGo Software Architecture

## 1. Purpose

**PurrGo** is an autonomous GNSS navigator and track logger for resource-constrained embedded hardware.

The primary architectural goals are:

- offline navigation;
- low power consumption;
- operation without Internet, cellular connectivity, Wi-Fi or cloud services;
- GNSS positioning and track recording;
- rendering of precompiled vector maps;
- waypoint navigation;
- physical-button control;
- deployment on STM32 with minimal changes to portable application logic;
- deterministic development and testing on a PC.

PurrGo deliberately does **not** implement turn-by-turn route calculation.

The device is a **navigator/logger**, not a routing engine.

---

## 2. Architectural principles

PurrGo is organized around a strict separation between:

1. portable application logic;
2. platform adapters;
3. target-specific application composition;
4. external data and hardware interfaces.

The same portable `src/core/` code is intended to execute both in the PC development environment and on the final STM32 target.

The PC implementation is therefore **not a separate navigation engine**. It is a development and validation environment for the embedded application logic.

The architecture follows these principles:

- hardware-independent core;
- deterministic data processing;
- explicit integer units and scaling;
- minimal dynamic allocation;
- dependency injection for external resources;
- independent update rates for GNSS, navigation, storage and display;
- preprocessed map data rather than on-device GIS processing;
- explicit power-management boundaries;
- hardware-specific code isolated from navigation algorithms.

---

## 3. System architecture

The high-level software architecture is:

```text
+------------------------------------------------------------------+
|                         APPLICATIONS                             |
|                                                                  |
|  apps/pc/                 apps/stm32/           apps/emulator/   |
|  PC tools                 firmware              PC emulator      |
|  experiments               composition           UI/hardware sim  |
+-----------------------------+----------------------+-------------+
                              |
                              v
+------------------------------------------------------------------+
|                       PLATFORM ADAPTERS                          |
|                                                                  |
|  src/platform/pc/       src/platform/stm32/   src/platform/ublox |
|  serial/file I/O        UART/SPI/SD/display   u-blox-specific    |
|  PC filesystem          MCU services          GNSS protocol      |
+-----------------------------+----------------------+-------------+
                              |
                              v
+------------------------------------------------------------------+
|                         PORTABLE CORE                            |
|                                                                  |
|  GNSS | navigation | geo | track | map | graphics | math        |
|                                                                  |
|              Hardware-independent C application logic            |
+------------------------------------------------------------------+
                              |
                              v
+------------------------------------------------------------------+
|                         EXTERNAL DATA                            |
|                                                                  |
| GNSS | map files | tracks | waypoints | configuration | input    |
+------------------------------------------------------------------+
```

The core must not know whether it is running on Windows, an STM32 development board, or the final navigator.

---

## 4. Repository architecture

The repository is organized approximately as follows:

```text
purrgo/
├── apps/
│   ├── pc/                 # PC applications
│   ├── stm32/              # STM32 application layer
│   └── emulator/           # PC hardware/emulator application
│
├── cmake/                  # CMake support files
│
├── docs/
│   └── architecture.md     # This document
│
├── include/
│   └── purrgo/             # Public C interfaces
│
├── src/
│   ├── core/               # Hardware-independent application logic
│   └── platform/
│       ├── pc/              # PC adapters
│       ├── stm32/           # STM32 adapters
│       └── ublox/           # u-blox-specific functionality
│
├── tests/
│   └── core/               # Deterministic core tests
│
├── third_party/             # External dependencies
│
├── tools/                   # Development and preprocessing tools
│
├── CMakeLists.txt
├── HARDWARE.md
└── README.md
```

The architecture intentionally keeps the public interfaces in `include/purrgo/` separate from their implementations in `src/`.

---

# 5. Portable core

## 5.1. Role

`src/core/` contains the algorithms and data structures that should remain independent of the target hardware.

Current and planned responsibilities include:

- GNSS parsing and normalization;
- navigation state;
- geographic calculations;
- coordinate transformations;
- track processing;
- waypoint calculations;
- map parsing and rendering algorithms;
- graphics primitives required by the map renderer;
- mathematical utilities.

The core is the primary unit of deterministic testing.

---

## 5.2. Core restrictions

Code in `src/core/` must:

1. Use standard C.
2. Avoid STM32 HAL headers.
3. Avoid CMSIS dependencies.
4. Avoid Win32 APIs.
5. Avoid GUI frameworks.
6. Avoid operating-system-specific APIs.
7. Avoid direct GPIO access.
8. Avoid direct UART, SPI, DMA or timer access.
9. Avoid direct display-controller access.
10. Avoid direct SD-card or filesystem implementation details.
11. Avoid uncontrolled dynamic allocation.
12. Use fixed-width integer types where the width is part of the interface contract.
13. Make byte order explicit when parsing binary formats.
14. Make physical units and scaling explicit.
15. Keep external resources behind interfaces supplied by platform/application code.
16. Be testable using deterministic PC input.

The core may depend on abstractions representing external resources, but it must not depend on their concrete hardware implementation.

---

# 6. Platform layer

The platform layer adapts hardware or operating-system services to the interfaces expected by the portable core.

Current platform separation is:

```text
src/platform/pc/
src/platform/stm32/
src/platform/ublox/
```

## 6.1. PC platform

`src/platform/pc/` provides desktop implementations used for:

- serial communication;
- file access;
- development utilities;
- GNSS replay;
- PC applications;
- deterministic testing.

The PC platform allows real GNSS receivers and recorded data to be processed before STM32 deployment.

---

## 6.2. STM32 platform

`src/platform/stm32/` is the hardware-facing layer.

It is responsible for services such as:

- UART;
- SPI;
- DMA;
- timers;
- RTC/time sources;
- microSD access;
- display interface;
- physical buttons;
- power-control signals;
- MCU low-power modes.

Board-specific pin mappings and peripheral initialization belong here or in the STM32 application composition layer.

They must not leak into `src/core/`.

---

## 6.3. u-blox platform

`src/platform/ublox/` contains receiver-specific functionality.

PurrGo deliberately separates generic GNSS processing from receiver-specific configuration.

This permits development with older receivers while allowing the final hardware to use a modern u-blox M10-class receiver.

The intended hardware evolution is:

```text
Development:

USB GNSS / mock data
        |
        v
NMEA
        |
        v
minmea / GNSS interface
        |
        v
portable core


Prototype:

GY-NEO6MV2
u-blox NEO-6M
        |
       UART
        |
       NMEA
        |
       minmea
        |
        v
portable core


Release:

u-blox M10-class
        |
   NMEA / UBX
        |
        v
u-blox platform
        |
        v
GNSS interface
        |
        v
portable core
```

Receiver-specific power and configuration operations should remain outside the navigation algorithms.

---

# 7. Application layer

## 7.1. PC applications

`apps/pc/` contains desktop entry points used for development and experimentation.

Current applications include:

- `purrgo_pc`;
- `pc_realtime_logger`.

The PC applications compose the portable core with the PC platform.

A typical real-time GNSS flow is:

```text
GNSS receiver
     |
     v
PC serial port
     |
     v
PC platform adapter
     |
     v
GNSS parser
     |
     v
navigation state
     |
     +----> track processing
     |
     +----> map processing
     |
     +----> UI/output
```

The PC real-time logger is primarily a development and validation tool.

---

## 7.2. STM32 application

`apps/stm32/` is the composition point for the embedded firmware.

It is responsible for connecting:

- STM32 platform services;
- GNSS input;
- navigation state;
- map rendering;
- display;
- storage;
- buttons;
- power-management policy.

The STM32 application should not duplicate navigation algorithms already implemented in `src/core/`.

---

## 7.3. Emulator

`apps/emulator/` provides a PC representation of the target device and its hardware-facing behavior.

The emulator is useful for testing:

- map rendering;
- display geometry;
- input handling;
- navigation state;
- UI behaviour;
- target-specific workflows.

The emulator should consume the same core algorithms as the embedded application whenever practical.

---

# 8. GNSS subsystem

GNSS processing is separated into transport, parsing and navigation layers.

```text
             GNSS receiver
                   |
                   v
        UART / USB / recorded file
                   |
                   v
             transport layer
                   |
                   v
           NMEA / UBX parsing
                   |
                   v
          normalized GNSS data
                   |
                   v
           navigation state
             /           \
            v             v
        track             map
        logging         navigation
```

## 8.1. NMEA

The initial GNSS processing path uses standard NMEA data and the `minmea` library.

NMEA parsing converts receiver sentences into normalized data consumed by the portable application logic.

The core should not depend on a particular serial-port implementation.

---

## 8.2. UBX

The u-blox platform layer may additionally provide UBX-specific configuration and control.

This is important for the final low-power device because the receiver may need to be configured dynamically for:

- update rate;
- enabled satellite constellations;
- power-saving modes;
- receiver state;
- other receiver-specific operating parameters.

These operations are platform functionality rather than navigation algorithms.

---

# 9. Navigation subsystem

The navigation subsystem maintains the normalized state required by the application.

Typical information includes:

- latitude;
- longitude;
- altitude;
- speed;
- course;
- UTC time;
- fix status;
- satellite information.

The GNSS receiver is therefore treated as an input source rather than as the navigation application itself.

The navigation state is consumed by:

```text
GNSS
 |
 +--> track recorder
 |
 +--> waypoint navigation
 |
 +--> map viewport
 |
 +--> position marker
 |
 +--> UI
```

PurrGo provides basic navigation to a waypoint, such as:

- distance;
- bearing/azimuth;
- current position relationship.

It does not calculate turn-by-turn routes.

---

# 10. Geographic calculations

Geographic calculations belong to the portable core.

The implementation should prefer deterministic integer or fixed-point arithmetic where practical.

The project uses explicit scaled coordinate representations rather than relying on implicit floating-point units.

For map data, the current internal coordinate representation is based on:

```text
degrees × 10^7
```

Map source geometry may use a different scale and is converted at the map-format boundary.

Intermediate calculations should use sufficiently wide integer types to prevent overflow.

For example, coordinate differences and projection products should be promoted before subtraction or multiplication.

The objective is:

- deterministic behaviour;
- portability;
- predictable MCU performance;
- reduced dependence on hardware floating-point support.

Floating-point arithmetic is not forbidden where it provides a concrete benefit, but it should not become an unnecessary dependency of the portable core.

---

# 11. Track subsystem

The track subsystem consumes normalized navigation positions and produces recorded track data.

The intended storage format is GPX.

The logical pipeline is:

```text
GNSS fix
   |
   v
navigation state
   |
   v
track filter / sampling
   |
   v
RAM buffer
   |
   v
GPX writer
   |
   v
microSD
```

The storage implementation should buffer track data to reduce unnecessary microSD activity.

This is particularly important for the release device because SD-card writes are relatively expensive in both energy and latency.

---

# 12. Waypoint navigation

Waypoint management is part of the application domain rather than the map renderer.

Waypoints may be stored locally and represented using standard geographic coordinates.

The navigation subsystem can calculate:

```text
current position
       |
       +----> waypoint
                 |
                 +----> distance
                 |
                 +----> bearing
```

Waypoint storage is intended to be local and offline.

---

# 13. Map subsystem

PurrGo uses **precompiled vector maps**.

The embedded device does not perform general-purpose GIS processing or generate maps from OpenStreetMap data.

The intended pipeline is:

```text
OpenStreetMap / source data
          |
          v
    PC preprocessing
          |
          v
  PurrGo binary map format
          |
          v
        microSD
          |
          v
    STM32 map subsystem
          |
          v
       renderer
          |
          v
        display
```

This architecture reduces:

- RAM requirements;
- CPU requirements;
- storage overhead;
- software complexity;
- energy consumption.

---

## 13.1. Map data model

The current map renderer operates on precompiled binary data using separate index/geometry resources.

The current implementation works with:

```text
IDX
 |
 +--> spatial/index information
 |
 +--> SQT / NAV / DATA traversal
 |
 +--> AABB visibility tests
 |
 v
MLP
 |
 +--> geometry
 |
 +--> points
 |
 +--> parts/rings
 |
 v
projection
 |
 v
graphics renderer
```

The map subsystem is therefore divided conceptually into:

1. spatial selection;
2. binary geometry access;
3. geographic clipping/culling;
4. coordinate projection;
5. graphics rendering.

---

## 13.2. Spatial filtering

The map renderer does not blindly render every geometry in the map.

The current processing pipeline uses spatial hierarchy information to locate relevant data:

```text
IDX
 |
 v
SQT
 |
 v
NAV nodes
 |
 v
DATA nodes
 |
 v
AABB intersection
 |
 +---- outside viewport --> discard
 |
 +---- intersects -------> geometry
```

This is essential for running large maps on a microcontroller with limited RAM.

---

## 13.3. Bounding boxes

Map geometry is filtered using axis-aligned bounding boxes.

The implementation also handles longitude ranges crossing the antimeridian.

Conceptually:

```text
normal longitude range:

min_x ---------------- max_x


antimeridian-crossing range:

       max_x       min_x
---------|----------|---------
         <          >
```

Camera intersection and projection therefore cannot assume that longitude always increases monotonically from `min_x` to `max_x`.

---

## 13.4. MLP geometry

The current MLP renderer supports multi-part geometry.

For line layers:

```text
geometry
   |
   +-- part 0 -> polyline
   +-- part 1 -> polyline
   +-- part 2 -> polyline
```

For polygon layers:

```text
geometry
   |
   +-- ring 0
   +-- ring 1
   +-- ring 2
```

Polygon rendering can represent holes using compound polygon rendering and an even-odd fill rule.

The parser validates structural information such as:

- number of parts;
- number of points;
- part start indices;
- geometry bounds.

The current implementation deliberately places a protective limit on the number of points held by one polygon rendering operation.

This is an implementation constraint, not a limitation of the underlying map format.

---

## 13.5. Memory strategy

The map renderer avoids dynamic allocation for temporary polygon geometry.

The current implementation uses a static point buffer.

This has several advantages:

- deterministic memory usage;
- no heap fragmentation;
- predictable behaviour on STM32;
- simpler failure handling;
- easier resource budgeting.

Large map geometries must therefore be processed within explicit implementation limits.

---

# 14. Map projection

The current map renderer converts geographic coordinates into framebuffer coordinates using integer arithmetic.

The basic transformation is:

```text
map coordinate
      |
      v
camera-relative coordinate
      |
      v
scale to viewport
      |
      v
screen coordinate
```

The current internal coordinate representation is:

```text
latitude / longitude = degrees × 10^7
```

MLP geometry coordinates are converted to this representation at the parser boundary.

Intermediate projection calculations use `int64_t`.

This is required because expressions involving coordinate differences and viewport scaling can exceed the range of `int32_t` even when the final screen coordinate fits comfortably into `int16_t`.

The final conversion to graphics coordinates occurs only after the full calculation and range protection.

---

# 15. Graphics architecture

The map subsystem is separated from the physical display controller.

Conceptually:

```text
map renderer
     |
     v
graphics primitives
     |
     v
framebuffer / graphics context
     |
     v
display adapter
     |
     v
physical display
```

The map renderer should not know whether the target display is:

- an emulator window;
- TFT;
- E-Ink;
- another framebuffer-backed display.

This permits the same map algorithms to be tested on a PC and later connected to an STM32 display driver.

---

# 16. Display subsystem

Display updates are deliberately independent from GNSS and navigation update rates.

A typical architecture is:

```text
GNSS                 1 Hz
Track recording      1 Hz
Navigation state     1 Hz
Display              event / variable rate
```

A GNSS update does not automatically imply a complete display redraw.

The display may instead be refreshed when:

- the position changes sufficiently;
- the map viewport changes;
- the user pans the map;
- the zoom level changes;
- the selected map changes;
- the track display changes;
- a waypoint/navigation state changes.

This is particularly important for E-Ink displays.

---

## 16.1. Display geometry

Display geometry is treated as a build-time hardware configuration rather than being scattered through application code.

The target display configuration should define, in one place:

- width;
- height;
- diagonal;
- color depth / bits per pixel;
- orientation.

The diagonal is retained because physical display size can be used to derive pixel density and to scale UI elements such as the position marker appropriately.

The map and graphics layers should consume the resulting viewport dimensions rather than hard-coded display constants.

---

## 16.2. Current display targets

The current hardware profiles are:

```text
Development:
    128 × 296
    E-Ink emulator geometry

Prototype:
    240 × 320
    2.4" ST7789 TFT

Release:
    128 × 296
    2.9" E-Ink
```

The release display is intended to provide a low-static-power reflective interface suitable for long battery operation.

The exact final display remains subject to hardware validation.

---

# 17. Storage architecture

The intended persistent storage is microSD.

The logical storage layout is:

```text
/maps/
    precompiled vector maps

/tracks/
    recorded GPX tracks

/waypoints/
    user waypoints

/config/
    device configuration
```

The application should access storage through an abstraction rather than depending directly on a particular filesystem or SD-card driver.

This permits the same application logic to operate with:

```text
PC filesystem
      |
      v
platform storage adapter


or

microSD
      |
      v
STM32 storage adapter
```

---

# 18. Power-management architecture

Low power consumption is an architectural requirement, not a final optimization.

The release device is intended to operate from a single 18650 Li-ion cell.

Power control should be possible independently for major consumers where supported by the hardware:

```text
                  +--> GNSS power
                  |
MCU power policy -+--> display activity
                  |
                  +--> microSD activity
                  |
                  +--> MCU operating mode
                  |
                  +--> peripheral power
```

Potential MCU states include:

```text
ACTIVE
  |
  v
NAVIGATION
  |
  v
IDLE
  |
  v
LOW POWER / STOP
```

The MCU should spend as much time as practical in low-power states between meaningful events.

Important wake-up sources include:

- GNSS data;
- timers;
- buttons;
- storage events;
- display operations.

---

## 18.1. GNSS power

The GNSS receiver is a separately managed power consumer.

The final u-blox M10-class implementation should support receiver-specific low-power configuration where appropriate.

The application architecture must therefore permit the receiver to be:

- active;
- configured;
- placed into a power-saving state;
- disabled when navigation is not required.

---

## 18.2. Display power

Display activity is independent of navigation-state processing.

For E-Ink, the goal is to avoid unnecessary refreshes because static display content does not require continuous power.

The application should therefore operate on an event-driven display-update model.

---

## 18.3. microSD power

microSD access should be minimized through:

- buffered writes;
- batched operations;
- reduced filesystem activity;
- optional hardware power control on the release platform.

This is particularly important because the SD card can have a substantial active and idle power impact compared with the MCU itself.

---

# 19. Hardware profiles

The software architecture supports three conceptual hardware profiles.

## 19.1. Development

```text
Platform:
    PC

GNSS:
    mock data / USB GNSS

Display:
    emulator

Storage:
    PC filesystem
```

Primary purpose:

- core algorithm development;
- deterministic tests;
- parser development;
- map rendering;
- UI experimentation.

---

## 19.2. Prototype

```text
MCU:
    STM32F446RE
    NUCLEO-F446RE

GNSS:
    GY-NEO6MV2
    u-blox NEO-6M

Display:
    2.4" 240 × 320 ST7789 TFT

Storage:
    microSD

Input:
    physical buttons / development controls
```

The prototype is intended primarily for validating:

- STM32 integration;
- peripheral drivers;
- GNSS communication;
- display rendering;
- map rendering;
- storage;
- application timing.

The NUCLEO board is a development platform and does not define the final PCB architecture.

---

## 19.3. Release

The current release direction is:

```text
MCU:
    STM32U5 class
    currently targeting STM32U585CIU6-class hardware

GNSS:
    modern u-blox M10-class receiver

Display:
    2.9" E-Ink
    128 × 296
    2 bpp / 4 logical grayscale levels

Storage:
    microSD

Input:
    physical buttons

Power:
    1 × 18650 Li-ion
```

The exact MCU, GNSS module, display and power-management components remain subject to hardware validation.

---

# 20. Build architecture

The project uses CMake and C11.

The build system should maintain the same conceptual separation as the source tree:

```text
portable core
     |
     +---- PC platform ----> PC applications
     |
     +---- STM32 platform -> STM32 application
     |
     +---- emulator ------- > PC emulator
```

Platform-specific code should be selected by build configuration rather than by contaminating portable source files with large numbers of target-specific conditionals.

The Windows development environment currently supports:

- CMake;
- MinGW-w64 GCC or MSVC;
- Visual Studio Code;
- C/C++ tooling;
- CMake Tools.

The PC build is the primary environment for fast development and deterministic testing.

---

# 21. Testing architecture

Tests belong primarily under:

```text
tests/core/
```

The preferred testing model is deterministic input/output testing.

For example:

```text
recorded GNSS data
        |
        v
portable parser
        |
        v
navigation state
        |
        v
expected result
```

Map processing can similarly be tested using known binary map fragments and expected rendering behaviour.

The objective is to detect algorithmic errors before introducing STM32-specific variables.

Hardware tests remain necessary for:

- actual GNSS behaviour;
- SPI timing;
- SD-card behaviour;
- display refresh;
- power consumption;
- low-power wake-up;
- DMA;
- physical buttons.

---

# 22. Dependency direction

Dependencies should point toward portable application logic.

Preferred:

```text
apps
  |
  v
platform
  |
  v
core
```

or, for externally supplied resources:

```text
application
    |
    +---- creates adapter
    |
    +---- passes interface
              |
              v
             core
```

Forbidden architecture:

```text
core
 |
 +--> STM32 HAL
 |
 +--> Win32
 |
 +--> display driver
 |
 +--> SD driver
 |
 +--> concrete UART
```

The core must remain reusable.

---

# 23. Resource-management strategy

PurrGo targets resource-constrained microcontrollers.

Memory usage therefore needs to be explicit.

Preferred techniques include:

- caller-owned buffers;
- static buffers where practical;
- bounded arrays;
- fixed-width integer types;
- streaming binary parsing;
- spatial culling before geometry decoding;
- batched storage operations;
- avoidance of unnecessary copies.

The map subsystem is a representative example:

```text
map index
   |
   v
spatial culling
   |
   v
selected geometry only
   |
   v
bounded temporary buffer
   |
   v
projection
   |
   v
graphics
```

This avoids loading an entire map or unnecessary geometry into RAM.

---

# 24. Error handling

External data must be treated as untrusted input.

This includes:

- GNSS data;
- binary map files;
- GPX files;
- waypoint data;
- configuration files;
- filesystem results.

Binary parsers should:

- validate sizes;
- validate offsets;
- validate counts;
- check read results;
- avoid signed/unsigned overflow;
- make endianness explicit;
- reject malformed structures rather than attempting unsafe interpretation.

The map parser in particular must not assume that binary offsets or geometry counts are valid merely because they originated from a generated map.

---

# 25. Data-flow summary

The complete intended runtime architecture is:

```text
                         +----------------+
                         | GNSS receiver  |
                         +-------+--------+
                                 |
                                 v
                         +----------------+
                         | GNSS transport |
                         +-------+--------+
                                 |
                                 v
                         +----------------+
                         | NMEA / UBX     |
                         | parser         |
                         +-------+--------+
                                 |
                                 v
                         +----------------+
                         | Navigation     |
                         | state          |
                         +---+--------+---+
                             |        |
                    +--------+        +--------+
                    v                          v
             +-------------+            +-------------+
             | Track       |            | Waypoint    |
             | subsystem   |            | navigation  |
             +------+------+            +-------------+
                    |
                    v
               GPX / microSD


Position + viewport
        |
        v
+-------------------+
| Map subsystem     |
|                   |
| IDX -> SQT/NAV    |
|      -> DATA      |
|      -> AABB      |
|      -> MLP       |
|      -> projection|
+---------+---------+
          |
          v
+-------------------+
| Graphics renderer |
+---------+---------+
          |
          v
+-------------------+
| Display adapter   |
+---------+---------+
          |
          v
       Display
```

The entire pipeline can be executed with PC adapters during development and with STM32 adapters on the embedded target.

---

# 26. Development strategy

The project follows a staged hardware-independent development model:

```text
Stage 1
PC core development
       |
       v
Stage 2
PC GNSS / map / emulator validation
       |
       v
Stage 3
STM32 prototype integration
       |
       v
Stage 4
power and peripheral validation
       |
       v
Stage 5
release hardware
```

The same core algorithms should survive these stages.

Hardware changes should primarily require new or modified platform adapters and application composition, not rewrites of navigation logic.

---

# 27. Architectural invariants

The following properties are considered architectural invariants of PurrGo:

1. PurrGo is an offline navigator/logger.
2. PurrGo does not perform turn-by-turn route calculation.
3. Navigation algorithms are portable C.
4. `src/core/` is independent of STM32 HAL, CMSIS and Windows APIs.
5. PC applications use the same core algorithms intended for STM32.
6. GNSS transport is separated from GNSS parsing and navigation state.
7. Receiver-specific u-blox functionality remains isolated from generic navigation logic.
8. Maps are precompiled on a PC.
9. The embedded device renders map data rather than acting as a general GIS processor.
10. Map visibility is spatially filtered before geometry rendering.
11. Map geometry is processed using bounded memory.
12. Display updates are independent from GNSS update frequency.
13. Storage access is abstracted from application logic.
14. Low-power operation is considered at architectural boundaries.
15. Physical controls are preferred over a touchscreen for the release device.
16. The final device is designed around local storage and autonomous operation.

---

# 28. Current implementation direction

The architecture is intentionally designed to accommodate the current development state without freezing unfinished hardware decisions.

The immediate software priorities are:

- stabilize the portable core;
- keep geographic arithmetic deterministic and overflow-safe;
- complete GNSS abstraction;
- continue map parser/renderer development;
- centralize display geometry configuration;
- validate map rendering on PC;
- move the same map/navigation code to STM32;
- keep storage and display behind platform interfaces;
- measure actual power consumption on prototype hardware.

The architecture should evolve with implementation, but the dependency direction and hardware-independent core should remain stable.