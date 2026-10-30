#!/bin/sh
# Сборка и запуск host-тестов (ядро + фейковая ФС) с ASan/UBSan.
# Запускать из корня репозитория. Каждый tests/host/test_*.c - отдельный бинарник.
# Дополнительные исходники и флаги отдельных тестов повторяют tests/host/CMakeLists.txt.
# Альтернатива: cmake --preset linux-gcc-test && ctest --preset linux-gcc-test
#
# Ошибка сборки одного теста не прерывает прогон: тест помечается BUILD-FAIL,
# код возврата ненулевой. Тесты, которым нужен сабмодуль third_party/eink_part_refresh,
# пропускаются (SKIP), если он не загружен.
CC=${CC:-gcc}
OUT=${OUT:-/tmp/purrgo_host_tests}
EPD=third_party/eink_part_refresh/epd4g
mkdir -p "$OUT"
rc=0

CFLAGS="-std=c11 -D_DEFAULT_SOURCE -g -fsanitize=address,undefined -fno-sanitize-recover=undefined \
    -Wall -Wextra -Wno-unused-parameter -DTEST_DATA_DIR=\"tests/host/data\" \
    -Iinclude -Isrc/core -Isrc/core/ui -Ithird_party/minmea -Itests/host"
COMMON="src/platform/pc/display_hal.c src/platform/pc/purrgo_logger.c \
    tests/host/fake_fs.c tests/host/fake_power.c"

# Ядро собирается в статическую библиотеку, как purrgo_core в CMake: из неё берутся
# только нужные тесту объектные файлы (тесты сами задают purrgo_system_time_ms и т.п.).
rm -rf "$OUT/core" && mkdir -p "$OUT/core"
for f in $(ls src/core/*.c src/core/*/*.c third_party/minmea/minmea.c); do
    o="$OUT/core/$(echo "$f" | tr / _).o"
    if ! $CC $CFLAGS -c "$f" -o "$o"; then echo "BUILD-FAIL core: $f"; exit 1; fi
done
ar rcs "$OUT/libpurrgo_core.a" "$OUT"/core/*.o
CORE="$OUT/libpurrgo_core.a"

# build NAME OUTPUT EXTRA_FLAGS_AND_SOURCES... -> 0, если собралось
run_one() {
    name=$1; shift
    echo "== $name"
    if ! $CC $CFLAGS "$@" "$CORE" -o "$OUT/$name" -lm 2>"$OUT/$name.build.log"; then
        echo "BUILD-FAIL (см. $OUT/$name.build.log)"
        rc=1
        return
    fi
    "$OUT/$name" $RUN_ARGS 2>&1 | grep -E "^(FAIL|XPASS)|checks,|all passed|mismatches" || true
    "$OUT/$name" $RUN_ARGS >/dev/null 2>&1 || { echo "FAILED: $name"; rc=1; }
}

for t in tests/host/test_*.c; do
    name=$(basename "$t" .c)
    RUN_ARGS=""
    extra=""
    case $name in
        test_sd_spi)
            extra="apps/stm32/common/sd_spi/sd_spi.c -Iapps/stm32/common/sd_spi" ;;
        test_gnss_diag)
            extra="apps/stm32/common/gnss_diag/gnss_diag.c -Iapps/stm32/common/gnss_diag" ;;
        test_display_eink)
            if [ ! -f $EPD/src/epd4g.c ]; then echo "== $name"; echo "SKIP (нет сабмодуля eink_part_refresh)"; continue; fi
            extra="apps/stm32/common/Src/display_eink.c $EPD/src/epd4g.c $EPD/src/epd4g_fb.c \
                -Itests/host/stm32_shim -Iapps/stm32/common/Inc -I$EPD/include -DEPD4G_BW_THRESHOLD=3" ;;
    esac
    run_one "$name" $COMMON $extra "$t"
done

# Самотест частичного обновления E-Ink на модели панели: модель 1 и 2 (смена ролей плоскостей RAM).
if [ -f $EPD/src/epd4g.c ]; then
    for model in 1 2; do
        RUN_ARGS=$model
        run_one "eink_selftest_host_$model" src/platform/pc/purrgo_logger.c \
            tests/host/eink_selftest_host.c apps/stm32/common/Src/eink_test.c \
            apps/stm32/common/Src/display_stm32.c apps/stm32/common/Src/display_eink.c \
            $EPD/src/epd4g.c $EPD/src/epd4g_fb.c \
            -Itests/host/stm32_shim -Iapps/stm32/common/Inc -I$EPD/include \
            -DEPD4G_BW_THRESHOLD=3 -DPURRGO_EINK_TEST_SCENES=0xFFU -Wl,--wrap=display_set_mode
    done
else
    echo "== eink_selftest_host"; echo "SKIP (нет сабмодуля eink_part_refresh)"
fi
exit $rc
