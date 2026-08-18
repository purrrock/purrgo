# PurrGo — Hardware Architecture

This document defines the hardware platforms for the PurrGo GNSS navigator/logger.

The project has two hardware targets:

1. **Development / Debug Platform** — selected for maximum convenience during firmware development, testing and debugging.
2. **Release Platform** — selected for minimum power consumption, compact size and long battery life.

The PurrGo device is a GNSS navigator/logger **without route calculation**.

The primary design goal of the release hardware is:

> Garmin-like functionality and usability with very low power consumption and long operation time from a single 18650 Li-ion cell.

---

## 1. Functional Requirements

The final device shall provide:

- GNSS positioning;
- current coordinates;
- speed;
- altitude;
- course over ground;
- UTC time;
- satellite/fix information;
- track recording;
- waypoint management;
- GPX import/export;
- map display;
- map zoom and pan;
- current-position marker;
- track display;
- basic navigation to a waypoint;
- physical user controls;
- operation without an Internet connection;
- operation without a mobile network;
- operation without Wi-Fi or Bluetooth;
- long battery life.

The project does **not** require:

- turn-by-turn route calculation;
- road routing;
- online maps;
- cloud services;
- permanent wireless connectivity.

---

# 2. Development / Debug Hardware

The development hardware is intentionally different from the final hardware.

Its purpose is to make firmware development, debugging and experimentation as easy as possible.

## 2.1 MCU Development Board

### NUCLEO-F446RE

The initial development platform is:

**STMicroelectronics NUCLEO-F446RE**

MCU:

- STM32F446RE
- ARM Cortex-M4F
- up to 180 MHz
- hardware floating-point unit
- integrated ST-LINK debugger/programmer

The NUCLEO-F446RE is a development platform, not the intended final PCB.

Advantages:

- integrated ST-LINK;
- USB connection to the PC;
- easy firmware flashing;
- convenient debugging with STM32CubeIDE;
- large number of GPIO pins;
- hardware UART/SPI/I²C interfaces;
- sufficient performance for GNSS parsing, logging and initial map rendering.

The F446RE is deliberately retained for development even though the release MCU is expected to be more power-efficient.

---

## 2.2 Development GNSS Receiver

### GY-NEO6MV2 / u-blox NEO-6M

The existing **GY-NEO6MV2** module is used for initial development.

Typical interface:

- UART;
- NMEA output;
- configurable GNSS receiver settings;
- external active/passive antenna depending on module variant.

The module is suitable for:

- UART driver development;
- NMEA parser testing;
- position acquisition;
- track logging;
- waypoint testing;
- testing the `minmea` library;
- initial map rendering.

The GY-NEO6MV2 is considered a **development component**, not the preferred final GNSS solution.

The exact electrical characteristics of a particular GY-NEO6MV2 board must be verified against the actual board before connecting it to the final power system. GY-NEO6MV2 boards available on the market are not necessarily electrically identical.

---

## 2.3 GNSS Protocol

The firmware shall initially support NMEA messages through the `minmea` library.

The architecture shall keep the GNSS interface independent from the parser.

Conceptually:

```text
GNSS UART
    │
    ▼
GNSS transport layer
    │
    ▼
NMEA parser / minmea
    │
    ▼
PurrGo navigation data
```

This allows the development receiver and the final u-blox receiver to be changed without rewriting the navigation subsystem.

The release firmware may additionally use u-blox UBX protocol where it provides a significant benefit, especially for:

- receiver configuration;
- power management;
- update rate configuration;
- GNSS constellation configuration;
- receiver status;
- proprietary high-precision data not available through standard NMEA.

---

# 3. Development Display

The first development display may be a small OLED or TFT display.

The development display is not required to have the same technology or resolution as the final display.

Its purpose is to make firmware debugging convenient.

The development UI shall initially expose:

```text
LAT
LON
ALT
SPD
COURSE
SAT
FIX
TIME
```

A simple graphical map/track view shall also be implemented when the display hardware permits it.

---

# 4. Development Storage

A microSD card module shall be used during development.

The microSD card provides:

- track storage;
- GPX files;
- test maps;
- map tiles;
- configuration files;
- test data.

The storage interface should use SPI initially because it is simple to debug.

If required by the final hardware, the release design may use SDMMC instead of SPI.

The firmware shall keep the filesystem/storage layer independent from the physical SD interface.

---

# 5. Development User Interface

The development setup may use:

- push buttons;
- rotary encoder;
- existing TM1638 module where useful;
- USB serial console.

