#include <purrgo/pc_gnss_mock.h>
#include <purrgo/hardware_config.h>
#include <stdio.h>
#include <string.h>

/*
 * Стартовая точка mock-приёмника. Переопределяется при конфигурации, например для
 * проверки отрицательной высоты (берег Мёртвого моря):
 *   cmake --preset linux-gcc-debug -DPURRGO_MOCK_LAT_1E7=315590000 \
 *         -DPURRGO_MOCK_LON_1E7=354731666 -DPURRGO_MOCK_ALT_M=-430
 */
#ifndef PURRGO_MOCK_LAT_1E7
#define PURRGO_MOCK_LAT_1E7 537135000
#endif
#ifndef PURRGO_MOCK_LON_1E7
#define PURRGO_MOCK_LON_1E7 284199000
#endif
#ifndef PURRGO_MOCK_ALT_M
#define PURRGO_MOCK_ALT_M 150
#endif

static purrgo_gnss_solution_t state;
static char nmea_buffer[384];
/* Поток к приложению: NMEA, затем (при PURRGO_GNSS_UBX) кадры UBX. */
static uint8_t stream_buffer[1024];
static size_t nmea_len = 0;     /* длина всего потока */
static size_t nmea_pos = 0;

#if PURRGO_GNSS_UBX
/* Одометр мока: дистанция растёт на MOCK_STEP_M за каждое обновление (раз в секунду). */
#define MOCK_STEP_M 3U
static uint32_t mock_odo_m;
static uint32_t mock_odo_total_m = 12345U;
static bool mock_ver_pending;
#endif

static void format_lat(int32_t lat_1e7, char* buf, size_t size) {
    int32_t abs_lat = lat_1e7 < 0 ? -lat_1e7 : lat_1e7;
    int32_t deg = abs_lat / 10000000;
    int32_t frac = abs_lat % 10000000;
    int32_t min_1e7 = frac * 60;
    int32_t min_int = min_1e7 / 10000000;
    int32_t min_frac = (min_1e7 % 10000000) / 1000;
    snprintf(buf, size, "%02d%02d.%04d,%c",
             (int)deg, (int)min_int, (int)min_frac,
             lat_1e7 >= 0 ? 'N' : 'S');
}

static void format_lon(int32_t lon_1e7, char* buf, size_t size) {
    int32_t abs_lon = lon_1e7 < 0 ? -lon_1e7 : lon_1e7;
    int32_t deg = abs_lon / 10000000;
    int32_t frac = abs_lon % 10000000;
    int32_t min_1e7 = frac * 60;
    int32_t min_int = min_1e7 / 10000000;
    int32_t min_frac = (min_1e7 % 10000000) / 1000;
    snprintf(buf, size, "%03d%02d.%04d,%c",
             (int)deg, (int)min_int, (int)min_frac,
             lon_1e7 >= 0 ? 'E' : 'W');
}

static uint8_t calc_checksum(const char* sentence) {
    uint8_t cksum = 0;
    for (int i = 1; sentence[i] && sentence[i] != '*'; i++) {
        cksum ^= (uint8_t)sentence[i];
    }
    return cksum;
}

static void generate_nmea(void) {
    char lat_str[32];
    char lon_str[32];
    format_lat(state.lat_1e7, lat_str, sizeof(lat_str));
    format_lon(state.lon_1e7, lon_str, sizeof(lon_str));

    char course_str[16] = "";
    if (state.course_valid) {
        snprintf(course_str,
                 sizeof(course_str),
                 "%d.%02d",
                 (int)(state.course_deg_100 / 100),
                 (int)(state.course_deg_100 % 100));
    }

    char rmc[128];
    snprintf(rmc, sizeof(rmc),
             "$GPRMC,%02d%02d%02d,A,%s,%s,%d.%02d,%s,%02d%02d%02d,,,A*",
             (int)state.hours, (int)state.minutes, (int)state.seconds,
             lat_str, lon_str,
             (int)(state.speed_knots / 100), (int)(state.speed_knots % 100),
             course_str,
             (int)state.day, (int)state.month, (int)state.year);

    uint8_t rmc_cksum = calc_checksum(rmc);

    char gga[128];
    snprintf(gga, sizeof(gga),
             "$GPGGA,%02d%02d%02d,%s,%s,1,%02d,1.0,%d.0,M,0.0,M,,*",
             (int)state.hours, (int)state.minutes, (int)state.seconds,
             lat_str, lon_str,
             (int)state.satellites_tracked,
             (int)state.alt_m);

    uint8_t gga_cksum = calc_checksum(gga);

    snprintf(nmea_buffer, sizeof(nmea_buffer), "%s%02X\r\n%s%02X\r\n",
             rmc, (int)rmc_cksum, gga, (int)gga_cksum);
    nmea_len = strlen(nmea_buffer);
    memcpy(stream_buffer, nmea_buffer, nmea_len);
    nmea_pos = 0;
}

