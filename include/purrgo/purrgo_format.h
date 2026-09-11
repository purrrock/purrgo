#ifndef PURRGO_FORMAT_H
#define PURRGO_FORMAT_H

#include <stddef.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

int purrgo_vsnprintf(char* buf, size_t size, const char* format, va_list args);
int purrgo_snprintf(char* buf, size_t size, const char* format, ...);

#ifdef __cplusplus
}
#endif

#endif // PURRGO_FORMAT_H
