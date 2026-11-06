#include "purrgo/fs_hal.h"

const char* purrgo_fs_get_config_path(void) {
    return "PURRGO.CFG";
}

const char* purrgo_fs_get_maps_path(void) {
    return "../../../../../tests/data/maps";
}

const char* purrgo_fs_get_routes_path(void) {
    return "../../../../../tests/data/routes";
}

const char* purrgo_fs_get_waypts_path(void) {
    return "../../../../../tests/data/waypts";
}

const char* purrgo_fs_get_books_path(void) {
    return "../../../../../tests/data/books";
}

const char* purrgo_fs_get_tracks_path(void) {
    return "";
}
