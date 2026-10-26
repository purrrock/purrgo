# Host-тесты ядра (PC). Подключаются из корня при -DPURRGO_BUILD_TESTS=ON.
# Каждый test_*.c - отдельный исполняемый файл и отдельный тест ctest.
# Платформенный слой подменяется: ФС - tests/host/fake_fs.c (с внедрением сбоев),
# хуки выключения - tests/host/fake_power.c,
# дисплей и логгер - PC-реализации.

file(GLOB PURRGO_HOST_TESTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/test_*.c")

foreach(test_src IN LISTS PURRGO_HOST_TESTS)
    get_filename_component(test_name ${test_src} NAME_WE)
    add_executable(${test_name}
        ${test_src}
        ${CMAKE_CURRENT_SOURCE_DIR}/fake_fs.c
        ${CMAKE_CURRENT_SOURCE_DIR}/fake_power.c
        ${PROJECT_SOURCE_DIR}/src/platform/pc/display_hal.c
        ${PROJECT_SOURCE_DIR}/src/platform/pc/purrgo_logger.c)
    target_include_directories(${test_name} PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src/core
        ${PROJECT_SOURCE_DIR}/src/core/ui
        ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_definitions(${test_name} PRIVATE
        TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
    purrgo_target_warnings(${test_name})
    target_link_libraries(${test_name} PRIVATE purrgo_core minmea m)
    add_test(NAME ${test_name} COMMAND ${test_name}
             WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
endforeach()

# test_sd_spi проверяет протокол microSD в SPI-режиме (HAL-независимый модуль
# прошивки U585) на программной модели карты.
if(TARGET test_sd_spi)
    target_sources(test_sd_spi PRIVATE
        ${PROJECT_SOURCE_DIR}/apps/stm32/common/sd_spi/sd_spi.c)
    target_include_directories(test_sd_spi PRIVATE
        ${PROJECT_SOURCE_DIR}/apps/stm32/common/sd_spi)
endif()
