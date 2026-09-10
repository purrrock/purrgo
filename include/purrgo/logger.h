#ifndef PURRGO_LOGGER_H
#define PURRGO_LOGGER_H

#ifdef __cplusplus
extern "C" {
#endif

void purrgo_logger_init(void);
void purrgo_logger_write(const char *format, ...);

#define PURRGO_LOG(...) do { purrgo_logger_write(__VA_ARGS__); } while(0)

#ifdef __cplusplus
}
#endif

#endif /* PURRGO_LOGGER_H */
