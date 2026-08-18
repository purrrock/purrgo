#include <assert.h>
#include "purrgo/gnss.h"

int main(void)
{
    purrgo_gnss_parser_t parser;
    purrgo_gnss_parser_init(&parser);
    assert(parser.length == 0U);
    return 0;
}
