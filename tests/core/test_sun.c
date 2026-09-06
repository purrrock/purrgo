#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "purrgo/sun.h"

static int num_failures = 0;
#define EXPECT_TRUE(cond) if (!(cond)) { printf("FAIL: %s at %d\n", #cond, __LINE__); num_failures++; }
#define EXPECT_FALSE(cond) if (cond) { printf("FAIL: %s at %d\n", #cond, __LINE__); num_failures++; }
#define EXPECT_EQ(exp, act) if ((exp) != (act)) { printf("FAIL: expected %d, got %d at %d\n", (int)(exp), (int)(act), __LINE__); num_failures++; }

void test_polar_division_by_zero() {
    printf("test_polar_division_by_zero\n");
    purrgo_sun_info_t info = {0};

    // Latitude 90 degrees (North Pole)
    // 90 * 10,000,000 = 900000000
    // Test on a summer day (June 21, 2023)
    purrgo_sun_calc(900000000, 0, 23, 6, 21, 12, 0, 0, &info);

    // In summer at North Pole, it should be polar day
    EXPECT_EQ(SUN_STATUS_POLAR_DAY, info.status);
    EXPECT_TRUE(info.is_daytime);
    EXPECT_EQ(-1, info.time_to_event_min);

    // Latitude -90 degrees (South Pole)
    // -90 * 10,000,000 = -900000000
    // Test on a summer day in North Hemisphere (June 21, 2023), so South Pole has Polar Night
    purrgo_sun_calc(-900000000, 0, 23, 6, 21, 12, 0, 0, &info);
    EXPECT_EQ(SUN_STATUS_POLAR_NIGHT, info.status);
    EXPECT_FALSE(info.is_daytime);
    EXPECT_EQ(-1, info.time_to_event_min);
}

void test_normal_day() {
    printf("test_normal_day\n");
    purrgo_sun_info_t info = {0};

    // Latitude 45 degrees, Longitude 0 degrees
    // Test on Equinox (March 21, 2023)
    purrgo_sun_calc(450000000, 0, 23, 3, 21, 12, 0, 0, &info);

    EXPECT_EQ(SUN_STATUS_NORMAL, info.status);
}

int main(void) {
    test_polar_division_by_zero();
    test_normal_day();

    if (num_failures > 0) {
        printf("FAILED %d tests.\n", num_failures);
        return 1;
    }
    printf("All purrgo_sun tests passed.\n");
    return 0;
}
