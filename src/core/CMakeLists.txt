# Исходники ядра лежат по подсистемам: gnss/ map/ nav/ book/ power/ ui/ gfx/ sys/.
# Все .c в подпапках подхватываются автоматически.
file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS "*.c")

add_library(purrgo_core STATIC ${CORE_SOURCES})

# Указываем путь к глобальным заголовочным файлам (на 2 уровня выше)
target_include_directories(purrgo_core PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

# Приватные заголовки между подсистемами подключаются от корня ядра: #include "map/map_projection.h".
# Внутри одной подсистемы - по короткому имени (#include "map_idx.h").
target_include_directories(purrgo_core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# purrgo_version.h (версия, ревизия git, признак Debug для загрузочного баннера) генерируется
# при сборке в ${CMAKE_BINARY_DIR}/generated (cmake/PurrgoVersion.cmake); нужен только ядру.
target_include_directories(purrgo_core PRIVATE ${PURRGO_VERSION_HEADER_DIR})
add_dependencies(purrgo_core purrgo_version)

# Линкуем библиотеку парсинга NMEA
target_link_libraries(purrgo_core PRIVATE minmea)

# Предупреждения (-Wall -Wextra -Wshadow -Wformat=2 -Wconversion -Wdouble-promotion),
# -ffunction-sections -fdata-sections (см. cmake/PurrgoWarnings.cmake).
purrgo_target_warnings(purrgo_core)
