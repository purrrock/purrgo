#ifndef PURRGO_LOGGER_H
#define PURRGO_LOGGER_H

#include "purrgo/purrgo_format.h"

#ifdef __cplusplus
extern "C" {
#endif

void purrgo_logger_init(void);

void purrgo_logger_write(const char *format, ...) PURRGO_PRINTF_FORMAT(1, 2);

#define PURRGO_LOG(...) do { purrgo_logger_write(__VA_ARGS__); } while(0)

#ifdef __cplusplus
}
#endif

#endif /* PURRGO_LOGGER_H */
