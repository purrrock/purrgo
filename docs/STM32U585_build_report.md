# STM32U585: результаты сборки

Данные сборки `arm-none-eabi-gcc 13.2.1`, пресет `stm32u5-release`, дата 2026-10-26. Замеры на
железе (скорость SD, ток, стек в рантайме) добавляются сюда по мере выполнения этапов 5 и 7
`TODO_migrate.md`.

## Размеры (Release)

| Прошивка | text | data | bss | RAM (data+bss) | Flash |
|---|---|---|---|---|---|
| U585 (768 КБ ОЗУ, 2 МБ Flash) | 111248 | 800 | 74992 | 75792 Б (9.6 %) | 112048 Б (text+data, 5.3 % из 2 МБ) |

Для сравнения: прошивка F411 (сборка до удаления, архивный релиз) занимала text 103020, data 288,
bss 74952 Б. Рост text на U585 (+8 КБ): драйвер SPI-SD (`sd_spi`), FatFs R0.16 вместо R0.12c из
CubeMX, HAL GPDMA.

## Программная арифметика

`tools/check_softfloat.sh` для .elf: soft-float helpers не найдены.

## Самые тяжёлые функции по стеку (U585, `-fstack-usage`, статически)

| Байт | Функция |
|---|---|
| 784 | `map_render_feature` (`src/core/map_render.c`) |
| 688 | `purrgo_config_save` |
| 656 | `purrgo_config_load` |
| 624 | `gfx_fill_compound_polygon` |
| 544 | `load_next_directory_page` |
| 288 | `purrgo_fs_readdir` (`apps/stm32/common/Src/fs_hal_stm32.c`) |

Это размер кадра самой функции, без вложенных вызовов. Размер стека задан в CubeMX
(`_Min_Stack_Size` = 0x2000); реальный запас в худшем кадре нужно измерить на плате
(раскраска стека `0xA5A5A5A5` и поиск первого изменённого слова после прогона).
