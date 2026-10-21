# Единый набор предупреждений компилятора для КОДА ПРОЕКТА (src/core, src/platform,
# apps/*/src, пользовательские файлы прошивки). Сторонний и сгенерированный CubeMX
# код (HAL, FatFs, minmea, драйвер Waveshare) эти флаги не получает.
#
#   PURRGO_WERROR=ON  - превращает предупреждения в ошибки (включена в пресете
#                       linux-gcc-test, то есть в CI).

option(PURRGO_WERROR "Treat warnings as errors in project code" OFF)

set(PURRGO_WARNING_FLAGS
    -Wall -Wextra -Wshadow -Wformat=2 -Wconversion -Wdouble-promotion)

# Флаги, которые нужны и для размера бинарника: каждая функция/объект в своей
# секции, чтобы --gc-sections выбрасывал неиспользуемое. На STM32 их (и -fstack-usage)
# уже задаёт CubeMX-тулчейн; для PC-сборки они добавляются здесь.
set(PURRGO_SECTION_FLAGS -ffunction-sections -fdata-sections)

function(purrgo_target_warnings target)
    target_compile_options(${target} PRIVATE ${PURRGO_WARNING_FLAGS} ${PURRGO_SECTION_FLAGS})
    if(PURRGO_WERROR)
        target_compile_options(${target} PRIVATE -Werror)
    endif()
endfunction()

# Для файлов, добавленных в чужую цель (прошивка STM32): флаги только на выбранные источники.
function(purrgo_sources_warnings)
    set(_flags ${PURRGO_WARNING_FLAGS})
    if(PURRGO_WERROR)
        list(APPEND _flags -Werror)
    endif()
    set_source_files_properties(${ARGN} PROPERTIES COMPILE_OPTIONS "${_flags}")
endfunction()
