# Скрипт режима -P: пишет purrgo_version.h (шаблон cmake/version.h.in).
# Вызывается при конфигурации и перед каждой сборкой цели purrgo_version
# (см. PurrgoVersion.cmake), поэтому ревизия git не устаревает между коммитами.
# configure_file() не трогает файл, если содержимое не изменилось, так что пересборка
# ядра запускается только при реальной смене версии, ревизии или типа сборки.
#
# Параметры (-D): SRC_DIR, TEMPLATE, OUT_FILE, PURRGO_VERSION, BUILD_TYPE.

find_package(Git QUIET)
set(PURRGO_GIT_REVISION "nogit")
if(GIT_FOUND AND EXISTS "${SRC_DIR}/.git")
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" describe --always --dirty --abbrev=7
        WORKING_DIRECTORY "${SRC_DIR}"
        OUTPUT_VARIABLE _rev
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _rc
        ERROR_QUIET)
    if(_rc EQUAL 0 AND NOT _rev STREQUAL "")
        set(PURRGO_GIT_REVISION "${_rev}")
    endif()
endif()

string(TOUPPER "${BUILD_TYPE}" _bt)
if(_bt STREQUAL "DEBUG")
    set(PURRGO_BUILD_IS_DEBUG 1)
else()
    set(PURRGO_BUILD_IS_DEBUG 0)
endif()

configure_file("${TEMPLATE}" "${OUT_FILE}" @ONLY)
