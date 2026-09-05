cmake_minimum_required(VERSION 3.20)
# Указываем CMake не линковать исполняемый файл при проверке кросс-компилятора
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

project(purrgo C)

set(CMAKE_C_STANDARD 11)
# ... остальной код файла без изменений ...
project(purrgo C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Опции эмуляции GNSS
option(USE_MOCK_GNSS "Enable mock GNSS data generator" ON)
if(USE_MOCK_GNSS)
    add_compile_definitions(USE_MOCK_GNSS)
endif()

# Внешние зависимости (например, minmea)
add_subdirectory(third_party)

# ==============================================================================
# 1. ПОРТАТИВНОЕ ЯДРО (Собирается для всех платформ)
# ==============================================================================

# Автоматически ищем все .c файлы в src/core и её подпапках
file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS "src/core/*.c")

add_library(purrgo_core STATIC ${CORE_SOURCES})

target_include_directories(purrgo_core PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(purrgo_core PRIVATE minmea)

# ==============================================================================
# 2. МАРШРУТИЗАЦИЯ СБОРКИ НА ОСНОВЕ ВЫБРАННОГО КОМПИЛЯТОРА
# ==============================================================================

if(CMAKE_C_COMPILER MATCHES "arm-none-eabi")
    # ---------------------------------------------------------
    # ВЕТКА МИКРОКОНТРОЛЛЕРА (Кросс-компиляция)
    # ---------------------------------------------------------
    message(STATUS "--- Target: STM32 Firmware (.elf) ---")
    
    # 1. Подключаем проект, сгенерированный STM32CubeMX
    add_subdirectory(apps/stm32/PurrGO_STM32)
    
    # 2. Привязываем наше портативное ядро к сгенерированному таргету STM32.
    # CubeMX обычно называет target именем проекта, т.е. PurrGO_STM32.
    # Если CubeMX выдаст ошибку "Cannot specify link libraries for target...",
    # убедитесь, что имя таргета в файле apps/stm32/PurrGO_STM32/CMakeLists.txt 
    # действительно "PurrGO_STM32" (или ${CMAKE_PROJECT_NAME}).
    
    # Даем платформенному коду STM32 доступ к заголовкам ядра (#include "purrgo/...")
    target_include_directories(PurrGO_STM32 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

else()
    # ---------------------------------------------------------
    # ВЕТКА ПК (Windows / Linux / macOS)
    # ---------------------------------------------------------
    message(STATUS "--- Target: PC Applications (.exe) ---")
    
    # Платформенный слой для ПК (Адаптеры файловой системы, Serial портов и т.д.)
    file(GLOB_RECURSE PC_PLATFORM_SOURCES CONFIGURE_DEPENDS "src/platform/pc/*.c")
    add_library(purrgo_platform_pc STATIC ${PC_PLATFORM_SOURCES})
    target_include_directories(purrgo_platform_pc PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/pc
    )

    # Сборка базового PC-приложения (заглушка)
    file(GLOB PC_APP_SOURCES CONFIGURE_DEPENDS "apps/pc/*.c")
    if(PC_APP_SOURCES)
        add_executable(purrgo_pc ${PC_APP_SOURCES})
        target_link_libraries(purrgo_pc PRIVATE purrgo_core purrgo_platform_pc)
    endif()

    # Сборка утилиты логирования с реального приемника (Realtime Logger)
    file(GLOB_RECURSE LOGGER_APP_SOURCES CONFIGURE_DEPENDS "apps/pc/pc_realtime_logger/*.c")
    if(LOGGER_APP_SOURCES)
        add_executable(pc_realtime_logger ${LOGGER_APP_SOURCES})
        target_link_libraries(pc_realtime_logger PRIVATE purrgo_core purrgo_platform_pc)
    endif()

    # Подключаем графический эмулятор
    add_subdirectory(apps/emulator)

    # ---------------------------------------------------------
    # МОДУЛЬНОЕ ТЕСТИРОВАНИЕ (Только для ПК)
    # ---------------------------------------------------------
    enable_testing()

    # Макрос для быстрого добавления тестов без дублирования кода
    macro(add_purrgo_test test_name test_source)
        add_executable(${test_name} ${test_source})
        target_link_libraries(${test_name} PRIVATE purrgo_core purrgo_platform_pc)
        add_test(NAME ${test_name} COMMAND ${test_name})
    endmacro()

    add_purrgo_test(test_geo tests/core/test_geo.c)
    add_purrgo_test(test_gnss tests/core/test_gnss.c)
    add_purrgo_test(test_track_logger tests/core/test_track_logger.c)
    add_purrgo_test(test_gfx_polygon tests/core/test_gfx_polygon.c)
    add_purrgo_test(test_gfx_clipping tests/core/test_gfx_clipping.c)
    add_purrgo_test(test_config tests/core/test_config.c)
    add_purrgo_test(test_app_fsm tests/core/test_app_fsm.c)
    add_purrgo_test(test_purrgo_time tests/core/test_purrgo_time.c)
    add_purrgo_test(test_map_lod tests/core/test_map_lod.c)
    add_purrgo_test(test_navigation tests/core/test_navigation.c)
    add_purrgo_test(test_map_mlp tests/core/test_map_mlp.c)
    add_purrgo_test(test_track_renderer tests/core/test_track_renderer.c)

endif()