# Автоматический поиск всех .c файлов в текущей папке и подпапках
file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS "*.c")

add_library(purrgo_core STATIC ${CORE_SOURCES})

# Указываем путь к глобальным заголовочным файлам (на 2 уровня выше)
target_include_directories(purrgo_core PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

# Линкуем библиотеку парсинга NMEA
target_link_libraries(purrgo_core PRIVATE minmea)

# Предупреждения (-Wall -Wextra -Wshadow -Wformat=2 -Wconversion -Wdouble-promotion),
# -ffunction-sections -fdata-sections (см. cmake/PurrgoWarnings.cmake).
purrgo_target_warnings(purrgo_core)
