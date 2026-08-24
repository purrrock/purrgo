#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "purrgo/purrgo_time.h"

static int num_failures = 0;
#define EXPECT_TRUE(cond) if (!(cond)) { printf("FAIL: %s at %d\n", #cond, __LINE__); num_failures++; }
#define EXPECT_FALSE(cond) if (cond) { printf("FAIL: %s at %d\n", #cond, __LINE__); num_failures++; }
#define EXPECT_EQ(exp, act) if ((exp) != (act)) { printf("FAIL: expected %d, got %d at %d\n", (int)(exp), (int)(act), __LINE__); num_failures++; }

void test_leap_years() {
    printf("test_leap_years\n");
    // 2000 is a leap year (0 % 4 == 0)
    EXPECT_TRUE(purrgo_time_is_leap_year(0));
    // 2004 is a leap year
    EXPECT_TRUE(purrgo_time_is_leap_year(4));
    // 2096 is a leap year
    EXPECT_TRUE(purrgo_time_is_leap_year(96));
    // 2001 is not
    EXPECT_FALSE(purrgo_time_is_leap_year(1));
    // 2099 is not
    EXPECT_FALSE(purrgo_time_is_leap_year(99));
}

void test_month_lengths() {
    printf("test_month_lengths\n");
    // Leap year Feb
    EXPECT_EQ(29, purrgo_time_days_in_month(2, 0));
    EXPECT_EQ(29, purrgo_time_days_in_month(2, 4));

    // Non-leap year Feb
    EXPECT_EQ(28, purrgo_time_days_in_month(2, 1));
    EXPECT_EQ(28, purrgo_time_days_in_month(2, 99));

    // Jan, Mar, May, Jul, Aug, Oct, Dec have 31 days
    EXPECT_EQ(31, purrgo_time_days_in_month(1, 10));
    EXPECT_EQ(31, purrgo_time_days_in_month(3, 10));
    EXPECT_EQ(31, purrgo_time_days_in_month(12, 10));

    // Apr, Jun, Sep, Nov have 30 days
    EXPECT_EQ(30, purrgo_time_days_in_month(4, 10));
    EXPECT_EQ(30, purrgo_time_days_in_month(11, 10));
}

void test_datetime_to_epoch_validation() {
    printf("test_datetime_to_epoch_validation\n");
    uint32_t epoch = 0;
    // Valid date
    EXPECT_TRUE(purrgo_time_datetime_to_epoch(23, 10, 5, 12, 0, 0, &epoch));

    // Invalid year > 99
    EXPECT_FALSE(purrgo_time_datetime_to_epoch(100, 1, 1, 12, 0, 0, &epoch));

    // Invalid month
    EXPECT_FALSE(purrgo_time_datetime_to_epoch(23, 0, 1, 12, 0, 0, &epoch));
    EXPECT_FALSE(purrgo_time_datetime_to_epoch(23, 13, 1, 12, 0, 0, &epoch));

    // Invalid day
    EXPECT_FALSE(purrgo_time_datetime_to_epoch(23, 1, 0, 12, 0, 0, &epoch));
    EXPECT_FALSE(purrgo_time_datetime_to_epoch(23, 1, 32, 12, 0, 0, &epoch));

    // 31 Feb
    EXPECT_FALSE(purrgo_time_datetime_to_epoch(23, 2, 31, 12, 0, 0, &epoch));
    // 29 Feb in non-leap year
    EXPECT_FALSE(purrgo_time_datetime_to_epoch(23, 2, 29, 12, 0, 0, &epoch));
    // 29 Feb in leap year
    EXPECT_TRUE(purrgo_time_datetime_to_epoch(24, 2, 29, 12, 0, 0, &epoch));

    // Invalid time
    EXPECT_FALSE(purrgo_time_datetime_to_epoch(23, 10, 5, 24, 0, 0, &epoch));
    EXPECT_FALSE(purrgo_time_datetime_to_epoch(23, 10, 5, 12, 60, 0, &epoch));
    EXPECT_FALSE(purrgo_time_datetime_to_epoch(23, 10, 5, 12, 0, 60, &epoch));
}

