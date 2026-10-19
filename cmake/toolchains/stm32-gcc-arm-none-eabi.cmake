# Тулчейн прошивки STM32 для сборки из корня репозитория.
#
# Тонкая обёртка над файлом, который генерирует STM32CubeMX
# (apps/stm32/PurrGO_STM32/cmake/gcc-arm-none-eabi.cmake): флаги MCU, оптимизации
# и т.д. по-прежнему берутся оттуда и обновляются при перегенерации CubeMX.
#
# CubeMX-файл рассчитан на то, что корень CMake-проекта == папка CubeMX-проекта,
# поэтому здесь исправляются две вещи, зависящие от этого предположения:
#   1. путь к linker script (-T ".../STM32F411xx_FLASH.ld");
#   2. имя map-файла (CMAKE_PROJECT_NAME на момент чтения тулчейна ещё не задан).

get_filename_component(PURRGO_CUBEMX_DIR
    "${CMAKE_CURRENT_LIST_DIR}/../../apps/stm32/PurrGO_STM32" ABSOLUTE)

include("${PURRGO_CUBEMX_DIR}/cmake/gcc-arm-none-eabi.cmake")

string(REPLACE "${CMAKE_SOURCE_DIR}/" "${PURRGO_CUBEMX_DIR}/"
    CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")

string(REGEX REPLACE "-Wl,-Map=[^ ]*"
    "-Wl,-Map=\"${CMAKE_BINARY_DIR}/PurrGO_STM32.map\""
    CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
