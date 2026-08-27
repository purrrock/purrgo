cmake_minimum_required(VERSION 3.20)
project(purrgo C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Определение значения по умолчанию в зависимости от целевой ОС
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(DEFAULT_BUILD_EMULATOR ON)
else()
    # Для кросс-компиляции (CMAKE_SYSTEM_NAME = "Generic" или "Linux")
    set(DEFAULT_BUILD_EMULATOR OFF)
endif()

# Инициализация опции с динамическим значением по умолчанию
option(BUILD_EMULATOR "Build hardware-in-the-loop PC emulator" ${DEFAULT_BUILD_EMULATOR})

option(USE_MOCK_GNSS "Enable mock GNSS data generator" ON)
if(USE_MOCK_GNSS)
    add_compile_definitions(USE_MOCK_GNSS)
endif()

add_subdirectory(third_party)

# Автоматический рекурсивный поиск всех .c файлов в директории src/core/.
set(CORE_SOURCES
    src/core/app_fsm.c
    src/core/app_ui.c
    src/core/ui/ui_map.c
    src/core/ui/ui_trip.c
    src/core/ui/ui_config.c
    src/core/ui/ui_dir_select.c
    src/core/config.c
    src/core/geo.c
    src/core/gfx/font5x7.c
    src/core/gfx/gfx_circle.c
    src/core/gfx/gfx_line.c
    src/core/gfx/gfx_polygon.c
    src/core/gfx/gfx_rect.c
    src/core/gfx/gfx_renderer.c
    src/core/gfx/gfx_text.c
    src/core/gnss.c
    src/core/gnss_adapter.c
    src/core/gnss_mock.c
    src/core/gpx_parser.c
    src/core/map.c
    src/core/map_culling.c
    src/core/map_idx.c
    src/core/map_mlp.c
    src/core/map_projection.c
    src/core/map_render.c
    src/core/map_style.c
    src/core/navigation.c
    src/core/purrgo_time.c
    src/core/sun.c
    src/core/sun_tables.c
    src/core/track_logger.c
    src/core/ubx.c
)

add_library(purrgo_core
    ${CORE_SOURCES}
)

target_include_directories(purrgo_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(purrgo_core PRIVATE minmea)

file(GLOB_RECURSE UBLOX_SOURCES CONFIGURE_DEPENDS "src/platform/ublox/*.c")
add_library(purrgo_ublox7
    ${UBLOX_SOURCES}
)

target_include_directories(purrgo_ublox7 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/ublox
)

target_link_libraries(purrgo_ublox7 PUBLIC purrgo_core)

if (WIN32 OR UNIX)
    file(GLOB_RECURSE PC_PLATFORM_SOURCES CONFIGURE_DEPENDS "src/platform/pc/*.c")
    
    add_library(purrgo_platform_pc
        ${PC_PLATFORM_SOURCES}
    )
    
target_include_directories(purrgo_platform_pc
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/pc
)

    file(GLOB PC_APP_SOURCES CONFIGURE_DEPENDS "apps/pc/*.c")
    add_executable(purrgo_pc
        ${PC_APP_SOURCES}
    )
    target_link_libraries(purrgo_pc PRIVATE purrgo_core purrgo_platform_pc)

    file(GLOB_RECURSE LOGGER_APP_SOURCES CONFIGURE_DEPENDS "apps/pc/pc_realtime_logger/*.c")
    add_executable(pc_realtime_logger
        ${LOGGER_APP_SOURCES}
    )
    target_link_libraries(pc_realtime_logger PRIVATE purrgo_core purrgo_platform_pc)

    enable_testing()

    add_executable(test_geo tests/core/test_geo.c)
    target_link_libraries(test_geo PRIVATE purrgo_core purrgo_platform_pc)
    add_test(NAME test_geo COMMAND test_geo)

    add_executable(test_gnss tests/core/test_gnss.c)
    target_link_libraries(test_gnss PRIVATE purrgo_core purrgo_platform_pc)
    add_test(NAME test_gnss COMMAND test_gnss)

    add_executable(test_track_logger tests/core/test_track_logger.c)
    target_link_libraries(test_track_logger PRIVATE purrgo_core purrgo_platform_pc)
    add_test(NAME test_track_logger COMMAND test_track_logger)

    add_executable(test_gfx_polygon tests/core/test_gfx_polygon.c)
    target_link_libraries(test_gfx_polygon PRIVATE purrgo_core purrgo_platform_pc)
    add_test(NAME test_gfx_polygon COMMAND test_gfx_polygon)

    add_executable(test_gfx_clipping tests/core/test_gfx_clipping.c)
    target_link_libraries(test_gfx_clipping PRIVATE purrgo_core purrgo_platform_pc)
    add_test(NAME test_gfx_clipping COMMAND test_gfx_clipping)

    add_executable(test_config tests/core/test_config.c)
    target_link_libraries(test_config PRIVATE purrgo_core purrgo_platform_pc)
    add_test(NAME test_config COMMAND test_config)

    add_executable(test_app_fsm tests/core/test_app_fsm.c)
    target_link_libraries(test_app_fsm PRIVATE purrgo_core purrgo_platform_pc)
    add_test(NAME test_app_fsm COMMAND test_app_fsm)

    add_executable(test_purrgo_time tests/core/test_purrgo_time.c)
    target_link_libraries(test_purrgo_time PRIVATE purrgo_core purrgo_platform_pc)
    add_test(NAME test_purrgo_time COMMAND test_purrgo_time)

    add_executable(test_map_lod tests/core/test_map_lod.c)
    target_link_libraries(test_map_lod PRIVATE purrgo_core purrgo_platform_pc)
    add_test(NAME test_map_lod COMMAND test_map_lod)
endif()

# Подключение директории эмулятора перенесено в конец дерева сборки, 
# чтобы цель purrgo_core уже существовала в контексте CMake на момент вызова.
if(BUILD_EMULATOR)
    add_subdirectory(apps/emulator)
endif()