#if PURRGO_GNSS_UBX
static void put_u16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

/* Добавляет кадр UBX в конец потока (контрольная сумма Fletcher-8). */
static void append_ubx(uint8_t cls, uint8_t id, const uint8_t *payload, uint16_t len)
{
    if (nmea_len + (size_t)len + 8U > sizeof stream_buffer) {
        return;
    }
    uint8_t *f = &stream_buffer[nmea_len];
    uint8_t a = 0, b = 0;

    f[0] = 0xB5; f[1] = 0x62; f[2] = cls; f[3] = id;
    put_u16(&f[4], len);
    if (len != 0U) {
        memcpy(&f[6], payload, len);
    }
    for (size_t i = 2; i < 6U + (size_t)len; i++) {
        a = (uint8_t)(a + f[i]);
        b = (uint8_t)(b + a);
    }
    f[6U + len] = a;
    f[7U + len] = b;
    nmea_len += (size_t)len + 8U;
}

static void generate_ubx(void)
{
    uint8_t pvt[92];
    uint8_t odo[20];
    uint8_t sec[4];
    uint8_t rf[28];

    memset(pvt, 0, sizeof pvt);
    put_u16(&pvt[4], (uint16_t)(2000U + state.year));
    pvt[6] = state.month; pvt[7] = state.day;
    pvt[8] = state.hours; pvt[9] = state.minutes; pvt[10] = state.seconds;
    pvt[11] = 0x03;                                  /* validDate, validTime */
    pvt[20] = 3;                                     /* 3D */
    pvt[21] = 0x01;                                  /* gnssFixOK */
    pvt[23] = state.satellites_tracked;
    put_u32(&pvt[24], (uint32_t)state.lon_1e7);
    put_u32(&pvt[28], (uint32_t)state.lat_1e7);
    put_u32(&pvt[36], (uint32_t)((int32_t)state.alt_m * 1000));
    put_u32(&pvt[40], 3500U);                        /* hAcc 3.5 м */
    put_u32(&pvt[44], 5200U);                        /* vAcc 5.2 м */
    put_u32(&pvt[60], (uint32_t)(((int64_t)state.speed_knots * 1852) / 360));
    put_u32(&pvt[64], (uint32_t)(state.course_deg_100 * 1000));
    put_u32(&pvt[68], 300U);                         /* sAcc */
    put_u32(&pvt[72], 200000U);                      /* headAcc 2 градуса */
    put_u16(&pvt[76], 150U);                         /* pDOP 1.5 */
    append_ubx(0x01, 0x07, pvt, sizeof pvt);

    memset(odo, 0, sizeof odo);
    put_u32(&odo[8], mock_odo_m);
    put_u32(&odo[12], mock_odo_total_m);
    put_u32(&odo[16], 2U);
    append_ubx(0x01, 0x09, odo, sizeof odo);

    memset(sec, 0, sizeof sec);
    sec[0] = 2;
    sec[1] = (uint8_t)(0x01 | (1U << 1) | 0x08 | (1U << 4));   /* детекторы включены, всё чисто */
    append_ubx(0x27, 0x09, sec, sizeof sec);

    memset(rf, 0, sizeof rf);
    rf[1] = 1;
    rf[4 + 2] = 2;                                   /* antStatus OK */
    rf[4 + 3] = 1;                                   /* antPower ON */
    put_u16(&rf[4 + 12], 90U);                       /* noisePerMS */
    put_u16(&rf[4 + 14], 3000U);                     /* agcCnt ~37% */
    rf[4 + 16] = 12;                                 /* cwSuppression */
    append_ubx(0x0A, 0x38, rf, sizeof rf);

    if (mock_ver_pending) {
        uint8_t ver[70];

        memset(ver, 0, sizeof ver);
        memcpy(&ver[0], "EXT CORE 5.30 (mock)", 20);
        memcpy(&ver[30], "000A0000", 8);
        memcpy(&ver[40], "PROTVER=34.30", 13);
        append_ubx(0x0A, 0x04, ver, sizeof ver);
        mock_ver_pending = false;
    }
}

