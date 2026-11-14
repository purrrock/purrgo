#include "purrgo/fs_hal.h"

#ifdef __EMSCRIPTEN__
/*
 * Web-сборка: виртуальная файловая система Emscripten (MEMFS в памяти вкладки).
 * Демо-карта, книги и маршруты упаковываются в образ при сборке (--preload-file),
 * PURRGO.CFG и треки живут только до перезагрузки страницы.
 */
const char* purrgo_fs_get_config_path(void) { return "/PURRGO.CFG"; }
const char* purrgo_fs_get_maps_path(void)   { return "/maps"; }
const char* purrgo_fs_get_routes_path(void) { return "/routes"; }
const char* purrgo_fs_get_waypts_path(void) { return "/waypts"; }
const char* purrgo_fs_get_books_path(void)  { return "/books"; }
const char* purrgo_fs_get_tracks_path(void) { return "/tracks"; }
#else

const char* purrgo_fs_get_config_path(void) {
    return "PURRGO.CFG";
}

const char* purrgo_fs_get_maps_path(void) {
    return "../../../../tests/data/maps";
}

const char* purrgo_fs_get_routes_path(void) {
    return "../../../../tests/data/routes";
}

const char* purrgo_fs_get_waypts_path(void) {
    return "../../../../tests/data/waypts";
}

const char* purrgo_fs_get_books_path(void) {
    return "../../../../tests/data/books";
}

const char* purrgo_fs_get_tracks_path(void) {
    return "";
}

#endif /* __EMSCRIPTEN__ */
