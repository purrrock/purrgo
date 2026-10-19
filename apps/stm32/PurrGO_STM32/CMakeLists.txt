cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once by STM32CubeMX,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#
# Подпроект прошивки STM32. Подключается из корневого CMakeLists.txt репозитория
# (PURRGO_PLATFORM=stm32, пресеты stm32-debug / stm32-release). Самостоятельным
# CMake-проектом больше не является: third_party, purrgo_core, project(),
# C-стандарт и тип сборки задаются корневым CMakeLists.txt.
#
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    message(FATAL_ERROR
        "Эту папку нельзя конфигурировать напрямую. "
        "Откройте корень репозитория и выберите пресет stm32-debug / stm32-release "
        "(cmake --preset stm32-debug).")
endif()

# cmake/stm32cubemx/CMakeLists.txt (генерируется CubeMX) обращается к цели
# прошивки через ${CMAKE_PROJECT_NAME}. Корневой проект называется purrgo, поэтому
# в области видимости этой папки (и дочерних) имя переопределяется.
set(CMAKE_PROJECT_NAME PurrGO_STM32)

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
    # Platform-dependent PurrGO logger.
    Core/Src/purrgo_logger.c

    # SD/FatFs support.
    Core/Src/fatfs_sd.c
    Core/Src/dma.c

    # STM32 implementation of the common GNSS byte-stream interface.
    Core/Src/gnss_io.c

    # STM32 system time implementation.
    Core/Src/system_time.c

    # STM32 button driver stub.
    Core/Src/buttons.c

    # STM32 stub implementation of the filesystem HAL.
    Core/Src/fs_hal_stm32.c

    # STM32 display implementation.
    Core/Src/display_stm32.c

    # Temporary TFT display driver for debug
    Core/Src/display_st7789.c

    # Debug button emulator via UART
    Core/Src/debug_buttons.c

    # STM32 Power logic
    Core/Src/power_stm32.c

    # Application wrapper
    Core/Src/stm32_app.c

    # Waveshare STM32 E-Ink 2.7" V2 driver.
    ${PROJECT_SOURCE_DIR}/third_party/e-Paper/STM32/STM32-F103ZET6/User/e-Paper/EPD_2in7_V2.c
    Core/Src/DEV_Config.c
    Core/Src/display_eink.c

    # Display test
    Core/Src/eink_test.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    # Add user defined include paths

    # Waveshare STM32 E-Paper V2 driver.
    ${PROJECT_SOURCE_DIR}/third_party/e-Paper/STM32/STM32-F103ZET6/User/e-Paper
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files.
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    purrgo_core
    # Add user defined libraries
)

# Артефакты (.elf, .map) лежат в корне каталога сборки пресета:
# out/build/<preset>/PurrGO_STM32.elf
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    ADDITIONAL_CLEAN_FILES   "${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.map"
)
