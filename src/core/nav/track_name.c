#include "purrgo/track_name.h"
#include "purrgo/fs_hal.h"
#include "purrgo/purrgo_format.h"
#include "purrgo/purrgo_time.h"
#include "purrgo/track_logger.h"
#include <string.h>

/*
 * Имена прежнего формата - секунды от 01.01.2026 (не больше нескольких десятков
 * миллионов за год). Новые имена за 2017 год и позже не меньше этого порога
 * (0x20000000 секунд = 17 лет от 2000 года).
 */
#define TRACK_ID_NEW_FORMAT_MIN 0x20000000u

static char lower_ascii(char c)
{
    return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

int purrgo_track_name_cmp(const char *a, const char *b)
{
    for (;;) {
        const unsigned char ca = (unsigned char)lower_ascii(*a);
        const unsigned char cb = (unsigned char)lower_ascii(*b);

        if (ca != cb) {
            return (ca < cb) ? -1 : 1;
        }
        if (ca == '\0') {
            return 0;
        }
        a++;
        b++;
    }
}

static int hex_digit(char c)
{
    if (c >= '0' && c <= '9') { return c - '0'; }
    if (c >= 'a' && c <= 'f') { return c - 'a' + 10; }
    if (c >= 'A' && c <= 'F') { return c - 'A' + 10; }
    return -1;
}

bool purrgo_track_name_decode(const char *name, uint32_t *epoch_out)
{
    if (name == NULL || strlen(name) != 12u) {
        return false;
    }
    if (name[8] != '.' ||
        lower_ascii(name[9]) != 'g' ||
        lower_ascii(name[10]) != 'p' ||
        lower_ascii(name[11]) != 'x') {
        return false;
    }

    uint32_t v = 0;
    for (int i = 0; i < 8; i++) {
        const int d = hex_digit(name[i]);

        if (d < 0) {
            return false;
        }
        v = (v << 4) | (uint32_t)d;
    }

    if (v < TRACK_ID_NEW_FORMAT_MIN) {
        return false;
    }
    if (epoch_out != NULL) {
        *epoch_out = v;
    }
    return true;
}

void purrgo_track_name_label(const char *name, int16_t tz_offset_min,
                             char *out, size_t size)
{
    if (out == NULL || size == 0u) {
        return;
    }

    uint32_t epoch;
    uint8_t y, mo, d, h, mi, s;

    if (name != NULL && purrgo_track_name_decode(name, &epoch)) {
        const int64_t local = (int64_t)epoch + (int64_t)tz_offset_min * 60;

        if (local >= 0 && local <= (int64_t)UINT32_MAX &&
            purrgo_time_epoch_to_datetime((uint32_t)local, &y, &mo, &d, &h, &mi, &s)) {
            (void)purrgo_snprintf(out, size, "%04u-%02u-%02u %02u:%02u",
                                  2000u + (unsigned)y, (unsigned)mo, (unsigned)d,
                                  (unsigned)h, (unsigned)mi);
            return;
        }
    }

    (void)purrgo_snprintf(out, size, "%s", (name != NULL) ? name : "");
}

bool purrgo_track_name_is_recording(const char *name)
{
    if (name == NULL || purrgo_logger_get_state() != LOGGER_STATE_RECORDING) {
        return false;
    }

    const char *active = purrgo_logger_get_active_filename();
    if (active == NULL) {
        return false;
    }

    char path[PURRGO_FS_MAX_PATH + 16];
    const char *prefix = purrgo_fs_get_tracks_path();
    const int n = (prefix[0] != '\0')
        ? purrgo_snprintf(path, sizeof path, "%s/%s", prefix, name)
        : purrgo_snprintf(path, sizeof path, "%s", name);

    if (n <= 0 || (size_t)n >= sizeof path) {
        return false;
    }
    return purrgo_track_name_cmp(path, active) == 0;
}
