#!/bin/sh
# Сборка и запуск host-тестов (ядро + фейковая ФС). Запускать из корня репозитория.
set -e
OUT=${OUT:-/tmp/purrgo_host_test}
SRC=$(ls src/core/*.c src/core/*/*.c | grep -v gnss_mock.c)
gcc -std=c11 -g -fsanitize=address,undefined -Wall -Wextra -Wno-unused-parameter \
    -Iinclude -Isrc/core -Isrc/core/ui -Ithird_party/minmea -Itests/host \
    $SRC third_party/minmea/minmea.c \
    src/platform/pc/display_hal.c src/platform/pc/purrgo_logger.c \
    tests/host/fake_fs.c tests/host/test_refact_critical.c -o "$OUT"
"$OUT"
