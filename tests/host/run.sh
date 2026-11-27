#!/bin/sh
# Сборка и запуск host-тестов (ядро + фейковая ФС) с ASan/UBSan.
# Запускать из корня репозитория. Каждый tests/host/test_*.c - отдельный бинарник.
# Дополнительные исходники и флаги отдельных тестов повторяют tests/host/CMakeLists.txt.
# Альтернатива: cmake --preset linux-gcc-test && ctest --preset linux-gcc-test
#
# Ошибка сборки одного теста не прерывает прогон: тест помечается BUILD-FAIL,
# код возврата ненулевой. Тесты E-Ink используют third_party/epd4g; если каталога нет, они пропускаются (SKIP).
CC=${CC:-gcc}
OUT=${OUT:-/tmp/purrgo_host_tests}
EPD=third_party/epd4g
mkdir -p "$OUT"
rc=0

CFLAGS="-std=c11 -D_DEFAULT_SOURCE -g -fsanitize=address,undefined -fno-sanitize-recover=undefined \
    -Wall -Wextra -Wno-unused-parameter -DTEST_DATA_DIR=\"tests/host/data\" \
    -Iinclude -Isrc/core -Ithird_party/minmea -Itests/host"
COMMON="src/platform/pc/display_hal.c src/platform/pc/purrgo_logger.c \
    tests/host/fake_fs.c tests/host/fake_power.c"

# purrgo_version.h (версия, ревизия git, признак Debug) в CMake-сборке генерирует
# cmake/PurrgoVersionGen.cmake; boot_banner.c его включает. Здесь тот же шаблон
# cmake/version.h.in заполняется через sed, чтобы скрипту не нужен был cmake.
GEN="$OUT/generated"
mkdir -p "$GEN"
VER=$(sed -n 's/^set(PURRGO_VERSION "\([^"]*\)".*/\1/p' cmake/PurrgoVersion.cmake | head -n 1)
REV=$(git describe --always --dirty --abbrev=7 2>/dev/null || echo nogit)
sed -e "s|@PURRGO_VERSION@|${VER:-0.0.0}|" \
    -e "s|@PURRGO_GIT_REVISION@|$REV|" \
    -e "s|@PURRGO_BUILD_IS_DEBUG@|1|" \
    cmake/version.h.in > "$GEN/purrgo_version.h"
CFLAGS="$CFLAGS -I$GEN"

# Ядро собирается в статическую библиотеку, как purrgo_core в CMake: из неё берутся
# только нужные тесту объектные файлы (тесты сами задают purrgo_system_time_ms и т.п.).
# PURRGO_TRACK_DIAG=0 - как в tests/host/CMakeLists.txt: тесты логгера считают обычную
# (не диагностическую) запись трека, а значение по умолчанию в track_logger.c может быть
# временно переключено на 1 для полевого теста. test_track_diag включает track_logger.c
# в себя с PURRGO_TRACK_DIAG=1 и от этого флага не зависит.
rm -rf "$OUT/core" && mkdir -p "$OUT/core"
for f in $(ls src/core/*/*.c third_party/minmea/minmea.c); do
    o="$OUT/core/$(echo "$f" | tr / _).o"
    if ! $CC $CFLAGS -DPURRGO_TRACK_DIAG=0 -c "$f" -o "$o"; then echo "BUILD-FAIL core: $f"; exit 1; fi
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
        test_gfx_line_equiv)
            extra="tests/host/gfx_line_ref.c" ;;
        test_sd_spi)
            extra="stm32/port/storage/sd_spi.c -Istm32/port/storage" ;;
        test_sd_power)
            extra="stm32/port/storage/sd_power.c -Istm32/port/storage" ;;
        test_gnss_diag)
            extra="stm32/port/gnss/gnss_diag.c -Istm32/port/gnss" ;;
        test_gnss_link)
            extra="stm32/port/gnss/gnss_link.c stm32/port/gnss/gnss_diag.c \
                -Istm32/port/gnss" ;;
        test_display_eink)
            if [ ! -f $EPD/src/epd4g.c ]; then echo "== $name"; echo "SKIP (нет third_party/epd4g)"; continue; fi
            extra="stm32/port/display/display_eink.c $EPD/src/epd4g.c $EPD/src/epd4g_fb.c \
                -Itests/host/stm32_shim -Istm32/port/display -I$EPD/include -DEPD4G_BW_THRESHOLD=3" ;;
    esac
    run_one "$name" $COMMON $extra "$t"
done

# Режимы трекера, которые не выбраны по умолчанию (по умолчанию цикл по EN, 3): PSMCT (1) и PSMOO (2). Профили приёмника
# и вход/выход из трекера зависят от PURRGO_GNSS_TRACKER_MODE на этапе компиляции, поэтому два теста перегоняются
# с этим режимом и с исходниками ядра, которые от него зависят (явно заданные объекты имеют приоритет над библиотекой).
for mode in 1 2; do
    for base in test_gnss_ubx test_tracker_mode; do
        run_one "${base}_mode${mode}" $COMMON -DPURRGO_GNSS_TRACKER_MODE=$mode \
            src/core/gnss/gnss_receiver.c src/core/ui/app_fsm.c src/core/gnss/gnss_duty.c \
            "tests/host/$base.c"
    done
done

# Самотест частичного обновления E-Ink на модели панели: модель 1 и 2 (смена ролей плоскостей RAM).
if [ -f $EPD/src/epd4g.c ]; then
    for model in 1 2; do
        RUN_ARGS=$model
        run_one "eink_selftest_host_$model" src/platform/pc/purrgo_logger.c \
            tests/host/eink_selftest_host.c stm32/port/display/eink_test.c \
            stm32/port/display/display_stm32.c stm32/port/display/display_eink.c \
            $EPD/src/epd4g.c $EPD/src/epd4g_fb.c \
            -Itests/host/stm32_shim -Istm32/port/display -I$EPD/include \
            -DEPD4G_BW_THRESHOLD=3 -DPURRGO_EINK_TEST_SCENES=0xFFU -Wl,--wrap=display_set_mode
    done
else
    echo "== eink_selftest_host"; echo "SKIP (нет third_party/epd4g)"
fi
exit $rc
