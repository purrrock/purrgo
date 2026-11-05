#ifndef PURRGO_LOGGER_H
#define PURRGO_LOGGER_H

#include "purrgo/purrgo_format.h"

#ifdef __cplusplus
extern "C" {
#endif

void purrgo_logger_init(void);

void purrgo_logger_write(const char *format, ...) PURRGO_PRINTF_FORMAT(1, 2);

/*
 * Уровни логирования (выбираются при сборке: -DPURRGO_LOG_LEVEL=<n>, в CMake -
 * кэш-переменная PURRGO_LOG_LEVEL).
 *
 *   0 NONE  - ничего не выводится;
 *   1 ERROR - только ошибки (PURRGO_LOGE);
 *   2 WARN  - + предупреждения (PURRGO_LOGW);
 *   3 INFO  - + обычные сообщения (PURRGO_LOGI, он же PURRGO_LOG);
 *   4 DEBUG - + отладка (PURRGO_LOGD).
 *
 * По умолчанию (если CMake ничего не передал): INFO. CMake для Release и
 * MinSizeRel передаёт ERROR, для Debug - INFO.
 */
#define PURRGO_LOG_LEVEL_NONE   0
#define PURRGO_LOG_LEVEL_ERROR  1
#define PURRGO_LOG_LEVEL_WARN   2
#define PURRGO_LOG_LEVEL_INFO   3
#define PURRGO_LOG_LEVEL_DEBUG  4

#ifndef PURRGO_LOG_LEVEL
#define PURRGO_LOG_LEVEL PURRGO_LOG_LEVEL_DEBUG
#endif

#if (PURRGO_LOG_LEVEL < PURRGO_LOG_LEVEL_NONE) || (PURRGO_LOG_LEVEL > PURRGO_LOG_LEVEL_DEBUG)
#error "PURRGO_LOG_LEVEL must be 0..4"
#endif

/*
 * Отключённый уровень превращается в `if (0)`: формат по-прежнему проверяется
 * компилятором (printf-атрибут), аргументы не вычисляются, а код и строки
 * формата выбрасываются оптимизатором, поэтому в Release логи не занимают флеш.
 */
#define PURRGO_LOG_AT(level, ...) \
    do { if ((level) <= PURRGO_LOG_LEVEL) { purrgo_logger_write(__VA_ARGS__); } } while (0)

#define PURRGO_LOGE(...) PURRGO_LOG_AT(PURRGO_LOG_LEVEL_ERROR, __VA_ARGS__)
#define PURRGO_LOGW(...) PURRGO_LOG_AT(PURRGO_LOG_LEVEL_WARN,  __VA_ARGS__)
#define PURRGO_LOGI(...) PURRGO_LOG_AT(PURRGO_LOG_LEVEL_INFO,  __VA_ARGS__)
#define PURRGO_LOGD(...) PURRGO_LOG_AT(PURRGO_LOG_LEVEL_DEBUG, __VA_ARGS__)

/* Исторический макрос: обычное сообщение уровня INFO. */
#define PURRGO_LOG(...)  PURRGO_LOGI(__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* PURRGO_LOGGER_H */
