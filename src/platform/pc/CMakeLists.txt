# Платформенный слой для ПК (подключается из корневого CMakeLists.txt при PURRGO_PLATFORM=pc)

file(GLOB_RECURSE PC_PLATFORM_SOURCES CONFIGURE_DEPENDS "*.c")

add_library(purrgo_platform_pc STATIC ${PC_PLATFORM_SOURCES})

target_include_directories(purrgo_platform_pc PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)
