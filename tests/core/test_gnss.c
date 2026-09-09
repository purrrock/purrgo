// file: tests/core/test_gnss.c
#include <assert.h>
#include "purrgo/gnss.h"
#include "purrgo/app_fsm.h"
#include "purrgo/purrgo_time.h" // Добавлен инклуд нового модуля времени
#include "purrgo/gnss_types.h"
#include "purrgo/gnss_adapter.h"
#include <string.h>

static void test_timezone(void) {
    purrgo_gnss_solution_t utc;
    purrgo_gnss_solution_t local;

    // Test 1: No timezone offset
    utc.valid = true;
    utc.hours = 12; utc.minutes = 34; utc.seconds = 56;
    utc.day = 15; utc.month = 5; utc.year = 23;
    purrgo_time_apply_timezone(&utc, &local, 0);
    assert(local.hours == 12 && local.minutes == 34 && local.day == 15 && local.month == 5 && local.year == 23);
    assert(utc.hours == 12 && utc.day == 15); // Ensure UTC is unmutated

    // Test 2: Positive offset without midnight crossing
    utc.hours = 10; utc.minutes = 0; utc.day = 15; utc.month = 5; utc.year = 23;
    purrgo_time_apply_timezone(&utc, &local, 120); // +2 hours
    assert(local.hours == 12 && local.minutes == 0 && local.day == 15 && local.month == 5);
    assert(utc.hours == 10 && utc.day == 15);

    // Test 3: Positive offset crossing midnight
    utc.hours = 23; utc.minutes = 0; utc.day = 15; utc.month = 5; utc.year = 23;
    purrgo_time_apply_timezone(&utc, &local, 120); // +2 hours
    assert(local.hours == 1 && local.minutes == 0 && local.day == 16 && local.month == 5);
    assert(utc.hours == 23 && utc.day == 15);

    // Test 4: Negative offset crossing midnight
    utc.hours = 1; utc.minutes = 0; utc.day = 15; utc.month = 5; utc.year = 23;
    purrgo_time_apply_timezone(&utc, &local, -120); // -2 hours
    assert(local.hours == 23 && local.minutes == 0 && local.day == 14 && local.month == 5);

    // Test 5: End of a 31-day month
    utc.hours = 23; utc.minutes = 0; utc.day = 31; utc.month = 1; utc.year = 23;
    purrgo_time_apply_timezone(&utc, &local, 120); // +2 hours
    assert(local.hours == 1 && local.minutes == 0 && local.day == 1 && local.month == 2);

    // Test 6: End of a 30-day month
    utc.hours = 23; utc.minutes = 0; utc.day = 30; utc.month = 4; utc.year = 23;
    purrgo_time_apply_timezone(&utc, &local, 120); // +2 hours
    assert(local.hours == 1 && local.minutes == 0 && local.day == 1 && local.month == 5);

    // Test 7: February in a non-leap year
    utc.hours = 23; utc.minutes = 0; utc.day = 28; utc.month = 2; utc.year = 23;
    purrgo_time_apply_timezone(&utc, &local, 120); // +2 hours
    assert(local.hours == 1 && local.minutes == 0 && local.day == 1 && local.month == 3);

    // Test 8: February 29 in a leap year
    utc.hours = 23; utc.minutes = 0; utc.day = 28; utc.month = 2; utc.year = 24;
    purrgo_time_apply_timezone(&utc, &local, 120); // +2 hours
    assert(local.hours == 1 && local.minutes == 0 && local.day == 29 && local.month == 2);

    utc.hours = 23; utc.minutes = 0; utc.day = 29; utc.month = 2; utc.year = 24;
    purrgo_time_apply_timezone(&utc, &local, 120); // +2 hours
    assert(local.hours == 1 && local.minutes == 0 && local.day == 1 && local.month == 3);

    // Test 9: December 31 -> January 1
    utc.hours = 23; utc.minutes = 0; utc.day = 31; utc.month = 12; utc.year = 23;
    purrgo_time_apply_timezone(&utc, &local, 120); // +2 hours
    assert(local.hours == 1 && local.minutes == 0 && local.day == 1 && local.month == 1 && local.year == 24);

    // Test 10: January 1 with a negative timezone offset -> previous year
    utc.hours = 1; utc.minutes = 0; utc.day = 1; utc.month = 1; utc.year = 24;
    purrgo_time_apply_timezone(&utc, &local, -120); // -2 hours
    assert(local.hours == 23 && local.minutes == 0 && local.day == 31 && local.month == 12 && local.year == 23);

    // Test 11: Year rollover going backward from 2000 fails
    utc.hours = 1; utc.minutes = 0; utc.day = 1; utc.month = 1; utc.year = 0; // 2000
    assert(purrgo_time_apply_timezone(&utc, &local, -120) == false); // -2 hours out of bounds

    // Test 12: Year rollover going forward from 2099 fails
    utc.hours = 23; utc.minutes = 0; utc.day = 31; utc.month = 12; utc.year = 99; // 2099
    assert(purrgo_time_apply_timezone(&utc, &local, 120) == false); // +2 hours out of bounds

    // Test 13: Minimum configured timezone offset (-720)
    utc.hours = 1; utc.minutes = 0; utc.day = 15; utc.month = 5; utc.year = 23;
    purrgo_time_apply_timezone(&utc, &local, -720); // -12 hours
    assert(local.hours == 13 && local.minutes == 0 && local.day == 14 && local.month == 5);

    // Test 14: Maximum configured timezone offset (+840)
    utc.hours = 23; utc.minutes = 0; utc.day = 15; utc.month = 5; utc.year = 23;
    purrgo_time_apply_timezone(&utc, &local, 840); // +14 hours
    assert(local.hours == 13 && local.minutes == 0 && local.day == 16 && local.month == 5);
}

