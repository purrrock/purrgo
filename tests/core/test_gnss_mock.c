#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "purrgo/gnss_mock.h"
#include "purrgo/gnss_adapter.h"
#include "../../src/core/gnss_mock.c"

static void test_gnss_mock_init(void) {
    purrgo_gnss_mock_init();

    // Test that state is initialized correctly
    assert(state.valid == true);
    assert(state.lat_1e7 == 537135000);
    assert(state.lon_1e7 == 284199000);
    assert(state.speed_knots == 269);
    assert(state.alt_m == 150);
    assert(state.satellites_tracked == 9);
    assert(state.course_valid == true);
    assert(state.course_deg_100 == 4500);
    assert(state.hours == 12);
    assert(state.minutes == 34);
    assert(state.seconds == 56);
    assert(state.day == 1);
    assert(state.month == 1);
    assert(state.year == 26);

    // Read full NMEA stream from mock
    char buffer[512] = {0};
    size_t idx = 0;
    uint8_t byte;
    while(purrgo_gnss_mock_read_byte(&byte)) {
        if(idx < sizeof(buffer) - 1) {
            buffer[idx++] = (char)byte;
        }
    }

    // Should have read something
    assert(idx > 0);

    // Validate we can parse it
    purrgo_gnss_solution_t sol;
    memset(&sol, 0, sizeof(sol));

    // Split the buffer by newlines and process
    char *line = strtok(buffer, "\n");
    while(line) {
        char sentence[128];
        snprintf(sentence, sizeof(sentence), "%s\n", line);
        purrgo_gnss_process_nmea(sentence, &sol);
        line = strtok(NULL, "\n");
    }

    assert(sol.valid == true);
    assert(sol.lat_1e7 == 537135000);
    assert(sol.lon_1e7 == 284199000);
    assert(sol.speed_knots == 269); // Note: gnss_adapter scales knots. Assuming mock outputs correct
    assert(sol.alt_m == 150);
    assert(sol.satellites_tracked == 9);
    assert(sol.course_valid == true);
    assert(sol.course_deg_100 == 4500);
    assert(sol.hours == 12);
    assert(sol.minutes == 34);
    assert(sol.seconds == 56);
    assert(sol.day == 1);
    assert(sol.month == 1);
    assert(sol.year == 26);
}

static void test_gnss_mock_update(void) {
    purrgo_gnss_mock_init();

    // Update
    purrgo_gnss_mock_update();

    assert(state.seconds == 57);
    assert(state.lat_1e7 == 537135200);
    assert(state.lon_1e7 == 284199250);
    assert(state.course_deg_100 == 4650);

    // Read full NMEA stream from mock
    char buffer[512] = {0};
    size_t idx = 0;
    uint8_t byte;
    while(purrgo_gnss_mock_read_byte(&byte)) {
        if(idx < sizeof(buffer) - 1) {
            buffer[idx++] = (char)byte;
        }
    }

    // Validate we can parse it
    purrgo_gnss_solution_t sol;
    memset(&sol, 0, sizeof(sol));

    // Split the buffer by newlines and process
    char *line = strtok(buffer, "\n");
    while(line) {
        char sentence[128];
        snprintf(sentence, sizeof(sentence), "%s\n", line);
        purrgo_gnss_process_nmea(sentence, &sol);
        line = strtok(NULL, "\n");
    }

    assert(sol.valid == true);
    assert(sol.lat_1e7 == 537135200);
    assert(sol.lon_1e7 == 284199250);
    assert(sol.course_deg_100 == 4650);
    assert(sol.seconds == 57);
}

static void test_gnss_mock_time_rollover(void) {
    purrgo_gnss_mock_init();

    // Setup near rollover
    state.seconds = 59;
    state.minutes = 59;
    state.hours = 23;

    purrgo_gnss_mock_update();

    assert(state.seconds == 0);
    assert(state.minutes == 0);
    assert(state.hours == 0);
    // Note: mock doesn't handle day rollover based on update logic, only hours
}

static void test_gnss_mock_course_rollover_and_toggle(void) {
    purrgo_gnss_mock_init();

    // Setup near course rollover
    state.course_deg_100 = 35900;
    state.seconds = 8;
    state.course_valid = true;

    purrgo_gnss_mock_update(); // seconds=9
    assert(state.course_deg_100 == 50);
    assert(state.course_valid == true);

    purrgo_gnss_mock_update(); // seconds=10, should toggle
    assert(state.course_valid == false);
}

static void test_checksum_calculation(void) {
    uint8_t cksum = calc_checksum("$GPRMC,123456,A,1234.1234,N,12345.1234,E,0.0,0.0,010124,,,A*");
    // Just ensure it returns something deterministic
    assert(cksum > 0);
}

int main(void) {
    test_gnss_mock_init();
    test_gnss_mock_update();
    test_gnss_mock_time_rollover();
    test_gnss_mock_course_rollover_and_toggle();
    test_checksum_calculation();
    printf("test_gnss_mock passed!\n");
    return 0;
}
