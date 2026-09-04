#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "purrgo/sun.h"

static int num_failures = 0;
#define EXPECT_TRUE(cond) if (!(cond)) { printf("FAIL: %s at %d\n", #cond, __LINE__); num_failures++; }
#define EXPECT_FALSE(cond) if (cond) { printf("FAIL: %s at %d\n", #cond, __LINE__); num_failures++; }
#define EXPECT_EQ(exp, act) if ((exp) != (act)) { printf("FAIL: expected %d, got %d at %d\n", (int)(exp), (int)(act), __LINE__); num_failures++; }

void test_normal_day() {
    printf("test_normal_day\n");
    purrgo_sun_info_t info = {0};
    // Moscow (~55.75°N, 37.61°E) on March 21 (Spring Equinox)
    // 55.75 * 10^7 = 557500000, 37.61 * 10^7 = 376100000
    purrgo_sun_calc(557500000, 376100000, 23, 3, 21, 12, 0, 180, &info);
    EXPECT_EQ(SUN_STATUS_NORMAL, info.status);

    // Check if it's daytime (should be around solar noon)
    EXPECT_TRUE(info.is_daytime);
}

void test_polar_day() {
    printf("test_polar_day\n");
    purrgo_sun_info_t info = {0};

    // Svalbard, Norway (~78.22°N) on June 21 (Summer Solstice, Northern Hemisphere)
    purrgo_sun_calc(782200000, 156000000, 23, 6, 21, 12, 0, 120, &info);
    EXPECT_EQ(SUN_STATUS_POLAR_DAY, info.status);
    EXPECT_TRUE(info.is_daytime);
    EXPECT_EQ(-1, info.time_to_event_min);

    // Antarctica (~-80.00°N) on December 21 (Summer Solstice, Southern Hemisphere)
    purrgo_sun_calc(-800000000, 0, 23, 12, 21, 12, 0, 0, &info);
    EXPECT_EQ(SUN_STATUS_POLAR_DAY, info.status);
    EXPECT_TRUE(info.is_daytime);
    EXPECT_EQ(-1, info.time_to_event_min);
}

void test_polar_night() {
    printf("test_polar_night\n");
    purrgo_sun_info_t info = {0};

    // Svalbard, Norway (~78.22°N) on December 21 (Winter Solstice, Northern Hemisphere)
    purrgo_sun_calc(782200000, 156000000, 23, 12, 21, 12, 0, 120, &info);
    EXPECT_EQ(SUN_STATUS_POLAR_NIGHT, info.status);
    EXPECT_FALSE(info.is_daytime);
    EXPECT_EQ(-1, info.time_to_event_min);

    // Antarctica (~-80.00°N) on June 21 (Winter Solstice, Southern Hemisphere)
    purrgo_sun_calc(-800000000, 0, 23, 6, 21, 12, 0, 0, &info);
    EXPECT_EQ(SUN_STATUS_POLAR_NIGHT, info.status);
    EXPECT_FALSE(info.is_daytime);
    EXPECT_EQ(-1, info.time_to_event_min);
}

int main(void) {
    test_normal_day();
    test_polar_day();
    test_polar_night();

    if (num_failures > 0) {
        printf("FAILED %d tests.\n", num_failures);
        return 1;
    }
    printf("All test_sun tests passed.\n");
    return 0;
}
