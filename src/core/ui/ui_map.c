#include "ui_common.h"

extern int dbg_map_render_calls;

void ui_render_map(gfx_context_t* gfx, const purrgo_gnss_solution_t* gnss, const purrgo_sun_info_t* sun) {
    (void)gnss; // Currently unused in map view
    (void)sun;  // Currently unused in map view

    purrgo_viewport_t map_vp = {
        .width = PURRGO_HW_DISPLAY_WIDTH_PX,
        .height = PURRGO_HW_DISPLAY_HEIGHT_PX - 18,
        .offset_x = 0,
        .offset_y = 9
    };

    static bool map_screen_logged = false;

    // Clear top status area (0 to offset_y)
    gfx_set_color(gfx, 0, 3);
    gfx_fill_rect(gfx, 0, 0, PURRGO_HW_DISPLAY_WIDTH_PX, map_vp.offset_y);

    // Clear bottom status area
    gfx_fill_rect(gfx, 0, map_vp.offset_y + map_vp.height, PURRGO_HW_DISPLAY_WIDTH_PX, PURRGO_HW_DISPLAY_HEIGHT_PX - (map_vp.offset_y + map_vp.height));

    if (!map_screen_logged) {
        PURRGO_LOG("EMU: APP_STATE_MAP rendering started\n");
        map_screen_logged = true;
    }

    int32_t center_lat = purrgo_app_get_map_center_lat();
    int32_t center_lon = purrgo_app_get_map_center_lon();
    uint32_t width_m = purrgo_app_get_map_scale_width_m();

    purrgo_bbox_t dynamic_cam;
    purrgo_geo_bbox_from_center(center_lat, center_lon, width_m, &map_vp, &dynamic_cam);

    /* Верхняя служебная строка. */
    gfx_set_color(gfx, 0, 3);
    gfx_draw_string(gfx, 5, 1, "TOP STATUS AREA");

    if (purrgo_app_map_is_dirty()) {
        // Clear map viewport
        gfx_set_color(gfx, 0, 3);
        gfx_fill_rect(gfx, map_vp.offset_x, map_vp.offset_y, map_vp.width, map_vp.height);

        bool map_success = purrgo_map_render_viewport(gfx, &map_vp, &dynamic_cam, app_config.map_dir);

        // Name handling was removed from the orchestrator in the ui as it's not actually drawn right now, but let's read it to be safe
        char name_path[PURRGO_FS_MAX_PATH];
        char map_name_buf[65] = {0};
        snprintf(name_path, sizeof(name_path), "%s/map.name", app_config.map_dir);
        purrgo_file_t* name_file = purrgo_fs_open(name_path, FS_READ);

        if (name_file) {
            uint32_t n = purrgo_fs_read(name_file, (uint8_t*)map_name_buf, 64);
            if (n > 64) n = 64;
            map_name_buf[n] = '\0';
            purrgo_fs_close(name_file);
        } else {
            map_name_buf[0] = '\0';
        }

        if (map_success) {
            purrgo_app_map_clear_dirty();
        }
        dbg_map_render_calls++;
    }

    gfx_set_color(gfx, 0, 3);

    // Отрисовка масштаба в правом нижнем углу
    const char* scale_label = purrgo_app_get_map_scale_label();

    int label_len = 0;
    while(scale_label[label_len] != '\0') label_len++;
    int text_width = label_len * 6;

    int16_t scale_x = map_vp.offset_x + map_vp.width - text_width - 5;
    int16_t scale_y = map_vp.offset_y + map_vp.height - 8 - 5;

    gfx_draw_string(gfx, scale_x, scale_y, scale_label);
}
