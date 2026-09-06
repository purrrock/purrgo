#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "purrgo/track_logger.h"
#include "purrgo/config.h"
#include "purrgo/fs_hal.h"
#include "purrgo/logger.h"

// Provide mock file structure matching what track_logger might expect
struct purrgo_file_s {
    int dummy;
};

static struct purrgo_file_s mock_file;
static bool mock_file_opened = false;
static char last_filename[256];

purrgo_file_t* purrgo_fs_open(const char* path, uint32_t mode) {
    mock_file_opened = true;
    snprintf(last_filename, sizeof(last_filename), "%s", path);
    return &mock_file;
}

uint32_t purrgo_fs_read(purrgo_file_t* file, uint8_t* buffer, uint32_t size) { return 0; }
uint32_t purrgo_fs_write(purrgo_file_t* file, const uint8_t* buffer, uint32_t size) { return size; }
bool purrgo_fs_seek(purrgo_file_t* file, uint32_t offset) { return true; }
void purrgo_fs_close(purrgo_file_t* file) { mock_file_opened = false; }
void purrgo_fs_sync(purrgo_file_t* file) {}

void purrgo_logger_log(const char* level, const char* file, int line, const char* format, ...) {}

static int num_failures = 0;
#define EXPECT_TRUE(cond) if (!(cond)) { printf("FAIL: %s at %d\n", #cond, __LINE__); num_failures++; }
#define EXPECT_FALSE(cond) if (cond) { printf("FAIL: %s at %d\n", #cond, __LINE__); num_failures++; }
#define EXPECT_STREQ(exp, act) if (strcmp(exp, act) != 0) { printf("FAIL: expected %s, got %s at %d\n", exp, act, __LINE__); num_failures++; }

void test_timezone_positive() {
    printf("test_timezone_positive\n");
    app_config.tz_offset_minutes = 180; // UTC+3
    purrgo_gnss_solution_t fix = {0};
    fix.valid = true;
    fix.year = 23; fix.month = 10; fix.day = 5;
    fix.hours = 21; fix.minutes = 30; fix.seconds = 0; // 21:30 UTC -> 00:30 UTC+3 next day (Oct 6)

    purrgo_logger_stop();
    EXPECT_TRUE(purrgo_logger_start(&fix));

    // Check filename contains 231006-003000
    EXPECT_TRUE(strstr(last_filename, "231006-003000") != NULL);
}

void test_timezone_negative() {
    printf("test_timezone_negative\n");
    app_config.tz_offset_minutes = -240; // UTC-4
    purrgo_gnss_solution_t fix = {0};
    fix.valid = true;
    fix.year = 23; fix.month = 10; fix.day = 5;
    fix.hours = 2; fix.minutes = 30; fix.seconds = 0; // 02:30 UTC -> 22:30 UTC-4 prev day (Oct 4)

    purrgo_logger_stop();
    EXPECT_TRUE(purrgo_logger_start(&fix));

    // Check filename contains 231004-223000
    EXPECT_TRUE(strstr(last_filename, "231004-223000") != NULL);
}

void test_timezone_negative_underflow() {
    printf("test_timezone_negative_underflow\n");
    app_config.tz_offset_minutes = -180; // UTC-3
    purrgo_gnss_solution_t fix = {0};
    fix.valid = true;
    // VERY beginning of epoch
    fix.year = 0; fix.month = 1; fix.day = 1;
    fix.hours = 1; fix.minutes = 0; fix.seconds = 0; // 01:00 UTC -> 22:00 UTC-3 prev day (Underflows to 0)

    purrgo_logger_stop();
    // It should now reject the start since we can't represent negative epochs.
    EXPECT_FALSE(purrgo_logger_start(&fix));
}

int main(void) {
    test_timezone_positive();
    test_timezone_negative();
    test_timezone_negative_underflow();

    if (num_failures > 0) {
        printf("FAILED %d tests.\n", num_failures);
        return 1;
    }
    printf("All logger tests passed.\n");
    return 0;
}
