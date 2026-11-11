# UBX-расширения GNSS (u-blox M10, SPG 5.30)

Переключатель сборки: `PURRGO_GNSS_UBX` в `include/purrgo/hardware_config.h`
(`-DPURRGO_GNSS_UBX=0` - только NMEA, `1` - с UBX; по умолчанию 1 для приёмника UBX).
Модуль G10A-F30 - u-blox M10 (UBX-M10050-KB, L1), документ: SPG 5.30, протокол 34.30.
Документ SPG 7.0x (протокол 52, L1+L5) для этого модуля не подходит.

## Что включено при `PURRGO_GNSS_UBX=1`

| Сообщение | Ключ `CFG-MSGOUT-*_UART1` | Зачем |
|---|---|---|
| UBX-NAV-PVT | `0x20910007` | время, позиция, скорость, курс, hAcc/vAcc/sAcc; главнее RMC/GGA/GSA |
| UBX-NAV-ODO | `0x2091007F` | дистанция трека (нужен `CFG-ODO-USE_ODO`, по умолчанию выключен) |
| UBX-SEC-SIG | `0x20910635` | глушение и спуфинг |
| UBX-MON-RF | `0x2091035A` | CW-помеха, AGC, антенна |
| UBX-MON-VER | опрос | версия прошивки в лог |

NMEA GGA/RMC/GSA остаются: HDOP/VDOP есть только в них, и они резерв, если PVT пропал
(после `PURRGO_GNSS_PVT_HOLD_EPOCHS` эпох RMC без PVT решением снова владеет NMEA).

Настройка уходит четырьмя отдельными `UBX-CFG-VALSET` (базовая NMEA, сообщения,
одометр, модель динамики) и опросом MON-VER: отказ модуля (NAK) по одному кадру не отменяет
остальные. NAK пишется в лог (`GNSS: module rejected UBX class .. id ..`).

## Параметры сборки

| Define | По умолчанию | Смысл |
|---|---|---|
| `PURRGO_GNSS_UBX` | 1 (для UBX-приёмника) | 0 - только NMEA |
| `PURRGO_GNSS_DYNMODEL` | `PURRGO_GNSS_DYN_BICYCLE` (= Portable, 0) | `CFG-NAVSPG-DYNMODEL`; значения `PURRGO_GNSS_DYN_*` |
| `PURRGO_GNSS_ODO_PROFILE` | `PURRGO_GNSS_ODO_CYCL` (1) | `CFG-ODO-PROFILE` |
| `PURRGO_GNSS_TRACK_MAX_HACC_M` | 20 | точка с hAcc выше в трек не пишется |

Велосипедной модели динамики у приёмника нет. В даташите G10A-F30 перечислены Portable,
Stationary, Pedestrian, Automotive, Sea, Airborne 1g/2g/4g, Wrist; значение `BIKE` (10) из
SPG 5.30 - мотоцикл и "не во всех продуктах". Поэтому "велосипед" = Portable; Pedestrian
(`-DPURRGO_GNSS_DYNMODEL=PURRGO_GNSS_DYN_PEDESTRIAN`) сильнее сглаживает, но ограничен 30 м/с.

## Данные и обновление

Модуль выдаёт всё раз в `PURRGO_GNSS_UPDATE_PERIOD_MS` (1000 мс). Ядро берёт решение и снимок
путевого компьютера (`purrgo_trip_computer_update`) раз в секунду; экран Trip computer
перерисовывается, только когда изменилось показанное (километры с шагом 10 м, hAcc с шагом 0,1 м,
CW/AGC с шагом 5 %, состояния).

Экран Trip computer (строки ниже блока SUN):

- `TRP:` дистанция с начала записи трека (одометр модуля; при старте нового файла трека
  отправляется `UBX-NAV-RESETODO`; если отправить не удалось, отсчёт идёт от текущего значения);
- `ACC:` `H3.5 m V5.2 m` - hAcc / vAcc;
- `SIG:` `JAM OK SPF OK` - SEC-SIG (`WARN!`; для спуфинга `IND!` - признаки, `AFF!!` - подтверждён);
  `--` - детектор не сообщил состояние;
- `RF:` `CW10 AGC35 A:OK` - CW-помеха и AGC в процентах, антенна (`SHORT!` замыкание, `OPEN!` обрыв).

`---` - данных нет (сообщение не приходило дольше 3,5 с, нет фикса или сборка без UBX).

## GPX

Заголовок содержит `xmlns="http://www.topografix.com/GPX/1/1"` и пространство имён
`gpxtpx` = Garmin TrackPointExtension **v2** (в v1 нет скорости и курса):

```xml
<extensions>
  <gpxtpx:TrackPointExtension>
    <gpxtpx:speed>2.49</gpxtpx:speed>    <!-- м/с -->
    <gpxtpx:course>45.00</gpxtpx:course> <!-- градусы, только при надёжном курсе -->
  </gpxtpx:TrackPointExtension>
  <pg:hacc>3.52</pg:hacc>                <!-- м, только с PVT -->
  <pg:bat>100</pg:bat><pg:vbat>4793</pg:vbat>
</extensions>
```

`pg:spd` (сотые доли узла) заменён на `gpxtpx:speed` (м/с); `tools/track-analyze/analyze_diag.py`
читает оба варианта.

## Курс из PVT

`headMot` при стоянке случаен, поэтому курс считается надёжным только при скорости >= 0,5 м/с
и `headAcc` <= 45 градусов.
