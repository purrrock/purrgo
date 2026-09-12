#ifndef PURRGO_PARSE_H
#define PURRGO_PARSE_H

#include <stdint.h>

/**
 * @brief Parses a 32-bit signed integer from a string.
 *
 * This function skips leading spaces and tabs, handles optional '+' or '-' signs,
 * and parses decimal digits until a non-digit character is encountered.
 *
 * If the parsed value exceeds INT32_MAX, it is clamped to INT32_MAX.
 * If the parsed value is below INT32_MIN, it is clamped to INT32_MIN.
 *
 * Note: This module handles only generic syntax parsing. Caller-side semantic
 * validation (such as GPS coordinate bounds or configuration value ranges) is
 * intentionally left to the caller.
 *
 * @param str The string to parse.
 * @return The parsed 32-bit integer, clamped to INT32_MIN / INT32_MAX on overflow.
 */
int32_t purrgo_parse_int32(const char* str);

#endif /* PURRGO_PARSE_H */
