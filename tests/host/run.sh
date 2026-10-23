#!/bin/sh
# Сборка и запуск host-тестов (ядро + фейковая ФС) с ASan/UBSan.
# Запускать из корня репозитория. Каждый tests/host/test_*.c - отдельный бинарник.
# Альтернатива: cmake --preset linux-gcc-test && ctest --preset linux-gcc-test
set -e
OUT=${OUT:-/tmp/purrgo_host_tests}
CC=${CC:-gcc}
mkdir -p "$OUT"
SRC=$(ls src/core/*.c src/core/*/*.c | grep -v gnss_mock.c)
rc=0
for t in tests/host/test_*.c; do
    name=$(basename "$t" .c)
    $CC -std=c11 -D_DEFAULT_SOURCE -g -fsanitize=address,undefined -fno-sanitize-recover=undefined \
        -Wall -Wextra -Wno-unused-parameter \
        -DTEST_DATA_DIR='"tests/host/data"' \
        -Iinclude -Isrc/core -Isrc/core/ui -Ithird_party/minmea -Itests/host \
        $SRC third_party/minmea/minmea.c \
        src/platform/pc/display_hal.c src/platform/pc/purrgo_logger.c \
        tests/host/fake_fs.c tests/host/fake_power.c "$t" -o "$OUT/$name"
    echo "== $name"
    "$OUT/$name" 2>&1 | grep -E "^(FAIL|XPASS)|checks," || true
    "$OUT/$name" >/dev/null 2>&1 || rc=1
done
exit $rc
