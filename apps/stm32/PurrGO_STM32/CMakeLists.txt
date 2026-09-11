cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME PurrGO_STM32)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Add third-party dependencies required by the portable core.
add_subdirectory(../../../third_party third_party_build)

# Add the portable PurrGO core.
add_subdirectory(../../../src/core purrgo_core_build)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(PurrGO_STM32 PRIVATE
    # Platform-dependent PurrGO logger.
    Core/Src/purrgo_logger.c

    # SD/FatFs support.
    Core/Src/fatfs_sd.c

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
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ../../../include
    # Add user defined include paths
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