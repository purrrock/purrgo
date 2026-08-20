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

    // Test 11: Year rollover wrapping 00 to 99 explicitly as part of 2-digit 2000-2099 semantics
    utc.hours = 1; utc.minutes = 0; utc.day = 1; utc.month = 1; utc.year = 0; // 2000
    purrgo_time_apply_timezone(&utc, &local, -120); // -2 hours
    assert(local.hours == 23 && local.minutes == 0 && local.day == 31 && local.month == 12 && local.year == 99); // 2099 wrap around check

    // Test 12: Year rollover wrapping 99 to 00 explicitly as part of 2-digit 2000-2099 semantics
    utc.hours = 23; utc.minutes = 0; utc.day = 31; utc.month = 12; utc.year = 99; // 2099
    purrgo_time_apply_timezone(&utc, &local, 120); // +2 hours
    assert(local.hours == 1 && local.minutes == 0 && local.day == 1 && local.month == 1 && local.year == 0); // 2000 wrap around check

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

static void test_gnss_adapter_nmea(void) {
    purrgo_gnss_solution_t sol;
    memset(&sol, 0, sizeof(sol));

    // Test RMC valid
    const char *rmc_valid = "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A\n";
    purrgo_gnss_process_nmea(rmc_valid, &sol);
    assert(sol.valid == true);
    assert(sol.hours == 12 && sol.minutes == 35 && sol.seconds == 19);
    assert(sol.day == 23 && sol.month == 3 && sol.year == 94);
    assert(sol.lat_1e7 == 481173000); // 48 + 07.038/60 -> 48.1173
    assert(sol.lon_1e7 == 115166666); // 11 + 31.000/60 -> 11.5166666 (integer truncated)
    assert(sol.speed_knots == 2240); // 22.4 * 100
    assert(sol.course_deg_100 == 8440); // 84.4 * 100

    // Test GGA valid
    const char *gga_valid = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\n";
    purrgo_gnss_process_nmea(gga_valid, &sol);
    assert(sol.satellites_tracked == 8);
    assert(sol.alt_m == 545);
    assert(sol.fix_quality == 1);
    assert(sol.hdop_100 == 90);

    // Test RMC invalid (keeps existing data valid except for the valid flag itself)
    // 0A is the valid checksum for this string
    const char *rmc_invalid = "$GPRMC,123519,V,4807.038,N,01131.000,E,,,230394,,*0A\n";
    purrgo_gnss_process_nmea(rmc_invalid, &sol);
    assert(sol.valid == false); // Flag becomes invalid
    assert(sol.alt_m == 545); // Unrelated fields preserved

    // Test GSA
    const char *gsa = "$GPGSA,A,3,04,05,,09,12,,,24,,,,,2.5,1.3,2.1*39\n";
    purrgo_gnss_process_nmea(gsa, &sol);
    assert(sol.fix_type == 3);
    assert(sol.pdop_100 == 250);
    assert(sol.hdop_100 == 130);
    assert(sol.vdop_100 == 210);

    // Negative coordinates and subzero fractional parts
    const char *rmc_neg = "$GPRMC,123519,A,4807.038,S,01131.000,W,022.4,084.4,230394,003.1,W*65\n";
    purrgo_gnss_process_nmea(rmc_neg, &sol);
    assert(sol.lat_1e7 == -481173000);
    assert(sol.lon_1e7 == -115166666);

    // Test Invalid Checksum
    // The previous RMC negative sentence has checksum *65. Let's make it *66.
    // Ensure that it is rejected and existing state is unmutated.
    const char *rmc_bad_checksum = "$GPRMC,123519,A,5000.000,N,01000.000,E,000.0,000.0,230394,000.0,E*FF\n";
    purrgo_gnss_process_nmea(rmc_bad_checksum, &sol);
    // The previous coordinates should still be there.
    assert(sol.lat_1e7 == -481173000);
    assert(sol.lon_1e7 == -115166666);
}

int main(void)
{
    purrgo_gnss_parser_t parser;
    purrgo_gnss_parser_init(&parser);
    assert(parser.length == 0U);

    test_timezone();
    test_gnss_parser_framing();
    test_gnss_adapter_nmea();

    return 0;
}