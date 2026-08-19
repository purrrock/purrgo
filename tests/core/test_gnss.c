#include <assert.h>
#include "purrgo/gnss.h"
#include "purrgo/app_fsm.h"
#include "purrgo/gnss_types.h"
#include "purrgo/gnss_adapter.h"
#include <string.h>

static void test_timezone(void) {
    purrgo_gnss_solution_t fix;

    // Test 1: No timezone offset
    fix.valid = true;
    fix.hours = 12; fix.minutes = 34; fix.seconds = 56;
    fix.day = 15; fix.month = 5; fix.year = 23;
    purrgo_app_apply_timezone(&fix, 0);
    assert(fix.hours == 12 && fix.minutes == 34 && fix.day == 15 && fix.month == 5 && fix.year == 23);

    // Test 2: Positive offset without midnight crossing
    fix.hours = 10; fix.minutes = 0; fix.day = 15; fix.month = 5; fix.year = 23;
    purrgo_app_apply_timezone(&fix, 120); // +2 hours
    assert(fix.hours == 12 && fix.minutes == 0 && fix.day == 15 && fix.month == 5);

    // Test 3: Positive offset crossing midnight
    fix.hours = 23; fix.minutes = 0; fix.day = 15; fix.month = 5; fix.year = 23;
    purrgo_app_apply_timezone(&fix, 120); // +2 hours
    assert(fix.hours == 1 && fix.minutes == 0 && fix.day == 16 && fix.month == 5);

    // Test 4: Negative offset crossing midnight
    fix.hours = 1; fix.minutes = 0; fix.day = 15; fix.month = 5; fix.year = 23;
    purrgo_app_apply_timezone(&fix, -120); // -2 hours
    assert(fix.hours == 23 && fix.minutes == 0 && fix.day == 14 && fix.month == 5);

    // Test 5: End of a 31-day month
    fix.hours = 23; fix.minutes = 0; fix.day = 31; fix.month = 1; fix.year = 23;
    purrgo_app_apply_timezone(&fix, 120); // +2 hours
    assert(fix.hours == 1 && fix.minutes == 0 && fix.day == 1 && fix.month == 2);

    // Test 6: End of a 30-day month
    fix.hours = 23; fix.minutes = 0; fix.day = 30; fix.month = 4; fix.year = 23;
    purrgo_app_apply_timezone(&fix, 120); // +2 hours
    assert(fix.hours == 1 && fix.minutes == 0 && fix.day == 1 && fix.month == 5);

    // Test 7: February in a non-leap year
    fix.hours = 23; fix.minutes = 0; fix.day = 28; fix.month = 2; fix.year = 23;
    purrgo_app_apply_timezone(&fix, 120); // +2 hours
    assert(fix.hours == 1 && fix.minutes == 0 && fix.day == 1 && fix.month == 3);

    // Test 8: February 29 in a leap year
    fix.hours = 23; fix.minutes = 0; fix.day = 28; fix.month = 2; fix.year = 24;
    purrgo_app_apply_timezone(&fix, 120); // +2 hours
    assert(fix.hours == 1 && fix.minutes == 0 && fix.day == 29 && fix.month == 2);

    fix.hours = 23; fix.minutes = 0; fix.day = 29; fix.month = 2; fix.year = 24;
    purrgo_app_apply_timezone(&fix, 120); // +2 hours
    assert(fix.hours == 1 && fix.minutes == 0 && fix.day == 1 && fix.month == 3);

    // Test 9: December 31 -> January 1
    fix.hours = 23; fix.minutes = 0; fix.day = 31; fix.month = 12; fix.year = 23;
    purrgo_app_apply_timezone(&fix, 120); // +2 hours
    assert(fix.hours == 1 && fix.minutes == 0 && fix.day == 1 && fix.month == 1 && fix.year == 24);

    // Test 10: January 1 with a negative timezone offset -> previous year
    fix.hours = 1; fix.minutes = 0; fix.day = 1; fix.month = 1; fix.year = 24;
    purrgo_app_apply_timezone(&fix, -120); // -2 hours
    assert(fix.hours == 23 && fix.minutes == 0 && fix.day == 31 && fix.month == 12 && fix.year == 23);

    // Test 11: Minimum configured timezone offset (-720)
    fix.hours = 1; fix.minutes = 0; fix.day = 15; fix.month = 5; fix.year = 23;
    purrgo_app_apply_timezone(&fix, -720); // -12 hours
    assert(fix.hours == 13 && fix.minutes == 0 && fix.day == 14 && fix.month == 5);

    // Test 12: Maximum configured timezone offset (+840)
    fix.hours = 23; fix.minutes = 0; fix.day = 15; fix.month = 5; fix.year = 23;
    purrgo_app_apply_timezone(&fix, 840); // +14 hours
    assert(fix.hours == 13 && fix.minutes == 0 && fix.day == 16 && fix.month == 5);
}

static void test_gnss_parser_framing(void) {
    purrgo_gnss_parser_t parser;
    purrgo_gnss_parser_init(&parser);

    // One byte at a time
    assert(purrgo_gnss_parser_feed(&parser, '$') == false);
    assert(purrgo_gnss_parser_feed(&parser, 'G') == false);
    assert(purrgo_gnss_parser_feed(&parser, '\n') == true);
    assert(strcmp(parser.line, "$G") == 0);
    assert(parser.length == 2);

    purrgo_gnss_parser_init(&parser);

    // Test sentence overlong/malformed input exceeding buffer length
    // Line buffer size is 128
    for(int i = 0; i < 127; i++) {
        purrgo_gnss_parser_feed(&parser, 'A');
    }
    // Next byte should overflow and reset length
    assert(purrgo_gnss_parser_feed(&parser, 'B') == false);
    assert(parser.length == 0);
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
    assert(sol.satellites == 8);
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