void test_epoch_roundtrip() {
    printf("test_epoch_roundtrip\n");
    // Pick some interesting dates
    struct {
        uint8_t y, m, d, h, min, s;
    } dates[] = {
        {0, 1, 1, 0, 0, 0},     // 2000-01-01 00:00:00 (Epoch start)
        {0, 2, 29, 23, 59, 59}, // 2000-02-29 23:59:59 (Leap day end)
        {23, 12, 31, 23, 59, 59}, // 2023-12-31 23:59:59
        {99, 12, 31, 23, 59, 59}, // 2099-12-31 23:59:59
        {50, 6, 15, 12, 30, 45}, // Middle
    };

    for (int i = 0; i < 5; i++) {
        uint32_t epoch = 0;
        EXPECT_TRUE(purrgo_time_datetime_to_epoch(dates[i].y, dates[i].m, dates[i].d,
                                                  dates[i].h, dates[i].min, dates[i].s, &epoch));

        uint8_t y_out, m_out, d_out, h_out, min_out, s_out;
        EXPECT_TRUE(purrgo_time_epoch_to_datetime(epoch, &y_out, &m_out, &d_out, &h_out, &min_out, &s_out));

        EXPECT_EQ(dates[i].y, y_out);
        EXPECT_EQ(dates[i].m, m_out);
        EXPECT_EQ(dates[i].d, d_out);
        EXPECT_EQ(dates[i].h, h_out);
        EXPECT_EQ(dates[i].min, min_out);
        EXPECT_EQ(dates[i].s, s_out);
    }

    uint8_t y, m, d, h, min, s;
    // Epoch out of bounds tests
    EXPECT_FALSE(purrgo_time_epoch_to_datetime(3155760000UL, &y, &m, &d, &h, &min, &s));
    EXPECT_FALSE(purrgo_time_epoch_to_datetime(0xFFFFFFFF, &y, &m, &d, &h, &min, &s));
}

