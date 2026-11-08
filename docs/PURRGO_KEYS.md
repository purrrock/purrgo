# PurrGO keyboard specification

## Hardware

Устройство имеет четыре аппаратные кнопки, расположенные
в ряд под экраном:

KEY1 | KEY2 | KEY3 | KEY4

На плате STM32U585 это кнопки E-Ink HAT K1…K4 на выводах
PA0, PA1, PC14, PC15 соответственно (см. `PurrGO_STM32U585_PINOUT.md`).

Функции кнопок зависят от текущего режима работы.

В нижней строке экрана отображаются подсказки
по действиям кнопок.

## Button events

Поддерживаются события:

- SHORT_PRESS
- LONG_PRESS

DOUBLE_PRESS пока не поддерживается.

## Map screen

| Event        | KEY1     | KEY2      | KEY3   | KEY4                       |
|--------------|----------|-----------|--------|----------------------------|
| SHORT_PRESS  | PAN_LEFT | PAN_RIGHT | PAN_UP | PAN_DOWN                   |
| LONG_PRESS   | ZOOM_OUT | ZOOM_IN   | CENTER | NEXT_SCREEN → TRIP_COMPUTER|

## Trip computer

| Event        | KEY1 | KEY2 | KEY3 | KEY4                          |
|--------------|------|------|------|-------------------------------|
| SHORT_PRESS  | NONE | NONE | NONE | NEXT_SCREEN → CONFIG_MENU     |
| LONG_PRESS   | NONE | NONE | NONE | NEXT_SCREEN → CONFIG_MENU     |

## Menu screens

| Event        | KEY1 | KEY2 | KEY3   | KEY4 |
|--------------|------|------|--------|------|
| SHORT_PRESS  | UP   | DOWN | SELECT | BACK |

## On-screen keyboard (En/Ru)

Экранная клавиатура (`include/purrgo/keyboard.h`, экран - `src/core/ui/ui_keyboard.c`)
предназначена для ввода строки поиска объектов на карте и текста в книгах.
Раскладки EN, RU и SYM (цифры и знаки); Shift: выключен → один раз → Caps.
Текст набирается в кодировке PurrGO-256 (как названия на карте и текст книг).

| Event        | KEY1                | KEY2          | KEY3                      | KEY4              |
|--------------|---------------------|---------------|---------------------------|-------------------|
| SHORT_PRESS  | курсор влево        | курсор вправо | нажать клавишу под курсором | курсор вниз     |
| LONG_PRESS   | стереть символ      | курсор вверх  | готово (ввод принят)      | отмена            |

Курсор по строкам и столбцам замыкается. На экране подсказки кнопок в двух нижних строках.
Клавиатура не подключена к `app_fsm`: в состояние приложения её ещё не включали.
