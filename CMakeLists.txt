cmake_minimum_required(VERSION 3.20)

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
# 1. ПОРТАТИВНОЕ ЯДРО
# ==============================================================================

# Автоматически ищем все .c файлы в src/core и её подпапках
file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS "src/core/*.c")

add_library(purrgo_core STATIC ${CORE_SOURCES})

target_include_directories(purrgo_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(purrgo_core PRIVATE minmea)

# ==============================================================================
# 2. PC APPLICATIONS
# ==============================================================================

message(STATUS "--- Target: PC Applications (.exe) ---")

# Платформенный слой для ПК
file(GLOB_RECURSE PC_PLATFORM_SOURCES CONFIGURE_DEPENDS "src/platform/pc/*.c")

add_library(purrgo_platform_pc STATIC ${PC_PLATFORM_SOURCES})

target_include_directories(purrgo_platform_pc PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/pc
)

# Графический эмулятор
add_subdirectory(apps/emulator)

# ==============================================================================
# 3. МОДУЛЬНОЕ ТЕСТИРОВАНИЕ
# ==============================================================================

enable_testing()

# Макрос для быстрого добавления тестов без дублирования кода
macro(add_purrgo_test test_name test_source)
    add_executable(${test_name} ${test_source})
    target_link_libraries(${test_name} PRIVATE purrgo_core purrgo_platform_pc)
    target_link_options(${test_name} PRIVATE "-Wl,--allow-multiple-definition")
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
add_purrgo_test(test_map_culling tests/core/test_map_culling.c)
add_purrgo_test(test_map_style tests/core/test_map_style.c)
add_purrgo_test(test_navigation tests/core/test_navigation.c)
add_purrgo_test(test_map_mlp tests/core/test_map_mlp.c)
add_purrgo_test(test_track_renderer tests/core/test_track_renderer.c)
add_purrgo_test(test_sun tests/core/test_sun.c)
add_purrgo_test(test_gpx_parser tests/core/test_gpx_parser.c)
