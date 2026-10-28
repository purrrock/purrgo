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

# test_gnss_diag проверяет разбор NMEA и определение состояния связи с GNSS
# (HAL-независимый модуль прошивки STM32).
if(TARGET test_gnss_diag)
    target_sources(test_gnss_diag PRIVATE
        ${PROJECT_SOURCE_DIR}/apps/stm32/common/gnss_diag/gnss_diag.c)
    target_include_directories(test_gnss_diag PRIVATE
        ${PROJECT_SOURCE_DIR}/apps/stm32/common/gnss_diag)
endif()

# test_display_eink проверяет очередь обновлений E-Ink (display_eink.c) поверх
# настоящей библиотеки epd4g и модели панели; HAL подменяют заглушки stm32_shim.
# Нужен сабмодуль third_party/eink_part_refresh.
if(TARGET test_display_eink)
    set(EPD4G_DIR ${PROJECT_SOURCE_DIR}/third_party/eink_part_refresh/epd4g)
    if(EXISTS ${EPD4G_DIR}/src/epd4g.c)
        target_sources(test_display_eink PRIVATE
            ${PROJECT_SOURCE_DIR}/apps/stm32/common/Src/display_eink.c
            ${EPD4G_DIR}/src/epd4g.c
            ${EPD4G_DIR}/src/epd4g_fb.c)
        target_include_directories(test_display_eink PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/stm32_shim
            ${PROJECT_SOURCE_DIR}/apps/stm32/common/Inc
            ${EPD4G_DIR}/include)
        target_compile_definitions(test_display_eink PRIVATE EPD4G_BW_THRESHOLD=3)
    else()
        message(WARNING "third_party/eink_part_refresh не загружен: test_display_eink пропущен")
        set_target_properties(test_display_eink PROPERTIES EXCLUDE_FROM_ALL TRUE)
        set_tests_properties(test_display_eink PROPERTIES DISABLED TRUE)
    endif()
endif()

# eink_selftest_host прогоняет встроенный самотест частичного обновления
# (apps/stm32/common/Src/eink_test.c) на модели 1-bit контроллера панели и сверяет
# картинку на «стекле» с framebuffer'ом. Нужен сабмодуль third_party/eink_part_refresh.
if(EXISTS ${EPD4G_DIR}/src/epd4g.c)
    add_executable(eink_selftest_host
        ${CMAKE_CURRENT_SOURCE_DIR}/eink_selftest_host.c
        ${PROJECT_SOURCE_DIR}/apps/stm32/common/Src/eink_test.c
        ${PROJECT_SOURCE_DIR}/apps/stm32/common/Src/display_stm32.c
        ${PROJECT_SOURCE_DIR}/apps/stm32/common/Src/display_eink.c
        ${EPD4G_DIR}/src/epd4g.c
        ${EPD4G_DIR}/src/epd4g_fb.c
        ${PROJECT_SOURCE_DIR}/src/platform/pc/purrgo_logger.c)
    target_include_directories(eink_selftest_host PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/stm32_shim
        ${PROJECT_SOURCE_DIR}/apps/stm32/common/Inc
        ${EPD4G_DIR}/include)
    target_compile_definitions(eink_selftest_host PRIVATE EPD4G_BW_THRESHOLD=3)
    target_link_options(eink_selftest_host PRIVATE -Wl,--wrap=display_set_mode)
    purrgo_target_warnings(eink_selftest_host)
    target_link_libraries(eink_selftest_host PRIVATE purrgo_core m)
    add_test(NAME eink_selftest_host COMMAND eink_selftest_host 1
             WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    # Реальная панель меняет плоскости RAM ролями после обновления (модель 2): нужна версия
    # epd4g с EPD4G_BW_RESYNC, иначе в сценах 1-bit остаются следы предыдущих кадров.
    add_test(NAME eink_selftest_host_swap COMMAND eink_selftest_host 2
             WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
endif()
