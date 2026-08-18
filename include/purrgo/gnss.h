#ifndef PURRGO_GNSS_H
#define PURRGO_GNSS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Incremental byte-oriented GNSS/NMEA parser interface. */
typedef struct {
    char line[128];
    size_t length;
} purrgo_gnss_parser_t;

void purrgo_gnss_parser_init(purrgo_gnss_parser_t *parser);
bool purrgo_gnss_parser_feed(purrgo_gnss_parser_t *parser, uint8_t byte);

#endif /* PURRGO_GNSS_H */
