import re

with open("src/core/ui/ui_map.c", "r") as f:
    code = f.read()

# Replace ui_render_marker function signature and internals
code = code.replace("""static void ui_render_marker(
    gfx_context_t* gfx,
    const purrgo_gnss_solution_t* gnss,
    const purrgo_viewport_t* map_vp,
    const purrgo_bbox_t* dynamic_cam)
{
    int32_t lat;
    int32_t lon;

    /*
     * Use the current GNSS position when a valid fix exists.
     * Otherwise use the persistent configured fallback position.
     */
    if (gnss->valid) {
        lat = gnss->lat_1e7;
        lon = gnss->lon_1e7;
    } else {
        lat = app_config.last_lat_1e7;
        lon = app_config.last_lon_1e7;
    }

    /*
     * Project the geographic position using the existing integer projection.
     */
    int16_t cx;
    int16_t cy;""", """static void ui_calc_marker_state(
    const purrgo_gnss_solution_t* gnss,
    const purrgo_viewport_t* map_vp,
    const purrgo_bbox_t* dynamic_cam,
    marker_state_t* out_state)
{
    int32_t lat;
    int32_t lon;

    out_state->gnss_valid = gnss->valid;
    out_state->course_valid = gnss->course_valid;
    out_state->course_deg = (int16_t)((gnss->course_deg_100 / 100) % 360);
    out_state->rendered = false;

    /*
     * Use the current GNSS position when a valid fix exists.
     * Otherwise use the persistent configured fallback position.
     */
    if (gnss->valid) {
        lat = gnss->lat_1e7;
        lon = gnss->lon_1e7;
    } else {
        lat = app_config.last_lat_1e7;
        lon = app_config.last_lon_1e7;
    }

    out_state->lat_1e7 = lat;
    out_state->lon_1e7 = lon;

    /*
     * Project the geographic position using the existing integer projection.
     */
    int16_t cx;
    int16_t cy;""")

code = code.replace("""    /*
     * Maximum number of vertices required by any marker shape:
     *   - triangle: 3
     *   - octagon:  8
     */
    gfx_point_t pts[8];
    uint8_t point_count = 0;
    bool filled = false;

    if (gnss->valid && gnss->course_valid) {""", """    /*
     * Maximum number of vertices required by any marker shape:
     *   - triangle: 3
     *   - octagon:  8
     */
    if (gnss->valid && gnss->course_valid) {""")

code = code.replace("""        int16_t course_deg =
            (int16_t)((gnss->course_deg_100 / 100) % 360);

        int32_t s = get_sin_10k(course_deg);
        int32_t c = get_cos_10k(course_deg);

        for (uint8_t i = 0; i < 3; i++) {
            /*
             * Rotate local coordinates around the marker center.
             *
             * sin/cos are represented as integer values scaled by 10000.
             */
            pts[i].x = (int16_t)(
                cx +
                (local_x[i] * c - local_y[i] * s) / 10000
            );

            pts[i].y = (int16_t)(
                cy +
                (local_x[i] * s + local_y[i] * c) / 10000
            );
        }

        point_count = 3;
        filled = true;""", """        int32_t s = get_sin_10k(out_state->course_deg);
        int32_t c = get_cos_10k(out_state->course_deg);

        for (uint8_t i = 0; i < 3; i++) {
            /*
             * Rotate local coordinates around the marker center.
             *
             * sin/cos are represented as integer values scaled by 10000.
             */
            out_state->pts[i].x = (int16_t)(
                cx +
                (local_x[i] * c - local_y[i] * s) / 10000
            );

            out_state->pts[i].y = (int16_t)(
                cy +
                (local_x[i] * s + local_y[i] * c) / 10000
            );
        }

        out_state->point_count = 3;
        out_state->filled = true;""")

code = code.replace("""        pts[0].x = (int16_t)(cx - cut);
        pts[0].y = (int16_t)(cy + top);

        pts[1].x = (int16_t)(cx + cut);
        pts[1].y = (int16_t)(cy + top);

        pts[2].x = (int16_t)(cx + right);
        pts[2].y = (int16_t)(cy - cut);

        pts[3].x = (int16_t)(cx + right);
        pts[3].y = (int16_t)(cy + cut);

        pts[4].x = (int16_t)(cx + cut);
        pts[4].y = (int16_t)(cy + bottom);

        pts[5].x = (int16_t)(cx - cut);
        pts[5].y = (int16_t)(cy + bottom);

        pts[6].x = (int16_t)(cx + left);
        pts[6].y = (int16_t)(cy + cut);

        pts[7].x = (int16_t)(cx + left);
        pts[7].y = (int16_t)(cy - cut);

        point_count = 8;
        filled = true;""", """        out_state->pts[0].x = (int16_t)(cx - cut);
        out_state->pts[0].y = (int16_t)(cy + top);

        out_state->pts[1].x = (int16_t)(cx + cut);
        out_state->pts[1].y = (int16_t)(cy + top);

        out_state->pts[2].x = (int16_t)(cx + right);
        out_state->pts[2].y = (int16_t)(cy - cut);

        out_state->pts[3].x = (int16_t)(cx + right);
        out_state->pts[3].y = (int16_t)(cy + cut);

        out_state->pts[4].x = (int16_t)(cx + cut);
        out_state->pts[4].y = (int16_t)(cy + bottom);

        out_state->pts[5].x = (int16_t)(cx - cut);
        out_state->pts[5].y = (int16_t)(cy + bottom);

        out_state->pts[6].x = (int16_t)(cx + left);
        out_state->pts[6].y = (int16_t)(cy + cut);

        out_state->pts[7].x = (int16_t)(cx + left);
        out_state->pts[7].y = (int16_t)(cy - cut);

        out_state->point_count = 8;
        out_state->filled = true;""")

