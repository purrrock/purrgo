#!/bin/bash
set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
cd "$DIR"

# Generate mock fs_hal_stm32.c
sed -e 's/#include "fatfs.h"//' \
    -e 's/#include "purrgo\/logger.h"//' \
    -e 's/#include "purrgo\/fs_hal.h"/#include "..\/..\/..\/..\/include\/purrgo\/fs_hal.h"/' \
    ../Core/Src/fs_hal_stm32.c > fs_hal_stm32_test.c

gcc -I../../../../include test_lru_cache.c -o test_lru_cache
./test_lru_cache

rm fs_hal_stm32_test.c test_lru_cache
