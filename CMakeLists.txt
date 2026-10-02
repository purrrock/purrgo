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