void test_timezone_application() {
    printf("test_timezone_application\n");
    purrgo_gnss_solution_t utc = {0};
    purrgo_gnss_solution_t local = {0};
    utc.valid = true;

    // Normal offset +3
    utc.year = 23; utc.month = 10; utc.day = 5;
    utc.hours = 12; utc.minutes = 0; utc.seconds = 0;
    EXPECT_TRUE(purrgo_time_apply_timezone(&utc, &local, 180));
    EXPECT_EQ(23, local.year); EXPECT_EQ(10, local.month); EXPECT_EQ(5, local.day);
    EXPECT_EQ(15, local.hours); EXPECT_EQ(0, local.minutes); EXPECT_EQ(0, local.seconds);

    // Cross midnight forward
    utc.year = 23; utc.month = 10; utc.day = 5;
    utc.hours = 23; utc.minutes = 0; utc.seconds = 0;
    EXPECT_TRUE(purrgo_time_apply_timezone(&utc, &local, 180));
    EXPECT_EQ(23, local.year); EXPECT_EQ(10, local.month); EXPECT_EQ(6, local.day);
    EXPECT_EQ(2, local.hours); EXPECT_EQ(0, local.minutes); EXPECT_EQ(0, local.seconds);

    // Cross midnight backward
    utc.year = 23; utc.month = 10; utc.day = 5;
    utc.hours = 1; utc.minutes = 0; utc.seconds = 0;
    EXPECT_TRUE(purrgo_time_apply_timezone(&utc, &local, -180));
    EXPECT_EQ(23, local.year); EXPECT_EQ(10, local.month); EXPECT_EQ(4, local.day);
    EXPECT_EQ(22, local.hours); EXPECT_EQ(0, local.minutes); EXPECT_EQ(0, local.seconds);

    // Cross month backward (May 1 -> Apr 30)
    utc.year = 23; utc.month = 5; utc.day = 1;
    utc.hours = 1; utc.minutes = 0; utc.seconds = 0;
    EXPECT_TRUE(purrgo_time_apply_timezone(&utc, &local, -180));
    EXPECT_EQ(23, local.year); EXPECT_EQ(4, local.month); EXPECT_EQ(30, local.day);
    EXPECT_EQ(22, local.hours); EXPECT_EQ(0, local.minutes); EXPECT_EQ(0, local.seconds);

    // Cross month forward (Feb 28 2023 -> Mar 1 2023)
    utc.year = 23; utc.month = 2; utc.day = 28;
    utc.hours = 23; utc.minutes = 0; utc.seconds = 0;
    EXPECT_TRUE(purrgo_time_apply_timezone(&utc, &local, 180));
    EXPECT_EQ(23, local.year); EXPECT_EQ(3, local.month); EXPECT_EQ(1, local.day);
    EXPECT_EQ(2, local.hours); EXPECT_EQ(0, local.minutes); EXPECT_EQ(0, local.seconds);

    // Cross month forward (Feb 29 2024 -> Mar 1 2024)
    utc.year = 24; utc.month = 2; utc.day = 29;
    utc.hours = 23; utc.minutes = 0; utc.seconds = 0;
    EXPECT_TRUE(purrgo_time_apply_timezone(&utc, &local, 180));
    EXPECT_EQ(24, local.year); EXPECT_EQ(3, local.month); EXPECT_EQ(1, local.day);
    EXPECT_EQ(2, local.hours); EXPECT_EQ(0, local.minutes); EXPECT_EQ(0, local.seconds);

    // Cross year forward
    utc.year = 23; utc.month = 12; utc.day = 31;
    utc.hours = 23; utc.minutes = 0; utc.seconds = 0;
    EXPECT_TRUE(purrgo_time_apply_timezone(&utc, &local, 180));
    EXPECT_EQ(24, local.year); EXPECT_EQ(1, local.month); EXPECT_EQ(1, local.day);
    EXPECT_EQ(2, local.hours); EXPECT_EQ(0, local.minutes); EXPECT_EQ(0, local.seconds);

    // Cross year backward
    utc.year = 23; utc.month = 1; utc.day = 1;
    utc.hours = 1; utc.minutes = 0; utc.seconds = 0;
    EXPECT_TRUE(purrgo_time_apply_timezone(&utc, &local, -180));
    EXPECT_EQ(22, local.year); EXPECT_EQ(12, local.month); EXPECT_EQ(31, local.day);
    EXPECT_EQ(22, local.hours); EXPECT_EQ(0, local.minutes); EXPECT_EQ(0, local.seconds);

    // Century underflow (2000 Jan 1 -> out of bounds)
    utc.year = 0; utc.month = 1; utc.day = 1;
    utc.hours = 1; utc.minutes = 0; utc.seconds = 0;
    EXPECT_FALSE(purrgo_time_apply_timezone(&utc, &local, -180));

    // Century overflow (2099 Dec 31 -> out of bounds)
    utc.year = 99; utc.month = 12; utc.day = 31;
    utc.hours = 23; utc.minutes = 0; utc.seconds = 0;
    EXPECT_FALSE(purrgo_time_apply_timezone(&utc, &local, 180));

    // Fractional hour timezone (UTC+5:45 Nepal)
    utc.year = 23; utc.month = 10; utc.day = 5;
    utc.hours = 12; utc.minutes = 30; utc.seconds = 15;
    EXPECT_TRUE(purrgo_time_apply_timezone(&utc, &local, 5 * 60 + 45)); // +345
    EXPECT_EQ(23, local.year); EXPECT_EQ(10, local.month); EXPECT_EQ(5, local.day);
    EXPECT_EQ(18, local.hours); EXPECT_EQ(15, local.minutes); EXPECT_EQ(15, local.seconds);
}

int main(void) {
    test_leap_years();
    test_month_lengths();
    test_datetime_to_epoch_validation();
    test_epoch_roundtrip();
    test_timezone_application();

    if (num_failures > 0) {
        printf("FAILED %d tests.\n", num_failures);
        return 1;
    }
    printf("All purrgo_time tests passed.\n");
    return 0;
}
