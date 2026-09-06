#include <purrgo/gnss_mock.h>
#include <stdio.h>
#include <string.h>

static purrgo_gnss_solution_t state;
static char nmea_buffer[384];
static size_t nmea_len = 0;
static size_t nmea_pos = 0;

static void format_lat(int32_t lat_1e7, char* buf, size_t size) {
    int32_t abs_lat = lat_1e7 < 0 ? -lat_1e7 : lat_1e7;
    int32_t deg = abs_lat / 10000000;
    int32_t frac = abs_lat % 10000000;
    int32_t min_1e7 = frac * 60;
    int32_t min_int = min_1e7 / 10000000;
    int32_t min_frac = (min_1e7 % 10000000) / 1000;
    snprintf(buf, size, "%02d%02d.%04d,%c", (int)deg, (int)min_int, (int)min_frac, lat_1e7 >= 0 ? 'N' : 'S');
}

static void format_lon(int32_t lon_1e7, char* buf, size_t size) {
    int32_t abs_lon = lon_1e7 < 0 ? -lon_1e7 : lon_1e7;
    int32_t deg = abs_lon / 10000000;
    int32_t frac = abs_lon % 10000000;
    int32_t min_1e7 = frac * 60;
    int32_t min_int = min_1e7 / 10000000;
    int32_t min_frac = (min_1e7 % 10000000) / 1000;
    snprintf(buf, size, "%03d%02d.%04d,%c", (int)deg, (int)min_int, (int)min_frac, lon_1e7 >= 0 ? 'E' : 'W');
}

static uint8_t calc_checksum(const char* sentence) {
    uint8_t cksum = 0;
    for (int i = 1; sentence[i] && sentence[i] != '*'; i++) {
        cksum ^= sentence[i];
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
        snprintf(course_str, sizeof(course_str), "%d.%02d", (int)(state.course_deg_100 / 100), (int)(state.course_deg_100 % 100));
    }

    char rmc[128];
    snprintf(rmc, sizeof(rmc), "$GPRMC,%02d%02d%02d,A,%s,%s,%d.%02d,%s,%02d%02d%02d,,,A*",
             (int)state.hours, (int)state.minutes, (int)state.seconds,
             lat_str, lon_str,
             (int)(state.speed_knots / 100), (int)(state.speed_knots % 100),
             course_str,
             (int)state.day, (int)state.month, (int)state.year);

    uint8_t rmc_cksum = calc_checksum(rmc);

    char gga[128];
    snprintf(gga, sizeof(gga), "$GPGGA,%02d%02d%02d,%s,%s,1,%02d,1.0,%d.0,M,0.0,M,,*",
             (int)state.hours, (int)state.minutes, (int)state.seconds,
             lat_str, lon_str,
             (int)state.satellites_tracked,
             (int)state.alt_m);

    uint8_t gga_cksum = calc_checksum(gga);

    snprintf(nmea_buffer, sizeof(nmea_buffer), "%s%02X\r\n%s%02X\r\n", rmc, (int)rmc_cksum, gga, (int)gga_cksum);
    nmea_len = strlen(nmea_buffer);
    nmea_pos = 0;
}

void purrgo_gnss_mock_init(void) {
    memset(&state, 0, sizeof(state));
    state.valid = true; // 3D FIX

    // Base coordinates: 53.7135, 28.4199
    // Converted to int32_t * 10^7
    state.lat_1e7 = 537135000;
    state.lon_1e7 = 284199000;

    // Static Mock data
    state.speed_knots = 269; // 5km/h ~ 2.69 knots -> 269
    state.alt_m = 150;
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
}

void purrgo_gnss_mock_update(void) {
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
            state.hours = (state.hours + 1) % 24;
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

    generate_nmea();
}

bool purrgo_gnss_mock_read_byte(uint8_t *byte) {
    if (nmea_pos < nmea_len) {
        if (byte) {
            *byte = (uint8_t)nmea_buffer[nmea_pos];
        }
        nmea_pos++;
        return true;
    }
    return false;
}
