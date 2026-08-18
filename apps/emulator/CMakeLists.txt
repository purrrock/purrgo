# Find SDL2
find_package(SDL2 REQUIRED)

# Source files
file(GLOB EMULATOR_SOURCES CONFIGURE_DEPENDS "src/*.c")

# Executable
add_executable(purrgo_emulator ${EMULATOR_SOURCES})

# Target include directories
target_include_directories(purrgo_emulator PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Link libraries statically if needed, linking to purrgo_core from the parent project
target_link_libraries(purrgo_emulator PRIVATE purrgo_core SDL2::SDL2)
