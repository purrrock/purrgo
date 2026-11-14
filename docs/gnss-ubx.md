# UBX-расширения GNSS (u-blox M10, SPG 5.10 / 5.30)

Переключатель сборки: `PURRGO_GNSS_UBX` в `include/purrgo/hardware_config.h`
(`-DPURRGO_GNSS_UBX=0` - только NMEA, `1` - с UBX; по умолчанию 1 для приёмника UBX).
Модуль G10A-F30 - u-blox M10 (UBX-M10050-KB, L1). Установленный экземпляр сообщает
`ROM SPG 5.10`, протокол 34.10 (документ в `docs/datasheets/GNSS_modules/U-blox M10`);
более новые прошивки - SPG 5.30, протокол 34.30. Ключи ниже есть в обеих, кроме SEC-SIG.
Документ SPG 7.0x (протокол 52, L1+L5) для этого модуля не подходит.

## Что включено при `PURRGO_GNSS_UBX=1`

| Сообщение | Ключ `CFG-MSGOUT-*_UART1` | Зачем |
|---|---|---|
| UBX-NAV-PVT | `0x20910007` | время, позиция, скорость, курс, hAcc/vAcc/sAcc; главнее RMC/GGA/GSA |
| UBX-NAV-ODO | `0x2091007F` | дистанция трека (нужен `CFG-ODO-USE_ODO`, по умолчанию выключен) |
| UBX-SEC-SIG | `0x20910635` | глушение и спуфинг; только SPG 5.30+, в 5.10 нет (NAK), по умолчанию выключено (`PURRGO_GNSS_SEC_SIG`) |
| UBX-MON-RF | `0x2091035A` | CW-помеха, AGC, антенна |
| UBX-NAV-DOP | `0x20910039` | HDOP/VDOP/PDOP (без NMEA их больше неоткуда взять) |
| UBX-MON-VER | опрос | версия прошивки в лог |

## NMEA на UART приёмника

По умолчанию (`PURRGO_GNSS_UBX_NMEA=0`) NMEA выключен полностью: меньше нагрузка на UART (около
300-500 байт/с) и нет текста между бинарными кадрами. Порядок:

1. Включение: модуль на заводской настройке (NMEA, 38400). `gnss_link` ищет скорость UART **по NMEA**
   (или по кадру UBX, см. ниже), при необходимости меняет скорость модуля.
2. Связь установлена: уходит настройка. Базовый кадр, как и раньше, включает GGA/RMC/GSA (1 Гц) и
   выключает остальное; затем PVT, ODO, MON-RF, NAV-DOP, одометр, модель динамики.
3. Отдельным VALSET `CFG-UART1OUTPROT-NMEA` (`0x10740002`) = 0 - после остальной настройки и до PSM
   (кадры PSM по-прежнему самые последние). Ответ ACK/NAK приходит в UBX и при выключенном NMEA.

Если модуль ответил NAK на этот кадр (`GNSS: config "NMEA off" rejected by module`), остаётся
сокращённый NMEA (GGA/RMC/GSA): он разбирается как запасной путь, как и NMEA до настройки.

Откуда теперь берутся данные: время, позиция, высота, скорость, курс, тип фикса, число спутников,
hAcc/vAcc/sAcc - из UBX-NAV-PVT; HDOP/VDOP/PDOP - из UBX-NAV-DOP; таймаут потери фикса считается по
PVT. Число спутников "в поле зрения" (GSV) в мониторе больше недоступно: показываются спутники в
решении (`sv` из PVT).

Что учитывает отсутствие NMEA:

- **`gnss_link`**: после сброса одного МК (модуль не обесточен) NMEA у модуля уже выключен, поэтому
  модулем считается и корректный кадр UBX (верная контрольная сумма), не только предложение NMEA.
  После выключения питания по EN модуль возвращается к заводской настройке (NMEA, 38400), и всё
  проходит заново (настройка хранится только в слое RAM, `PURRGO_GNSS_UBX_LAYERS`).
- **Монитор** (`gnss_monitor`): связь определяется по кадрам UBX, состояние фикса - по PVT
  (`UBX OK, no fix yet ...`, `FIX after ... s, sats used=..`); сверка с 1PPS идёт по числу PVT с
  `fixOK` (`PVT-OK`) вместо RMC со статусом A.
- **Разбор NMEA** в ядре остаётся: он нужен сборкам `PURRGO_GNSS_UBX=0`, модулю до настройки и запасному
  пути при NAK. В режиме без NMEA через него не проходит ни один байт после настройки.

`-DPURRGO_GNSS_UBX_NMEA=1` оставляет NMEA включённым (GGA/RMC/GSA, 1 Гц) рядом с UBX, как раньше:
для отладки (u-center, внешние программы). Тогда пока приходит PVT, он главнее RMC/GGA/GSA (NMEA даёт
HDOP/VDOP и запасной путь, после `PURRGO_GNSS_PVT_HOLD_EPOCHS` эпох RMC без PVT решением снова владеет NMEA).

Настройка уходит отдельными `UBX-CFG-VALSET` (базовая NMEA, по одному на каждое
сообщение PVT/ODO/MON-RF/NAV-DOP, одометр, модель динамики, выключение NMEA) и опросом MON-VER: модуль применяет
VALSET целиком или отклоняет целиком, поэтому NAK по одному кадру не отменяет остальные.
Ответы ACK/NAK приходят в порядке отправки, поэтому отказ пишется в лог с именем группы:
`GNSS: config "NAV-ODO" rejected by module (NAK)` (имена: `NMEA/rate`, `NAV-PVT`, `NAV-ODO`,
`MON-RF`, `NAV-DOP`, `SEC-SIG`, `ODO enable/profile`, `DYNMODEL`, `NMEA off`). Сопоставление считается от последнего вызова
настройки; поздний ответ на команду смены скорости UART (gnss_link) может сдвинуть счёт на один.

## Диагностика в логе

- `GNSS: u-blox SW ... HW ...`, `FWVER`, `PROTVER` - ответ на опрос MON-VER.
- `GNSS: PVT fixType=.. fixOK=.. sv=.. validTime=.. hAcc=.. mm pDOP=..` - раз в 5 с (уровень INFO):
  видно, есть ли уже время без позиции. Мигание PPS-светодиода не означает 3D-фикс: по умолчанию
  (`CFG-TP-LEN_TP1`=0, `CFG-TP-LEN_LOCK_TP1`=100 мс, `CFG-TP-USE_LOCKED_TP1`=1) импульс выдаётся, как
  только приёмник синхронизировался по времени GNSS, и позиция для этого не нужна.

## Параметры сборки

| Define | По умолчанию | Смысл |
|---|---|---|
| `PURRGO_GNSS_UBX` | 1 (для UBX-приёмника) | 0 - только NMEA |
| `PURRGO_GNSS_UBX_NMEA` | 0 | 0 - NMEA выключен после установления связи; 1 - оставить GGA/RMC/GSA рядом с UBX |
| `PURRGO_GNSS_SEC_SIG` | 0 | 1 - включить UBX-SEC-SIG (нужна прошивка SPG 5.30+) |
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
