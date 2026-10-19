cmake_minimum_required(VERSION 3.22)

# ==============================================================================
# PurrGO — единая точка входа сборки.
#
# Конфигурация выбирается CMake-пресетом (см. CMakePresets.json), а не выбором
# корневой папки проекта:
#
#   win-mingw-debug / win-mingw-release  — PC-эмулятор, Windows (MSYS2 ucrt64)
#   linux-gcc-debug / linux-gcc-release  — PC-эмулятор, Linux
#   stm32-debug     / stm32-release      — прошивка STM32 (arm-none-eabi-gcc)
#
# Платформу задаёт кэш-переменная PURRGO_PLATFORM (pc | stm32), которую
# выставляет пресет. Без пресета по умолчанию собирается PC-эмулятор.
# ==============================================================================

set(PURRGO_PLATFORM "pc" CACHE STRING "Target platform: pc | stm32")
set_property(CACHE PURRGO_PLATFORM PROPERTY STRINGS pc stm32)
if(NOT PURRGO_PLATFORM MATCHES "^(pc|stm32)$")
    message(FATAL_ERROR "PURRGO_PLATFORM must be 'pc' or 'stm32' (got '${PURRGO_PLATFORM}')")
endif()

# Тип сборки по умолчанию для одноконфигурационных генераторов.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type")
endif()

# compile_commands.json для clangd / IntelliSense.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(PURRGO_PLATFORM STREQUAL "stm32")
    project(purrgo LANGUAGES C ASM)
else()
    project(purrgo LANGUAGES C)
endif()

message(STATUS "PurrGO platform: ${PURRGO_PLATFORM}, build type: ${CMAKE_BUILD_TYPE}")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(PURRGO_PLATFORM STREQUAL "pc")
    set(CMAKE_C_EXTENSIONS OFF)
else()
    # Как и раньше для прошивки: gnu11 (код CubeMX/HAL рассчитан на расширения).
    set(CMAKE_C_EXTENSIONS ON)
endif()

# Опции эмуляции GNSS (только PC; в прошивке мок не используется).
if(PURRGO_PLATFORM STREQUAL "pc")
    option(USE_MOCK_GNSS "Enable mock GNSS data generator" ON)
    if(USE_MOCK_GNSS)
        add_compile_definitions(USE_MOCK_GNSS)
    endif()
endif()

# ==============================================================================
# Общая часть: внешние зависимости и портативное ядро
# ==============================================================================

add_subdirectory(third_party)   # minmea
add_subdirectory(src/core)      # purrgo_core

# ==============================================================================
# Платформа
# ==============================================================================

if(PURRGO_PLATFORM STREQUAL "pc")
    message(STATUS "--- Target: PC Applications ---")
    add_subdirectory(src/platform/pc)   # purrgo_platform_pc
    add_subdirectory(apps/emulator)     # purrgo_emulator
else()
    message(STATUS "--- Target: STM32 firmware ---")
    add_subdirectory(apps/stm32/PurrGO_STM32)
endif()