The TM1638 module is useful for low-level experiments because it provides:

- buttons;
- seven-segment displays;
- LEDs;
- a simple serial interface.

It is not intended for the final navigator UI.

---

# 6. Release Hardware Architecture

The final hardware shall be optimized for:

- low average current;
- low quiescent current;
- long battery life;
- outdoor readability;
- simple physical controls;
- autonomous operation;
- compact PCB;
- serviceability.

The target battery is:

**one 18650 Li-ion cell**

Nominal voltage:

```text
3.6–3.7 V
```

Typical target capacity:

```text
3000–3500 mAh
```

The actual battery capacity shall be selected after power measurements.

---

# 7. Release MCU

## 7.1 STM32U5 Family

The preferred release MCU family is:

**STM32U5**

An STM32U575-class device is the current primary candidate.

The final exact package and part number shall be selected after the RAM, Flash, GPIO and peripheral requirements are established.

The reasons for selecting STM32U5 include:

- low-power operating modes;
- substantially better energy efficiency than the development F446 platform;
- sufficient CPU performance for map rendering;
- large RAM/Flash options;
- DMA;
- SPI;
- I²C;
- UART;
- RTC;
- low-power timers;
- flexible power-management capabilities.

The release firmware shall be designed so that most of the application logic is not dependent on STM32F446-specific features.

---

# 8. Release GNSS Receiver

## 8.1 Preferred Architecture

The preferred release GNSS receiver is a modern u-blox M10-family module.

A **u-blox MAX-M10S-class receiver** is the current candidate.

The final GNSS module shall preferably provide:

- multi-GNSS operation;
- GPS;
- Galileo;
- GLONASS and/or BeiDou where supported by the selected device/configuration;
- UART;
- low-power tracking modes;
- configurable update rate;
- external antenna support;
- configurable receiver parameters.

The final GNSS module shall be connected directly to the main PCB.

The development GY-NEO6MV2 module shall not dictate the final PCB design.

---

# 9. GNSS Antenna

The release design shall use a dedicated GNSS antenna positioned as far as practical from:

- switching regulators;
- high-speed digital signals;
- SD card traces;
- display electronics;
- high-current battery paths.

The antenna implementation shall be selected after the enclosure and PCB geometry are defined.

The final antenna may be:

- ceramic patch antenna;
- active GNSS antenna;
- other GNSS antenna suitable for the selected receiver.

The exact antenna model is **not fixed yet**.

Antenna performance and placement are considered part of the RF design and shall be validated on the physical PCB.

---

# 10. Release Display

The release display is intentionally not fixed to a conventional TFT or OLED.

Two technologies are currently preferred:

1. **Memory LCD**
2. **Black/white/red electrophoretic display (E-Ink / e-paper)**

The final choice shall be based on actual measurements and usability tests.

---

# 11. Option A — Memory LCD

Memory LCD is currently the preferred choice if smooth map interaction is required.

Desired characteristics:

- approximately 400×240 or higher;
- monochrome or limited colour;
- sunlight-readable;
- very low static power;
- fast enough update rate for navigation UI;
- SPI or similar serial interface.

Advantages:

- extremely low power;
- no conventional backlight required for reflective operation;
- image remains visible without continuous framebuffer refresh;
- substantially faster refresh than electrophoretic displays;
- well suited to a moving map.

Memory LCD is particularly attractive for a Garmin-like user interface.

---

# 12. Option B — Black/White/Red E-Ink

A second release-display candidate is a small electrophoretic display with:

- black;
- white;
- red.

This is the same general display technology used by many electronic shelf labels.

The desired resolution is approximately:

```text
400×300
```

or higher.

The exact display model is not yet fixed.

## Advantages

- excellent readability in direct sunlight;
- no conventional backlight;
- almost no display power while the image is static;
- excellent suitability for a stationary map;
- black/white/red provides useful cartographic emphasis.

Possible usage:

```text
BLACK  — roads, text, terrain
WHITE  — background
RED    — current track / selected object / warning
```

## Limitations

Electrophoretic displays have significantly slower refresh characteristics than LCDs.

Full-screen refresh may also produce visible flashing.

Therefore the PurrGo UI must not assume that the display can be refreshed like a conventional TFT.

The firmware shall support:

- full refresh;
- partial refresh where supported by the selected panel;
- refresh scheduling;
- reduced refresh frequency;
- static-map operation.

The map shall not necessarily be redrawn at every GNSS update.

---

# 13. Display Strategy

The navigation system shall separate:

```text
GNSS update rate
```

from:

```text
display update rate
```

