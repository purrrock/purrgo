#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "purrgo/config.h"
#include "purrgo/fs_hal.h"
#include "purrgo/logger.h"

struct purrgo_file_s {
    int dummy;
};

static struct purrgo_file_s mock_file;
static bool mock_file_exists = false;
static char mock_file_content[1024];
static size_t mock_file_len = 0;
static size_t mock_file_pos = 0;

purrgo_file_t* purrgo_fs_open(const char* path, fs_mode_t mode) {
    if (mode == FS_READ) {
        if (mock_file_exists) {
            mock_file_pos = 0;
            return &mock_file;
        }
        return NULL;
    } else {
        // write
        mock_file_exists = true;
        mock_file_len = 0;
        mock_file_pos = 0;
        return &mock_file;
    }
}

uint32_t purrgo_fs_read(purrgo_file_t* file, uint8_t* buffer, uint32_t size) {
    if (file != &mock_file) return 0;
    size_t to_read = mock_file_len - mock_file_pos;
    if (to_read > size) to_read = size;
    memcpy(buffer, mock_file_content + mock_file_pos, to_read);
    mock_file_pos += to_read;
    return to_read;
}

uint32_t purrgo_fs_write(purrgo_file_t* file, const uint8_t* buffer, uint32_t size) {
    if (file != &mock_file) return 0;
    size_t to_write = sizeof(mock_file_content) - mock_file_len;
    if (to_write > size) to_write = size;
    memcpy(mock_file_content + mock_file_len, buffer, to_write);
    mock_file_len += to_write;
    return to_write;
}

bool purrgo_fs_seek(purrgo_file_t* file, uint32_t offset) { return true; }
void purrgo_fs_close(purrgo_file_t* file) {}
void purrgo_fs_sync(purrgo_file_t* file) {}
purrgo_dir_t* purrgo_fs_opendir(const char* path) { return NULL; }
bool purrgo_fs_readdir(purrgo_dir_t* dir, purrgo_fs_dirent_t* dirent) { return false; }
void purrgo_fs_closedir(purrgo_dir_t* dir) {}

void purrgo_logger_log(const char* level, const char* file, int line, const char* format, ...) {}

static int num_failures = 0;
#define EXPECT_TRUE(cond) if (!(cond)) { printf("FAIL: %s at %d\n", #cond, __LINE__); num_failures++; }
#define EXPECT_FALSE(cond) if (cond) { printf("FAIL: %s at %d\n", #cond, __LINE__); num_failures++; }
#define EXPECT_EQ(exp, act) if ((exp) != (act)) { printf("FAIL: expected %ld, got %ld at %d\n", (long)(exp), (long)(act), __LINE__); num_failures++; }

void test_missing_keys_retain_defaults() {
    printf("test_missing_keys_retain_defaults\n");
    // Make a file missing some keys
    mock_file_exists = true;
    strcpy(mock_file_content, "TZ_MIN=-120\nLAST_LAT_1E7=1234567\n");
    mock_file_len = strlen(mock_file_content);

    // Mess up the config to ensure defaults are applied
    app_config.tz_offset_minutes = 999;
    app_config.last_lon_1e7 = 999;
    app_config.layer_landuse = false;

    purrgo_config_load();

    EXPECT_EQ(-120, app_config.tz_offset_minutes); // Read from file
    EXPECT_EQ(1234567, app_config.last_lat_1e7); // Read from file
    EXPECT_EQ(284199000, app_config.last_lon_1e7); // Default
    EXPECT_TRUE(app_config.layer_landuse); // Default
}

void test_map_layers_load_save() {
    printf("test_map_layers_load_save\n");

    // Initialize with some config
    purrgo_config_init();
    app_config.layer_landuse = false;
    app_config.layer_water = true;
    app_config.layer_landuse_labels = false;
    app_config.layer_water_labels = true;
    app_config.layer_roads = false;
    app_config.layer_poi = true;
    app_config.layer_poi_labels = false;
    app_config.layer_route = true;
    app_config.layer_track = false;

    // Save to mock file
    purrgo_config_save();

    // Verify it was saved correctly (spot check)
    EXPECT_TRUE(strstr(mock_file_content, "LAYER_LANDUSE=0") != NULL);
    EXPECT_TRUE(strstr(mock_file_content, "LAYER_WATER=1") != NULL);
    EXPECT_TRUE(strstr(mock_file_content, "LAYER_LANDUSE_LABELS=0") != NULL);
    EXPECT_TRUE(strstr(mock_file_content, "LAYER_WATER_LABELS=1") != NULL);
    EXPECT_TRUE(strstr(mock_file_content, "LAYER_ROADS=0") != NULL);
    EXPECT_TRUE(strstr(mock_file_content, "LAYER_POI=1") != NULL);
    EXPECT_TRUE(strstr(mock_file_content, "LAYER_POI_LABELS=0") != NULL);
    EXPECT_TRUE(strstr(mock_file_content, "LAYER_ROUTE=1") != NULL);
    EXPECT_TRUE(strstr(mock_file_content, "LAYER_TRACK=0") != NULL);

    // Clear struct
    purrgo_config_init();

    // Load from mock file
    purrgo_config_load();

    // Verify properties
    EXPECT_FALSE(app_config.layer_landuse);
    EXPECT_TRUE(app_config.layer_water);
    EXPECT_FALSE(app_config.layer_landuse_labels);
    EXPECT_TRUE(app_config.layer_water_labels);
    EXPECT_FALSE(app_config.layer_roads);
    EXPECT_TRUE(app_config.layer_poi);
    EXPECT_FALSE(app_config.layer_poi_labels);
    EXPECT_TRUE(app_config.layer_route);
    EXPECT_FALSE(app_config.layer_track);
}

void test_overflow_protection() {
    printf("test_overflow_protection\n");
    mock_file_exists = true;
    strcpy(mock_file_content, "LAST_LAT_1E7=21474836480\nTZ_MIN=99999\n"); // Out of bounds string and range
    mock_file_len = strlen(mock_file_content);

    // Set some valid defaults
    app_config.last_lat_1e7 = 111;
    app_config.tz_offset_minutes = 222;

    purrgo_config_load();

    // Should retain defaults since parsing or semantic limits failed
    EXPECT_EQ(537135000, app_config.last_lat_1e7); // Since config_load calls init, it will be the default, not 111
    EXPECT_EQ(180, app_config.tz_offset_minutes);
}

int main(void) {
    test_missing_keys_retain_defaults();
    test_overflow_protection();
    test_map_layers_load_save();

    if (num_failures > 0) {
        printf("FAILED %d tests.\n", num_failures);
        return 1;
    }
    printf("All config tests passed.\n");
    return 0;
}
