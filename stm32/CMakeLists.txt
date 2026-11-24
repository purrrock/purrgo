cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once by STM32CubeMX,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#
# Подпроект прошивки STM32U585. Подключается из корневого CMakeLists.txt
# репозитория (PURRGO_PLATFORM=stm32u5, пресеты stm32u5-debug / stm32u5-release).
# Самостоятельным CMake-проектом не является: third_party, purrgo_core,
# project(), C-стандарт и тип сборки задаются корневым CMakeLists.txt.
#
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    message(FATAL_ERROR
        "Эту папку нельзя конфигурировать напрямую. "
        "Откройте корень репозитория и выберите пресет stm32u5-debug / stm32u5-release "
        "(cmake --preset stm32u5-debug).")
endif()

# cmake/stm32cubemx/CMakeLists.txt (генерируется CubeMX) обращается к цели
# прошивки через ${CMAKE_PROJECT_NAME}. Корневой проект называется purrgo, поэтому
# в области видимости этой папки (и дочерних) имя переопределяется.
set(CMAKE_PROJECT_NAME PurrGO_STM32U5)

# Платформенные адаптеры STM32 (весь наш код прошивки) - stm32/port/<группа>/:
#   app/      главный цикл-обёртка, системное время, логгер, аппаратные тесты
#   input/    кнопки
#   display/  дисплей, очередь обновлений E-Ink, самотест
#   gnss/     UART+DMA, связь с модулем, диагностика
#   storage/  SD по SPI, FatFs, привязка ФС (storage/fatfs - сторонний код)
#   power/    питание, батарея, PVD, Stop 2
# CubeMX-файлы (Core/, Drivers/, startup) в port/ не входят.
set(PURRGO_STM32_PORT "${CMAKE_CURRENT_SOURCE_DIR}/port")

# Все .c на один уровень вглубь port/ (port/<группа>/*.c) подхватываются сами,
# новый файл в CMakeLists добавлять не нужно. FatFs лежит глубже (port/storage/fatfs)
# и сюда не попадает: сторонний код, предупреждений проекта не получает.
file(GLOB PURRGO_STM32_PORT_SOURCES CONFIGURE_DEPENDS "${PURRGO_STM32_PORT}/*/*.c")

# FatFs R0.16 (в CubeMX для STM32U5 middleware FATFS нет, D11).
# Интерфейс диска FatFs (disk_*) реализует port/storage/sd_card.c.
set(PURRGO_STM32_FATFS_SOURCES
    ${PURRGO_STM32_PORT}/storage/fatfs/ff.c
    ${PURRGO_STM32_PORT}/storage/fatfs/ffunicode.c
    ${PURRGO_STM32_PORT}/storage/fatfs/ffsystem.c
)

# Заголовки подключаются как раньше, без префикса группы (#include "sd_card.h"):
# в пути поиска попадает каждая папка port/<группа>. CubeMX-овский Core/Src/main.c
# тоже включает наши заголовки (stm32_app.h, hw_test.h, eink_test.h).
file(GLOB PURRGO_STM32_PORT_DIRS LIST_DIRECTORIES true "${PURRGO_STM32_PORT}/*")
set(PURRGO_STM32_PORT_INCLUDES "")
foreach(_dir IN LISTS PURRGO_STM32_PORT_DIRS)
    if(IS_DIRECTORY "${_dir}")
        list(APPEND PURRGO_STM32_PORT_INCLUDES "${_dir}")
    endif()
endforeach()
list(APPEND PURRGO_STM32_PORT_INCLUDES "${PURRGO_STM32_PORT}/storage/fatfs")

message(STATUS "Firmware target: ${CMAKE_PROJECT_NAME}")

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${PURRGO_STM32_PORT_SOURCES}
    ${PURRGO_STM32_FATFS_SOURCES}
)

# Предупреждения только для пользовательских файлов (не для CubeMX/HAL/FatFs/библиотеки epd4g).
# -ffunction-sections -fdata-sections -fstack-usage, --gc-sections и
# --print-memory-usage уже заданы CubeMX-тулчейном (cmake/gcc-arm-none-eabi.cmake).
purrgo_sources_warnings(${PURRGO_STM32_PORT_SOURCES})

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PURRGO_STM32_PORT_INCLUDES}
)

