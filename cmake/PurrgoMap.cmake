# Map-файл линковки для КАЖДОГО исполняемого файла проекта.
#
#   purrgo_target_map(<target>)  - рядом с исполняемым файлом цели создаётся
#                                  <target>.map (GNU ld / MinGW / lld: -Map,
#                                  Apple ld: -map).
#
# Прошивка STM32 получает map-файл от тулчейна CubeMX (PurrGO_STM32U5.map в корне
# каталога сборки), здесь - PC-эмулятор и host-тесты. Любую новую
# add_executable() нужно сопровождать вызовом purrgo_target_map().
#
#   PURRGO_GENERATE_MAP=OFF - отключить (по умолчанию ON).

option(PURRGO_GENERATE_MAP "Generate linker .map file for every executable" ON)

function(purrgo_target_map target)
    if(NOT PURRGO_GENERATE_MAP)
        return()
    endif()
    set(_map "$<TARGET_FILE_DIR:${target}>/$<TARGET_FILE_BASE_NAME:${target}>.map")
    if(APPLE)
        target_link_options(${target} PRIVATE "LINKER:-map,${_map}")
    else()
        target_link_options(${target} PRIVATE "LINKER:-Map=${_map}")
    endif()
    set_property(TARGET ${target} APPEND PROPERTY ADDITIONAL_CLEAN_FILES "${_map}")
endfunction()
