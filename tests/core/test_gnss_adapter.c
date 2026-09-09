// file: tests/core/test_gnss_adapter.c
#include <assert.h>
#include <string.h>
#include "purrgo/gnss_adapter.h"

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
    assert(sol.course_valid == true);

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

static void test_partial_gnss_sentences(void) {
    purrgo_gnss_solution_t sol;
    memset(&sol, 0, sizeof(sol));

    // Process only GGA. Should not set valid to true, but should update altitude, satellites, etc.
    const char* gga = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    purrgo_gnss_process_nmea(gga, &sol);

    // Valid should still be false because RMC wasn't processed
    assert(!sol.valid);
    // Altitude and sats should be updated
    assert(sol.alt_m == 545);
    assert(sol.satellites_tracked == 8);
    assert(sol.fix_quality == 1);

    // Process RMC, this should set valid
    const char* rmc = "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A\r\n";
    purrgo_gnss_process_nmea(rmc, &sol);

    // Now it should be valid
    assert(sol.valid);
    assert(sol.lat_1e7 == 481173000);
    assert(sol.lon_1e7 == 115166666);
    assert(sol.speed_knots == 2240);
    assert(sol.course_deg_100 == 8440);
    // Preserves altitude from GGA
    assert(sol.alt_m == 545);

    // Process GSA, check fix_type
    const char* gsa = "$GPGSA,A,3,04,05,,09,12,,,24,,,,,2.5,1.3,2.1*39\r\n";
    purrgo_gnss_process_nmea(gsa, &sol);
    assert(sol.fix_type == 3);
    assert(sol.pdop_100 == 250);
    assert(sol.hdop_100 == 130);
    assert(sol.vdop_100 == 210);
}

static void test_gnss_adapter_empty_fields(void) {
    purrgo_gnss_solution_t sol;
    memset(&sol, 0, sizeof(sol));

    // Test RMC valid but with missing course and speed
    const char *rmc_missing_speed = "$GPRMC,123519,A,4807.038,N,01131.000,E,,,230394,003.1,W*66\n";
    purrgo_gnss_process_nmea(rmc_missing_speed, &sol);
    assert(sol.valid == true);
    assert(sol.speed_knots == 0);
    assert(sol.course_valid == false);
    assert(sol.course_deg_100 == 0);

    // Test GGA missing altitude and hdop
    const char *gga_missing = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,,,,,,,*5B\n";
    memset(&sol, 0, sizeof(sol));
    purrgo_gnss_process_nmea(gga_missing, &sol);
    assert(sol.alt_m == 0);
    assert(sol.hdop_100 == 0);

    // Test GSA missing all dops
    const char *gsa_missing = "$GPGSA,A,3,04,05,,09,12,,,24,,,,,,,,*3D\n";
    memset(&sol, 0, sizeof(sol));
    purrgo_gnss_process_nmea(gsa_missing, &sol);
    assert(sol.fix_type == 3);
    assert(sol.pdop_100 == 0);
    assert(sol.hdop_100 == 0);
    assert(sol.vdop_100 == 0);
}

int main(void) {
    test_gnss_adapter_nmea();
    test_partial_gnss_sentences();
    test_gnss_adapter_empty_fields();
    return 0;
}
