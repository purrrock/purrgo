#include "ui/app_data_change.h"

#include "purrgo/geo.h"

void purrgo_app_data_change_reset(purrgo_app_data_change_t *dc) {
    dc->pos_valid = false;
    /* marker_valid сохраняется, см. комментарий в заголовке. */
}

void purrgo_app_data_change_forget_pos(purrgo_app_data_change_t *dc) {
    dc->pos_valid = false;
}

void purrgo_app_data_change_forget_marker(purrgo_app_data_change_t *dc) {
    dc->marker_valid = false;
}

void purrgo_app_data_change_marker_rendered(purrgo_app_data_change_t *dc,
                                            const purrgo_gnss_solution_t *rendered_fix) {
    if (rendered_fix && rendered_fix->valid) {
        dc->marker_lat_1e7 = rendered_fix->lat_1e7;
        dc->marker_lon_1e7 = rendered_fix->lon_1e7;
        dc->marker_valid = true;
    } else {
        dc->marker_valid = false;
    }
}

bool purrgo_app_data_change_pos_moved(const purrgo_app_data_change_t *dc, bool on_map,
                                      const purrgo_gnss_solution_t *fix) {
    const bool ref_valid = on_map ? dc->marker_valid : dc->pos_valid;
    const int32_t ref_lat = on_map ? dc->marker_lat_1e7 : dc->lat_1e7;
    const int32_t ref_lon = on_map ? dc->marker_lon_1e7 : dc->lon_1e7;

    if (!fix->valid) {
        // Позиция пропала: нужно убрать маркер/обновить статус
        return ref_valid;
    }
    if (!ref_valid) {
        return true;
    }
    return purrgo_geo_distance_m(ref_lat, ref_lon, fix->lat_1e7, fix->lon_1e7) >=
           PURRGO_GNSS_MARKER_UPDATE_DISTANCE_M;
}

bool purrgo_app_data_change_trip_changed(purrgo_app_data_change_t *dc,
                                         const purrgo_gnss_solution_t *fix,
                                         const purrgo_wpt_nav_t *wn) {
    bool changed = false;

    int32_t dv = fix->speed_knots - dc->speed_knots_100;
    int32_t da = fix->alt_m - dc->alt_m;
    if (dv < 0) dv = -dv;
    if (da < 0) da = -da;

    if (dv >= PURRGO_UI_SPEED_THRESHOLD_KNOTS_100 ||
        da >= PURRGO_UI_ALT_THRESHOLD_M ||
        fix->satellites_tracked != dc->satellites) {
        changed = true;
    }

    int32_t dd = (int32_t)wn->nav.distance_to_wp_m - (int32_t)dc->wpt_dist_m;
    int32_t db = (int32_t)wn->nav.bearing_to_wp_deg - (int32_t)dc->wpt_bearing_deg;
    if (dd < 0) dd = -dd;
    if (db < 0) db = -db;
    if (db > 180) db = 360 - db;

    if (wn->valid != dc->wpt_valid ||
        (wn->valid && (wn->nav.is_arrived != dc->wpt_arrived ||
                       dd >= PURRGO_UI_WPT_DIST_THRESHOLD_M ||
                       db >= PURRGO_UI_WPT_BEARING_THRESHOLD_DEG))) {
        changed = true;
        dc->wpt_valid = wn->valid;
        dc->wpt_arrived = wn->nav.is_arrived;
        dc->wpt_dist_m = wn->nav.distance_to_wp_m;
        dc->wpt_bearing_deg = wn->nav.bearing_to_wp_deg;
    }

    return changed;
}

void purrgo_app_data_change_commit(purrgo_app_data_change_t *dc,
                                   const purrgo_gnss_solution_t *fix) {
    dc->speed_knots_100 = fix->speed_knots;
    dc->alt_m = fix->alt_m;
    dc->satellites = fix->satellites_tracked;
    dc->pos_valid = fix->valid;
    dc->lat_1e7 = fix->lat_1e7;
    dc->lon_1e7 = fix->lon_1e7;
}