bool pc_gnss_mock_write(const uint8_t *data, uint16_t length)
{
    if (data == NULL || length < 8U || data[0] != 0xB5 || data[1] != 0x62) {
        return false;
    }
    if (data[2] == 0x01 && data[3] == 0x10) {        /* UBX-NAV-RESETODO */
        mock_odo_m = 0U;
    } else if (data[2] == 0x0A && data[3] == 0x04) { /* UBX-MON-VER (опрос) */
        mock_ver_pending = true;
    }
    return true;
}
#endif

void pc_gnss_mock_init(void) {
    memset(&state, 0, sizeof(state));
    state.valid = true; // 3D FIX

    // Base coordinates: 53.7135, 28.4199
    // Converted to int32_t * 10^7
    state.lat_1e7 = PURRGO_MOCK_LAT_1E7;
    state.lon_1e7 = PURRGO_MOCK_LON_1E7;

    // Static Mock data
    state.speed_knots = 269; // 5km/h ~ 2.69 knots -> 269
    state.alt_m = PURRGO_MOCK_ALT_M;
    state.satellites_tracked = 9;

    // Set course to valid and 45.00 degrees initially
    state.course_valid = true;
    state.course_deg_100 = 4500;

    // Static time and date
    state.hours = 12;
    state.minutes = 34;
    state.seconds = 56;
    state.day = 1;
    state.month = 1;
    state.year = 24;

    generate_nmea();
#if PURRGO_GNSS_UBX
    mock_odo_m = 0U;
    generate_ubx();
#endif
}

void pc_gnss_mock_update(void) {
    // Time progression logic
    // Increment seconds by 1
    state.seconds++;

    // If seconds reach 60, reset to 0 and increment minutes
    if (state.seconds >= 60) {
        state.seconds = 0;
        state.minutes++;

        // If minutes reach 60, reset to 0 and increment hours
        if (state.minutes >= 60) {
            state.minutes = 0;
            // Hours wrap around at 24 using modulo operation
            state.hours = (uint8_t)((state.hours + 1) % 24);
        }
    }

    // Simulate movement by slightly changing coordinates.
    // Adding 200 to latitude (approx 20 meter)
    state.lat_1e7 += 200;
    // Adding 150 to longitude (approx 25 meters)
    state.lon_1e7 += 250;

    // Toggle course_valid based on seconds to simulate GPS loss of course
    if (state.seconds % 10 == 0) {
        state.course_valid = !state.course_valid;
    }

    // Increment course slightly, wrapping around 360 degrees
    state.course_deg_100 += 150; // Add 1.5 degrees
    if (state.course_deg_100 >= 36000) {
        state.course_deg_100 -= 36000;
    }

#if PURRGO_GNSS_UBX
    mock_odo_m += MOCK_STEP_M;
    mock_odo_total_m += MOCK_STEP_M;
#endif
    generate_nmea();
#if PURRGO_GNSS_UBX
    generate_ubx();
#endif
}

bool pc_gnss_mock_read_byte(uint8_t *byte) {
    if (nmea_pos < nmea_len) {
        if (byte) {
            *byte = stream_buffer[nmea_pos];
        }
        nmea_pos++;
        return true;
    }
    return false;
}
