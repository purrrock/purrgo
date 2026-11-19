# Версия прошивки и генерация purrgo_version.h для загрузочного баннера
# (purrgo/boot_banner.h). Единственное место, где задан номер версии.

set(PURRGO_VERSION "0.1.0" CACHE STRING "PurrGO firmware version shown on the boot banner")

set(PURRGO_VERSION_HEADER_DIR "${CMAKE_BINARY_DIR}/generated")
set(PURRGO_VERSION_HEADER     "${PURRGO_VERSION_HEADER_DIR}/purrgo_version.h")

set(_purrgo_version_args
    -DSRC_DIR=${CMAKE_SOURCE_DIR}
    -DTEMPLATE=${CMAKE_SOURCE_DIR}/cmake/version.h.in
    -DOUT_FILE=${PURRGO_VERSION_HEADER}
    -DPURRGO_VERSION=${PURRGO_VERSION})

# Сразу при конфигурации: заголовок есть для IDE/clangd ещё до первой сборки.
execute_process(
    COMMAND ${CMAKE_COMMAND} ${_purrgo_version_args} -DBUILD_TYPE=${CMAKE_BUILD_TYPE}
            -P ${CMAKE_SOURCE_DIR}/cmake/PurrgoVersionGen.cmake)

# Перед каждой сборкой: ревизия git и тип сборки ($<CONFIG> верен и для многоконфигурационных
# генераторов).
add_custom_target(purrgo_version
    COMMAND ${CMAKE_COMMAND} ${_purrgo_version_args} -DBUILD_TYPE=$<CONFIG>
            -P ${CMAKE_SOURCE_DIR}/cmake/PurrgoVersionGen.cmake
    BYPRODUCTS ${PURRGO_VERSION_HEADER}
    COMMENT "Generating purrgo_version.h"
    VERBATIM)