For example:

```text
GNSS                  1 Hz
Track recording       1 Hz
Navigation calculation 1 Hz
Display                variable
```

The display may be updated:

- immediately after a significant position change;
- periodically;
- after user input;
- after map movement;
- after zoom;
- after entering a new map tile.

This is particularly important for E-Ink.

The firmware shall avoid unnecessary display updates.

---

# 14. Map Storage

The release device shall use removable or replaceable non-volatile storage.

Preferred medium:

**microSD**

The SD card shall contain:

```text
/maps
/tracks
/routes
/waypoints
/config
```

The exact filesystem and file format are to be defined separately.

Maps shall be preprocessed on a PC.

The STM32 shall not perform full OpenStreetMap processing or route calculation.

---

# 15. Map Architecture

The map system shall use pre-generated map data.

Conceptually:

```text
OpenStreetMap / other source
             │
             ▼
       PC preprocessing
             │
             ▼
       PurrGo map data
             │
             ▼
           microSD
             │
             ▼
         STM32U5
             │
             ▼
          Display
```

The release device is therefore a **map renderer**, not a map-generation system.

This significantly reduces:

- CPU requirements;
- RAM requirements;
- storage requirements;
- software complexity;
- energy consumption.

---

# 16. Battery

The primary release battery is:

**1 × 18650 Li-ion**

Target:

```text
3000–3500 mAh
```

The final battery shall be selected based on:

- measured capacity;
- discharge characteristics;
- physical size;
- temperature range;
- protection requirements;
- availability.

The firmware shall monitor battery voltage.

Battery percentage shall not be calculated from voltage alone unless a suitable battery model is implemented.

---

# 17. Power Management

The release device shall use a dedicated low-quiescent-current power-management solution.

The design shall minimize:

- regulator quiescent current;
- LED current;
- leakage through GPIO;
- inactive peripheral current;
- SD card idle current;
- display driver idle current.

The power tree shall be designed around the actual Li-ion discharge curve.

Potential architecture:

```text
             18650
               │
               ▼
       Protection / Power
          management
               │
       ┌───────┴────────┐
       │                │
      3.3 V            other
       │              supplies
       │
 ┌─────┼─────────┬──────────┐
 │     │         │          │
MCU   GNSS      SD       Display
```

The exact regulator topology shall be selected after measuring the current requirements of the final components.

---

# 18. Power Modes

The firmware shall explicitly define power modes.

At minimum:

### Active

All required peripherals are active.

Used for:

- user interaction;
- map movement;
- display updates;
- GNSS processing.

### Navigation

GNSS operates continuously while the MCU performs only required processing.

The display is updated according to the selected display technology.

### Logging

GNSS remains active.

Track data is buffered in RAM and periodically written to storage.

### Idle

The MCU enters a low-power mode while waiting for:

- GNSS UART activity;
- timer;
- button;
- RTC;
- required peripheral event.

### Deep Sleep

Used when the navigator is not actively being used.

The device may:

- disable display activity;
- reduce GNSS activity;
- stop unnecessary peripherals;
- wake on button/RTC/GNSS event.

---

# 19. SD Card Power Control

The release hardware should provide the possibility of switching SD card power.

The purpose is to avoid unnecessary SD-card idle consumption.

Possible architecture:

```text
3.3 V
 │
 ├── MCU
 ├── GNSS
 │
 └── Load switch
       │
       ▼
      SD
```

The SD card shall be powered only when required where practical.

The firmware shall also minimize filesystem activity by buffering track records in RAM.

---

# 20. User Controls

The release device shall use physical controls rather than a touchscreen.

Target:

- 5–8 buttons;
- optional rotary encoder or directional control;
- dedicated power/wake button.

A possible layout:

```text
       UP
       ▲
 LEFT ◄ ● ► RIGHT
       ▼
      DOWN

   MENU     BACK
```

The final mechanical arrangement is not yet fixed.

Physical controls are preferred because they:

- consume essentially no power when idle;
- work with gloves;
- work in rain;
- work without looking at the controls;
- fit the Garmin-like design goal.

---

# 21. USB

USB shall be provided primarily for:

- firmware update;
- debugging during development;
- GPX transfer;
- map transfer;
- configuration.

The release device does not require USB to remain active during normal navigation.

USB power consumption shall therefore be excluded from the normal operating power budget.

---

# 22. Debug Interfaces

The release PCB should expose test/debug connections for:

- SWD;
- UART;
- power measurement;
- GNSS UART;
- SPI;
- I²C where used.

