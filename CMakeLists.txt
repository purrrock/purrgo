cmake_minimum_required(VERSION 3.20)
project(purrgo C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(purrgo_core
    src/core/geo.c
    src/core/gnss.c
    src/core/navigation.c
)

    add_library(purrgo_ublox7
        src/platform/ublox/ublox7_config.c
    )

    target_include_directories(purrgo_ublox7 PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/ublox
    )

    target_link_libraries(purrgo_ublox7 PUBLIC purrgo_core)

add_subdirectory(third_party)

target_include_directories(purrgo_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if (WIN32 OR UNIX)
    add_library(purrgo_platform_pc
        src/platform/pc/platform_pc.c
    )
    target_include_directories(purrgo_platform_pc
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/pc
    )

    add_executable(purrgo_pc
        apps/pc/main.c
    )
    target_link_libraries(purrgo_pc PRIVATE purrgo_core purrgo_platform_pc)

    enable_testing()

    add_executable(test_geo tests/core/test_geo.c)
    target_link_libraries(test_geo PRIVATE purrgo_core)
    add_test(NAME test_geo COMMAND test_geo)

    add_executable(test_gnss tests/core/test_gnss.c)
    target_link_libraries(test_gnss PRIVATE purrgo_core)
    add_test(NAME test_gnss COMMAND test_gnss)
endif()
