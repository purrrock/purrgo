import re

with open("src/core/ui/ui_map.c", "r") as f:
    code = f.read()

# Make sure we don't accidentally redefine things

code = code.replace("""        bool map_success = purrgo_map_render_viewport(gfx, &map_vp, &dynamic_cam, app_config.map_dir);

        ui_render_marker(gfx, gnss, &map_vp, &dynamic_cam);

        // Restore clipping so status UI can be drawn
        gfx_reset_clip(gfx);

        if (map_success) {
            purrgo_app_map_clear_dirty();
        }
        dbg_map_render_calls++;
    }""", """        bool map_success = purrgo_map_render_viewport(gfx, &map_vp, &dynamic_cam, app_config.map_dir);

        marker_state_t new_marker_state;
        ui_calc_marker_state(gnss, &map_vp, &dynamic_cam, &new_marker_state);
        ui_draw_marker(gfx, &new_marker_state);
        prev_marker_state = new_marker_state;

        // Restore clipping so status UI can be drawn
        gfx_reset_clip(gfx);

        if (map_success) {
            purrgo_app_map_clear_dirty();
        }
        display_refresh();
        dbg_map_render_calls++;
    } else {
        marker_state_t new_marker_state;
        ui_calc_marker_state(gnss, &map_vp, &dynamic_cam, &new_marker_state);

        bool changed = (new_marker_state.rendered != prev_marker_state.rendered) ||
                       (new_marker_state.gnss_valid != prev_marker_state.gnss_valid) ||
                       (new_marker_state.course_valid != prev_marker_state.course_valid) ||
                       (new_marker_state.course_deg != prev_marker_state.course_deg) ||
                       (new_marker_state.lat_1e7 != prev_marker_state.lat_1e7) ||
                       (new_marker_state.lon_1e7 != prev_marker_state.lon_1e7);

        if (changed) {
            int16_t min_x = map_vp.offset_x + map_vp.width;
            int16_t max_x = map_vp.offset_x;
            int16_t min_y = map_vp.offset_y + map_vp.height;
            int16_t max_y = map_vp.offset_y;

            if (prev_marker_state.rendered) {
                if (prev_marker_state.min_x < min_x) min_x = prev_marker_state.min_x;
                if (prev_marker_state.max_x > max_x) max_x = prev_marker_state.max_x;
                if (prev_marker_state.min_y < min_y) min_y = prev_marker_state.min_y;
                if (prev_marker_state.max_y > max_y) max_y = prev_marker_state.max_y;
            }

            if (new_marker_state.rendered) {
                if (new_marker_state.min_x < min_x) min_x = new_marker_state.min_x;
                if (new_marker_state.max_x > max_x) max_x = new_marker_state.max_x;
                if (new_marker_state.min_y < min_y) min_y = new_marker_state.min_y;
                if (new_marker_state.max_y > max_y) max_y = new_marker_state.max_y;
            }

            if (min_x <= max_x && min_y <= max_y) {
                // Clamp to viewport
                if (min_x < map_vp.offset_x) min_x = map_vp.offset_x;
                if (max_x >= map_vp.offset_x + map_vp.width) max_x = map_vp.offset_x + map_vp.width - 1;
                if (min_y < map_vp.offset_y) min_y = map_vp.offset_y;
                if (max_y >= map_vp.offset_y + map_vp.height) max_y = map_vp.offset_y + map_vp.height - 1;

                int16_t clip_w = max_x - min_x + 1;
                int16_t clip_h = max_y - min_y + 1;

                if (clip_w > 0 && clip_h > 0) {
                    gfx_set_clip(gfx, min_x, min_y, clip_w, clip_h);
                    purrgo_map_render_viewport(gfx, &map_vp, &dynamic_cam, app_config.map_dir);
                    ui_draw_marker(gfx, &new_marker_state);
                    gfx_reset_clip(gfx);

                    display_refresh_region(min_x, min_y, clip_w, clip_h);
                }
            }
            prev_marker_state = new_marker_state;
        }
    }""")

with open("src/core/ui/ui_map.c", "w") as f:
    f.write(code)