At minimum, SWD shall remain accessible on engineering samples.

A convenient test connector or test pads should be provided.

---

# 23. Development vs Release Hardware

| Component | Development | Release |
|---|---|---|
| MCU | STM32F446RE / NUCLEO-F446RE | STM32U5 family |
| Debugger | Integrated ST-LINK | External SWD during development |
| GNSS | GY-NEO6MV2 | Modern u-blox M10-class |
| Display | OLED/TFT | Memory LCD or B/W/R E-Ink |
| Storage | microSD | microSD |
| Controls | Buttons / TM1638 | Physical buttons |
| Battery | USB / bench supply | 1 × 18650 |
| Power | NUCLEO power system | Low-Iq power architecture |
| USB | Always available | Service/data interface |
| PCB | Development boards/modules | Custom PCB |

---

# 24. Initial Development Hardware

The minimum hardware required to start the project is:

- NUCLEO-F446RE;
- GY-NEO6MV2 GNSS module;
- GNSS antenna;
- USB cable;
- small display;
- microSD module;
- microSD card;
- breadboard and jumper wires;
- push buttons.

The existing TM1638 module may be used for additional low-level UI experiments.

---

# 25. Release Target

The release hardware shall target:

### Primary goal

**24 hours of continuous navigation from one 18650 battery.**

### Design goal

**36+ hours under typical navigation conditions.**

### Low-power goal

**48 hours or more in an optimized logging/navigation mode**, subject to actual measurements.

These are engineering targets, not guaranteed specifications.

Actual battery life shall be determined from measurements of the completed hardware.

---

# 26. Energy Budget

The project shall use measured power consumption rather than estimates wherever possible.

Power measurements shall be made separately for:

1. MCU;
2. GNSS receiver;
3. display;
4. SD card;
5. power converter;
6. buttons/LEDs;
7. complete device.

The final battery-life calculation shall use:

```text
Operating time ≈ usable battery energy / average system power
```

The usable battery energy shall account for:

- regulator efficiency;
- battery discharge curve;
- minimum operating voltage;
- temperature;
- battery ageing.

---

# 27. Hardware Design Principles

The following principles are mandatory for the release design:

1. **No unnecessary peripherals.**
2. **No Wi-Fi unless a future requirement explicitly justifies it.**
3. **No Bluetooth unless a future requirement explicitly justifies it.**
4. **No cellular modem.**
5. **No route-calculation hardware/software requirement.**
6. **GNSS and display shall be independently power-manageable where practical.**
7. **SD card activity shall be minimized.**
8. **The display shall not be refreshed unnecessarily.**
9. **The MCU shall spend as much time as practical in low-power modes.**
10. **All important power-consumption figures shall eventually be measured on real hardware.**

---

# 28. Current Hardware Decision

The current project direction is:

```text
DEVELOPMENT

NUCLEO-F446RE
      │
      ├── GY-NEO6MV2
      ├── OLED/TFT
      ├── microSD
      └── buttons/TM1638


RELEASE

       18650
          │
          ▼
    Low-Iq power system
          │
          ▼
      STM32U5
       │  │  │
       │  │  └──── Physical buttons
       │  │
       │  └─────── microSD
       │
       ├────────── Memory LCD
       │             OR
       │        B/W/R E-Ink
       │
       └────────── u-blox M10
                      │
                   GNSS antenna
```

The final display technology remains an engineering decision between:

- **Memory LCD** — preferred for fast map interaction;
- **black/white/red E-Ink** — preferred for maximum sunlight readability and potentially lower average display power.

The final choice shall be made after testing actual display modules with the PurrGo map renderer.

---

# 29. Hardware Roadmap

### Prototype 1

```text
STM32F446RE
+
GY-NEO6MV2
+
simple display
```

Goal:

- UART;
- GNSS;
- NMEA/minmea;
- position;
- speed;
- altitude;
- time.

### Prototype 2

Add:

```text
microSD
+
track logging
+
GPX
```

### Prototype 3

Add:

```text
map renderer
+
waypoints
+
physical controls
```

### Prototype 4

Evaluate:

```text
Memory LCD
vs.
B/W/R E-Ink
```

with actual power measurements.

### Prototype 5

Move firmware to:

```text
STM32U5
+
modern u-blox GNSS
```

and measure the complete power budget.

### Release PCB

Integrate:

- STM32U5;
- GNSS;
- antenna;
- display;
- SD;
- buttons;
- battery;
- power management;
- SWD;
- USB;
- test points.

The release PCB shall be designed only after the major components have been experimentally validated.