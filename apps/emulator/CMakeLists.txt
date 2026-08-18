# Поиск установленной в MSYS2/MinGW библиотеки SDL2
find_package(SDL2 REQUIRED)

file(GLOB EMULATOR_SOURCES CONFIGURE_DEPENDS "src/*.c")

add_executable(purrgo_emulator ${EMULATOR_SOURCES})

target_include_directories(purrgo_emulator PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Линковка с ядром проекта и импортированными целями SDL2
target_link_libraries(purrgo_emulator PRIVATE purrgo_core SDL2::SDL2main SDL2::SDL2)