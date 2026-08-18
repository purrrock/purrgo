#include "purrgo/gnss.h"

void purrgo_gnss_parser_init(purrgo_gnss_parser_t *parser)
{
    parser->length = 0U;
}

bool purrgo_gnss_parser_feed(purrgo_gnss_parser_t *parser, uint8_t byte)
{
    if (byte == '\n') {
        return true;
    }

    if (parser->length + 1U >= sizeof(parser->line)) {
        /* Reset on overflow; the caller can count malformed sentences. */
        parser->length = 0U;
        return false;
    }

    parser->line[parser->length++] = (char)byte;
    parser->line[parser->length] = '\0';
    return false;
}
