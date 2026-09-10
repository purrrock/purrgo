# PurrGO keyboard specification

## Hardware

Устройство имеет четыре аппаратные кнопки, расположенные
в ряд под экраном:

KEY1 | KEY2 | KEY3 | KEY4

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