static void test_gnss_parser_framing(void) {
    purrgo_gnss_parser_t parser;
    purrgo_gnss_parser_init(&parser);

    // One byte at a time (LF termination)
    assert(purrgo_gnss_parser_feed(&parser, '$') == false);
    assert(purrgo_gnss_parser_feed(&parser, 'G') == false);
    assert(purrgo_gnss_parser_feed(&parser, '\n') == true);
    assert(strcmp(parser.line, "$G") == 0);
    assert(parser.length == 2);

    purrgo_gnss_parser_init(&parser);

    // Test CRLF termination
    assert(purrgo_gnss_parser_feed(&parser, '$') == false);
    assert(purrgo_gnss_parser_feed(&parser, 'G') == false);
    assert(purrgo_gnss_parser_feed(&parser, '\r') == false);
    assert(purrgo_gnss_parser_feed(&parser, '\n') == true);
    assert(strcmp(parser.line, "$G") == 0); // CR should be ignored, not inserted
    assert(parser.length == 2);

    // Prepare for multiple sentences
    purrgo_gnss_parser_init(&parser);

    // Multiple sentences check
    assert(purrgo_gnss_parser_feed(&parser, 'A') == false);
    assert(purrgo_gnss_parser_feed(&parser, '\r') == false);
    assert(purrgo_gnss_parser_feed(&parser, '\n') == true);
    assert(strcmp(parser.line, "A") == 0);

    purrgo_gnss_parser_init(&parser);
    assert(purrgo_gnss_parser_feed(&parser, 'B') == false);
    assert(purrgo_gnss_parser_feed(&parser, '\n') == true);
    assert(strcmp(parser.line, "B") == 0);

    purrgo_gnss_parser_init(&parser);

    // Boundary Test 1: Exactly one byte below capacity (126 bytes + newline)
    for (int i = 0; i < 126; i++) {
        assert(purrgo_gnss_parser_feed(&parser, 'X') == false);
    }
    assert(purrgo_gnss_parser_feed(&parser, '\n') == true);
    assert(parser.length == 126);

    purrgo_gnss_parser_init(&parser);

    // Boundary Test 2: Exactly at capacity boundary (127 bytes + newline)
    // The parser buffer is size 128. It stores the chars and a null terminator.
    // Max length before overflow is 127.
    for (int i = 0; i < 127; i++) {
        assert(purrgo_gnss_parser_feed(&parser, 'Y') == false);
    }
    assert(purrgo_gnss_parser_feed(&parser, '\n') == true);
    assert(parser.length == 127);

    purrgo_gnss_parser_init(&parser);

    // Boundary Test 3: Overlong sentence (128 chars). Should trigger overflow reset.
    for (int i = 0; i < 127; i++) {
        assert(purrgo_gnss_parser_feed(&parser, 'Z') == false);
    }
    // 128th char causes an overflow
    assert(purrgo_gnss_parser_feed(&parser, 'A') == false);
    assert(parser.length == 0);

    // Ensure parser recovers and accepts a subsequent valid sentence after an overflow
    assert(purrgo_gnss_parser_feed(&parser, '$') == false);
    assert(purrgo_gnss_parser_feed(&parser, 'O') == false);
    assert(purrgo_gnss_parser_feed(&parser, 'K') == false);
    assert(purrgo_gnss_parser_feed(&parser, '\n') == true);
    assert(strcmp(parser.line, "$OK") == 0);
    assert(parser.length == 3);
}

int main(void)
{
    purrgo_gnss_parser_t parser;
    purrgo_gnss_parser_init(&parser);
    assert(parser.length == 0U);

    test_timezone();
    test_gnss_parser_framing();

    return 0;
}