code = code.replace("""        pts[0].x = cx;
        pts[0].y = (int16_t)(cy + top_y);

        pts[1].x = (int16_t)(cx - w);
        pts[1].y = (int16_t)(cy + bottom_y);

        pts[2].x = (int16_t)(cx + w);
        pts[2].y = (int16_t)(cy + bottom_y);

        point_count = 3;
        filled = false;""", """        out_state->pts[0].x = cx;
        out_state->pts[0].y = (int16_t)(cy + top_y);

        out_state->pts[1].x = (int16_t)(cx - w);
        out_state->pts[1].y = (int16_t)(cy + bottom_y);

        out_state->pts[2].x = (int16_t)(cx + w);
        out_state->pts[2].y = (int16_t)(cy + bottom_y);

        out_state->point_count = 3;
        out_state->filled = false;""")

code = code.replace("""    /*
     * Determine the complete marker bounding box.
     */
    int16_t min_x = pts[0].x;
    int16_t max_x = pts[0].x;
    int16_t min_y = pts[0].y;
    int16_t max_y = pts[0].y;

    for (uint8_t i = 1; i < point_count; i++) {
        if (pts[i].x < min_x) {
            min_x = pts[i].x;
        }

        if (pts[i].x > max_x) {
            max_x = pts[i].x;
        }

        if (pts[i].y < min_y) {
            min_y = pts[i].y;
        }

        if (pts[i].y > max_y) {
            max_y = pts[i].y;
        }
    }""", """    /*
     * Determine the complete marker bounding box.
     */
    int16_t min_x = out_state->pts[0].x;
    int16_t max_x = out_state->pts[0].x;
    int16_t min_y = out_state->pts[0].y;
    int16_t max_y = out_state->pts[0].y;

    for (uint8_t i = 1; i < out_state->point_count; i++) {
        if (out_state->pts[i].x < min_x) {
            min_x = out_state->pts[i].x;
        }

        if (out_state->pts[i].x > max_x) {
            max_x = out_state->pts[i].x;
        }

        if (out_state->pts[i].y < min_y) {
            min_y = out_state->pts[i].y;
        }

        if (out_state->pts[i].y > max_y) {
            max_y = out_state->pts[i].y;
        }
    }

    out_state->min_x = min_x;
    out_state->max_x = max_x;
    out_state->min_y = min_y;
    out_state->max_y = max_y;""")

code = code.replace("""    if (min_x < map_vp->offset_x ||
        max_x >= map_vp->offset_x + map_vp->width ||
        min_y < map_vp->offset_y ||
        max_y >= map_vp->offset_y + map_vp->height) {
        return;
    }

    /*
     * Preserve the caller's foreground colour.
     */
    gfx_color_t old_fg = gfx->color_fg;

    gfx_set_color(gfx, BLACK, gfx->color_bg);

    if (filled) {
        gfx_fill_polygon(gfx, pts, point_count);
    } else {
        gfx_draw_polygon(gfx, pts, point_count);
    }

    gfx_set_color(gfx, old_fg, gfx->color_bg);
}""", """    if (min_x < map_vp->offset_x ||
        max_x >= map_vp->offset_x + map_vp->width ||
        min_y < map_vp->offset_y ||
        max_y >= map_vp->offset_y + map_vp->height) {
        return;
    }

    out_state->rendered = true;
}

static void ui_draw_marker(gfx_context_t* gfx, const marker_state_t* state) {
    if (!state->rendered) return;

    /*
     * Preserve the caller's foreground colour.
     */
    gfx_color_t old_fg = gfx->color_fg;

    gfx_set_color(gfx, BLACK, gfx->color_bg);

    if (state->filled) {
        gfx_fill_polygon(gfx, (gfx_point_t*)state->pts, state->point_count);
    } else {
        gfx_draw_polygon(gfx, (gfx_point_t*)state->pts, state->point_count);
    }

    gfx_set_color(gfx, old_fg, gfx->color_bg);
}""")

with open("src/core/ui/ui_map.c", "w") as f:
    f.write(code)
