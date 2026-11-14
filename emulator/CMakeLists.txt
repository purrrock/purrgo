file(GLOB EMULATOR_SOURCES CONFIGURE_DEPENDS "src/*.c")

add_executable(purrgo_emulator ${EMULATOR_SOURCES})

target_include_directories(purrgo_emulator PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

purrgo_target_warnings(purrgo_emulator)
purrgo_target_map(purrgo_emulator)

if(EMSCRIPTEN)
    # ==========================================================================
    # Web-сборка (WebAssembly): эмулятор в браузере.
    #
    #   emcmake cmake --preset web-release   (или пресет в VS Code)
    #
    # SDL2 берётся из портов Emscripten (-sUSE_SDL=2), find_package не нужен.
    # Результат: index.html + index.js + index.wasm (+ index.data с образом ФС).
    #
    # PURRGO_WEB_PRELOAD_DIR - каталог, содержимое которого кладётся в корень
    # виртуальной ФС (maps/, books/, routes/ ...). Пусто - эмулятор стартует без
    # карты (на экране будет "карта не найдена").
    # ==========================================================================
    set(PURRGO_WEB_PRELOAD_DIR "" CACHE PATH
        "Directory packed into the virtual FS root (maps/, books/, routes/...)")

    set_target_properties(purrgo_emulator PROPERTIES
        OUTPUT_NAME "index"
        SUFFIX ".html")

    target_compile_options(purrgo_emulator PRIVATE -sUSE_SDL=2)
    target_link_options(purrgo_emulator PRIVATE
        -sUSE_SDL=2
        -sALLOW_MEMORY_GROWTH=1
        -sINITIAL_MEMORY=64MB
        -sSTACK_SIZE=1MB
        -sEXPORTED_RUNTIME_METHODS=ccall,cwrap
        --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html)

    if(PURRGO_WEB_PRELOAD_DIR)
        target_link_options(purrgo_emulator PRIVATE
            "--preload-file" "${PURRGO_WEB_PRELOAD_DIR}@/")
        set_property(TARGET purrgo_emulator APPEND PROPERTY LINK_DEPENDS
            "${PURRGO_WEB_PRELOAD_DIR}")
    endif()

    target_link_libraries(purrgo_emulator PRIVATE purrgo_core purrgo_platform_pc)
else()
    # Поиск установленной в MSYS2/MinGW библиотеки SDL2
    find_package(SDL2 REQUIRED)

    # Выбрасывать неиспользуемые секции (флаги -ffunction-sections уже заданы выше).
    if(NOT APPLE)
        target_link_options(purrgo_emulator PRIVATE -Wl,--gc-sections)
    endif()

    # Линковка с ядром проекта и импортированными целями SDL2
    target_link_libraries(purrgo_emulator PRIVATE purrgo_core purrgo_platform_pc SDL2::SDL2main SDL2::SDL2)

    if(WIN32)
        # Автоматическое копирование SDL2.dll в директорию сборки эмулятора
        add_custom_command(TARGET purrgo_emulator POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "C:/msys64/ucrt64/bin/SDL2.dll"
            $<TARGET_FILE_DIR:purrgo_emulator>
        )
    endif()
endif()
