#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

// Mock GNSS API
#include "purrgo/app_fsm.h"
#include "purrgo/gnss_types.h"

static purrgo_gnss_solution_t mock_gnss = {0};

const purrgo_gnss_solution_t* purrgo_app_get_gnss_solution(void) {
    return &mock_gnss;
}

// Mock FATFS types to avoid pulling ST headers
#define __fatfs_H
typedef uint32_t DWORD;
typedef struct { int dummy; } FATFS;
typedef struct { int dummy; } FIL;
typedef struct { int dummy; } Diskio_drvTypeDef;
Diskio_drvTypeDef USER_Driver;
uint8_t FATFS_LinkDriver(void* drv, char* path) { return 0; }

// Include the C file to test it directly
#include "../../apps/stm32/PurrGO_STM32/FATFS/App/fatfs.c"

static int num_failures = 0;
#define EXPECT_EQ(exp, act) if ((exp) != (act)) { printf("FAIL: expected %u, got %u at %d\n", (uint32_t)(exp), (uint32_t)(act), __LINE__); num_failures++; }

void test_fallback() {
    mock_gnss.valid = false;
    mock_gnss.year = 0;
    DWORD t = get_fattime();
    DWORD exp = ((DWORD)(2026 - 1980) << 25) | ((DWORD)1 << 21) | ((DWORD)1 << 16) | ((DWORD)0 << 11) | ((DWORD)0 << 5) | ((DWORD)0 >> 1);
    EXPECT_EQ(exp, t);
}

void test_fallback_year_0() {
    mock_gnss.valid = true;
    mock_gnss.year = 0;
    DWORD t = get_fattime();
    DWORD exp = ((DWORD)(2026 - 1980) << 25) | ((DWORD)1 << 21) | ((DWORD)1 << 16) | ((DWORD)0 << 11) | ((DWORD)0 << 5) | ((DWORD)0 >> 1);
    EXPECT_EQ(exp, t);
}

void test_valid_gnss_time() {
    mock_gnss.valid = true;
    mock_gnss.year = 24; // 2024
    mock_gnss.month = 10;
    mock_gnss.day = 5;
    mock_gnss.hours = 14;
    mock_gnss.minutes = 30;
    mock_gnss.seconds = 16;
    DWORD t = get_fattime();

    DWORD exp = ((DWORD)(2024 - 1980) << 25)
              | ((DWORD)10 << 21)
              | ((DWORD)5 << 16)
              | ((DWORD)14 << 11)
              | ((DWORD)30 << 5)
              | ((DWORD)(16 / 2));
    EXPECT_EQ(exp, t);
}

int main() {
    test_fallback();
    test_fallback_year_0();
    test_valid_gnss_time();

    if (num_failures > 0) {
        printf("FAILED %d tests\n", num_failures);
        return 1;
    }
    printf("All fatfs time tests passed.\n");
    return 0;
}