# E-Ink: библиотека epd4g (third_party/epd4g, часть проекта). INTERFACE-
# библиотека: её исходники компилируются в этом таргете, поэтому видят main.h,
# HAL, hspi1 и метки EINK_* из CubeMX. Порт STM32 HAL (epd4g_port_stm32_hal.c)
# использует hspi1 и порты EINK_CS/DC/RST/BUSY.
set(EPD4G_PORT_STM32_HAL ON)
add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/epd4g
                 ${CMAKE_BINARY_DIR}/epd4g)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols

    # 1-bit режим: только белый (3) -> белый, любой серый -> чёрный. Как в
    # прежнем драйвере Waveshare (меню и экран выключения рисуются двумя цветами).
    EPD4G_BW_THRESHOLD=3
)

# Режим аппаратного тестирования: вместо главного цикла навигатора после
# инициализации периферии выполняется один тест (см. port/app/hw_test.h).
#   cmake --preset stm32u5-release -DPURRGO_HW_TEST=SD_SPEED
# Пустое значение - обычная прошивка. Смена значения пересобирает проект.
set(PURRGO_HW_TEST_NAMES SD_SPEED SD_POWER GNSS_BBR EINK_4GRAY EINK_PARTIAL)
set(PURRGO_HW_TEST "" CACHE STRING
    "Hardware test instead of the navigator: ${PURRGO_HW_TEST_NAMES} (empty = normal firmware)")
set_property(CACHE PURRGO_HW_TEST PROPERTY STRINGS "" ${PURRGO_HW_TEST_NAMES})

# Параметры теста GNSS_BBR (port/app/hw_test_gnss.c). Пустое значение - значение по умолчанию из исходника.
#   cmake --preset stm32u5-release -DPURRGO_HW_TEST=GNSS_BBR -DPURRGO_HWTEST_GNSS_PSMOO_BAUD=115200
# Значение остаётся в кэше CMake: чтобы вернуть умолчание, передайте пустое (-DPURRGO_HWTEST_GNSS_PSMOO_BAUD=).
set(PURRGO_HWTEST_GNSS_PARAMS PHASES ACQ_TIMEOUT_S ACQ_HOLD_S EN_CYCLES EN_OFF_S EN_FIX_TIMEOUT_S
    PSMOO_PERIOD_S PSMOO_ONTIME_S PSMOO_CYCLES PSMOO_BAUD HOT_MS)
foreach(_p IN LISTS PURRGO_HWTEST_GNSS_PARAMS)
    set(PURRGO_HWTEST_GNSS_${_p} "" CACHE STRING "GNSS_BBR test parameter ${_p} (empty = default)")
endforeach()

if(NOT PURRGO_HW_TEST STREQUAL "")
    if(NOT PURRGO_HW_TEST IN_LIST PURRGO_HW_TEST_NAMES)
        message(FATAL_ERROR
            "PURRGO_HW_TEST='${PURRGO_HW_TEST}': допустимо ${PURRGO_HW_TEST_NAMES} или пусто")
    endif()
    message(STATUS "!!! HARDWARE TEST BUILD: ${PURRGO_HW_TEST} (навигатор не запускается) !!!")
    if(PURRGO_HW_TEST STREQUAL "EINK_4GRAY")
        target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE PURRGO_EINK_SELFTEST_4GRAY)
    elseif(PURRGO_HW_TEST STREQUAL "EINK_PARTIAL")
        target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE PURRGO_EINK_SELFTEST_PARTIAL)
    else()
        target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
            PURRGO_HW_TEST_ENABLED PURRGO_HW_TEST_${PURRGO_HW_TEST})
    endif()
    if(PURRGO_HW_TEST STREQUAL "GNSS_BBR")
        foreach(_p IN LISTS PURRGO_HWTEST_GNSS_PARAMS)
            set(_v "${PURRGO_HWTEST_GNSS_${_p}}")
            if(NOT _v STREQUAL "")
                if(NOT _v MATCHES "^[0-9]+$")
                    message(FATAL_ERROR "PURRGO_HWTEST_GNSS_${_p}='${_v}': нужно целое число")
                endif()
                message(STATUS "GNSS_BBR: PURRGO_HWTEST_GNSS_${_p}=${_v}")
                target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE PURRGO_HWTEST_GNSS_${_p}=${_v})
            endif()
        endforeach()
    endif()
endif()

# Remove wrong libob.a library dependency when using cpp files.
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    purrgo_core
    epd4g
    # Add user defined libraries
)

# Артефакты (.elf, .map) лежат в корне каталога сборки пресета:
# out/build/<preset>/PurrGO_STM32U5.elf
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    ADDITIONAL_CLEAN_FILES   "${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.map"
)
