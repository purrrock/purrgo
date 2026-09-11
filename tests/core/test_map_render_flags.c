#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "purrgo/map.h"
#include "purrgo/config.h"
#include "purrgo/fs_hal.h"
#include "purrgo/app_fsm.h"

// Provide mocked config
purrgo_config_t app_config;

int open_landuse_idx = 0;
int open_landuse_mlp = 0;
int open_landuse_db = 0;
int open_roads_idx = 0;
int open_roads_mlp = 0;
int open_poi_idx = 0;
int open_poi_db = 0;

int render_layer_calls = 0;

purrgo_file_t* purrgo_fs_open(const char* path, fs_mode_t mode) {
    if (strstr(path, "landuse.idx")) open_landuse_idx++;
    if (strstr(path, "landuse.mlp")) open_landuse_mlp++;
    if (strstr(path, "landuse.db")) open_landuse_db++;
    if (strstr(path, "roads.idx")) open_roads_idx++;
    if (strstr(path, "roads.mlp")) open_roads_mlp++;
    if (strstr(path, "pois.idx")) open_poi_idx++;
    if (strstr(path, "pois.db")) open_poi_db++;
    return (purrgo_file_t*)1;
}

void purrgo_fs_close(purrgo_file_t* file) {}
bool purrgo_fs_seek(purrgo_file_t* file, uint32_t offset) { return true; }
uint32_t purrgo_fs_read(purrgo_file_t* file, uint8_t* buffer, uint32_t size) {
    // Fill with 0s to make it fail map_parse_pgo_header magic check (needs "PGO\0")
    memset(buffer, 0, size);
    return size;
}

// We also need to mock a few app_fsm/render functions if we don't compile with them, but we compile this as a standalone executable linking against core!
// Wait, we can just compile with core. But we need to override `purrgo_fs_open` which is in platform. So we link without platform.

void reset_counters() {
    open_landuse_idx = 0;
    open_landuse_mlp = 0;
    open_landuse_db = 0;
    open_roads_idx = 0;
    open_roads_mlp = 0;
    open_poi_idx = 0;
    open_poi_db = 0;
}

purrgo_map_scale_t purrgo_app_get_map_zoom_level() { return PURRGO_MAP_SCALE_500M; }

// Include the implementation so we don't have multiple definitions in linking
#include "../../src/core/map.c"

int main() {
    gfx_context_t gfx = {0};
    purrgo_viewport_t vp = {0};
    purrgo_bbox_t cam = {0};

    // TEST 1: ALL ENABLED
    reset_counters();
    app_config.layer_landuse = true;
    app_config.layer_landuse_labels = true;
    app_config.layer_roads = true;
    app_config.layer_poi = true;
    app_config.layer_poi_labels = true;

    purrgo_map_render_viewport(&gfx, &vp, &cam, "mock_dir");

    assert(open_landuse_idx == 1);
    assert(open_landuse_mlp == 1);
    assert(open_landuse_db == 1);
    assert(open_roads_idx == 1);
    assert(open_roads_mlp == 1);
    assert(open_poi_idx == 1);
    assert(open_poi_db == 1);

    // TEST 2: ALL DISABLED
    reset_counters();
    app_config.layer_landuse = false;
    app_config.layer_landuse_labels = false;
    app_config.layer_roads = false;
    app_config.layer_poi = false;
    app_config.layer_poi_labels = false;

    purrgo_map_render_viewport(&gfx, &vp, &cam, "mock_dir");

    assert(open_landuse_idx == 0);
    assert(open_landuse_mlp == 0);
    assert(open_landuse_db == 0);
    assert(open_roads_idx == 0);
    assert(open_roads_mlp == 0);
    assert(open_poi_idx == 0);
    assert(open_poi_db == 0);

    // TEST 3: MIXED
    reset_counters();
    app_config.layer_landuse = true;
    app_config.layer_landuse_labels = false;
    app_config.layer_roads = true;
    app_config.layer_poi = true;
    app_config.layer_poi_labels = false;

    purrgo_map_render_viewport(&gfx, &vp, &cam, "mock_dir");

    assert(open_landuse_idx == 1);
    assert(open_landuse_mlp == 1);
    assert(open_landuse_db == 0); // labels false
    assert(open_roads_idx == 1);
    assert(open_roads_mlp == 1);
    assert(open_poi_idx == 1);
    assert(open_poi_db == 0); // labels false

    // TEST 4: Only labels enabled (should NOT open DB if layer is disabled)
    reset_counters();
    app_config.layer_landuse = false;
    app_config.layer_landuse_labels = true;
    app_config.layer_roads = false;
    app_config.layer_poi = false;
    app_config.layer_poi_labels = true;

    purrgo_map_render_viewport(&gfx, &vp, &cam, "mock_dir");

    assert(open_landuse_idx == 0);
    assert(open_landuse_mlp == 0);
    assert(open_landuse_db == 0); // layer disabled, labels should not matter
    assert(open_roads_idx == 0);
    assert(open_roads_mlp == 0);
    assert(open_poi_idx == 0);
    assert(open_poi_db == 0); // layer disabled, labels should not matter

    printf("Render flags tests passed!\n");
    return 0;
}
