cmake_minimum_required(VERSION 3.22)

# ==============================================================================
# PurrGO — единая точка входа сборки.
#
# Конфигурация выбирается CMake-пресетом (см. CMakePresets.json), а не выбором
# корневой папки проекта:
#
#   win-mingw-debug / win-mingw-release  — PC-эмулятор, Windows (MSYS2 ucrt64)
#   linux-gcc-debug / linux-gcc-release  — PC-эмулятор, Linux
#   stm32u5-debug   / stm32u5-release    — прошивка STM32U585 (arm-none-eabi-gcc)
#
# Платформу задаёт кэш-переменная PURRGO_PLATFORM (pc | stm32u5), которую
# выставляет пресет. Без пресета по умолчанию собирается PC-эмулятор.
# ==============================================================================

set(PURRGO_PLATFORM "pc" CACHE STRING "Target platform: pc | stm32u5 (U585)")
set_property(CACHE PURRGO_PLATFORM PROPERTY STRINGS pc stm32u5)
if(NOT PURRGO_PLATFORM MATCHES "^(pc|stm32u5)$")
    message(FATAL_ERROR "PURRGO_PLATFORM must be 'pc' or 'stm32u5' (got '${PURRGO_PLATFORM}')")
endif()

# Тип сборки по умолчанию для одноконфигурационных генераторов.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type")
endif()

# compile_commands.json для clangd / IntelliSense.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(PURRGO_PLATFORM STREQUAL "stm32u5")
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
        # Стартовая точка mock (по умолчанию - Беларусь, 150 м). Пример для берега
        # Мёртвого моря: -DPURRGO_MOCK_LAT_1E7=315590000 -DPURRGO_MOCK_LON_1E7=354731666
        # -DPURRGO_MOCK_ALT_M=-430
        foreach(_mock_var PURRGO_MOCK_LAT_1E7 PURRGO_MOCK_LON_1E7 PURRGO_MOCK_ALT_M)
            set(${_mock_var} "" CACHE STRING "Override mock GNSS start value (empty = default)")
            if(NOT "${${_mock_var}}" STREQUAL "")
                add_compile_definitions(${_mock_var}=${${_mock_var}})
            endif()
        endforeach()
    endif()
endif()

# Опции сборки (только PC).
#   PURRGO_BUILD_EMULATOR - собирать SDL2-эмулятор (нужен SDL2); без него можно
#                           собрать ядро и host-тесты.
#   PURRGO_BUILD_TESTS    - host-тесты (tests/host), запуск через ctest.
#   PURRGO_SANITIZE       - ASan + UBSan для ядра и тестов (GCC/Clang).
option(PURRGO_BUILD_EMULATOR "Build the SDL2 PC emulator (pc platform)" ON)
option(PURRGO_BUILD_TESTS    "Build host unit tests (pc platform)" OFF)
option(PURRGO_SANITIZE       "Enable AddressSanitizer + UndefinedBehaviorSanitizer" OFF)

if(PURRGO_SANITIZE)
    add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

include(cmake/PurrgoWarnings.cmake)

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
    if(PURRGO_BUILD_EMULATOR)
        add_subdirectory(apps/emulator) # purrgo_emulator
    endif()
    if(PURRGO_BUILD_TESTS)
        enable_testing()
        add_subdirectory(tests/host)
    endif()
else()
    message(STATUS "--- Target: STM32U585 firmware ---")
    add_subdirectory(apps/stm32/PurrGO_STM32U5)
endif()
