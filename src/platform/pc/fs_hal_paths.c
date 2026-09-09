#include "purrgo/fs_hal.h"

const char* purrgo_fs_get_config_path(void) {
    return "PURRGO.CFG";
}

const char* purrgo_fs_get_maps_path(void) {
    return "../../../tests/data/maps";
}

const char* purrgo_fs_get_tracks_path(void) {
    return "";
}
