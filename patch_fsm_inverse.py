import re

with open("src/core/app_fsm.c", "r") as f:
    content = f.read()

search = """        int64_t geo_width = camera_span_x(&dynamic_cam);
        int64_t geo_height = (int64_t)dynamic_cam.max_y - (int64_t)dynamic_cam.min_y;

        int64_t delta_lon = ((int64_t)(sx - target_x) * geo_width) / map_vp.width;
        int64_t delta_lat = ((int64_t)(target_y - sy) * geo_height) / map_vp.height;

        int64_t candidate_lon = (int64_t)map_center_lon_1e7 + delta_lon;
        int64_t candidate_lat = (int64_t)map_center_lat_1e7 + delta_lat;

        if (candidate_lon > INT32_MAX) candidate_lon = INT32_MAX;
        if (candidate_lon < INT32_MIN) candidate_lon = INT32_MIN;
        if (candidate_lat > 900000000LL) candidate_lat = 900000000LL;
        if (candidate_lat < -900000000LL) candidate_lat = -900000000LL;"""

replace = """        // Exact inverse projection from GNSS coordinate to find new camera center
        int64_t geo_height = (int64_t)dynamic_cam.max_y - (int64_t)dynamic_cam.min_y;
        int64_t rad_y = geo_height / 2;
        int64_t Y_req = (int64_t)map_vp.height + map_vp.offset_y - target_y;
        int64_t candidate_lat = (int64_t)fix->lat_1e7 + rad_y - (Y_req * geo_height) / map_vp.height;

        if (candidate_lat > 900000000LL) candidate_lat = 900000000LL;
        if (candidate_lat < -900000000LL) candidate_lat = -900000000LL;

        // Calculate exact width at the new latitude
        purrgo_bbox_t temp_cam;
        purrgo_geo_bbox_from_center(
            (int32_t)candidate_lat,
            map_center_lon_1e7, // Lon doesn't matter for width
            purrgo_app_get_map_scale_width_m(),
            &map_vp,
            &temp_cam
        );

        int64_t geo_width = camera_span_x(&temp_cam);
        int64_t rad_x = geo_width / 2;
        int64_t X_req = (int64_t)target_x - map_vp.offset_x;
        int64_t candidate_lon = (int64_t)fix->lon_1e7 + rad_x - (X_req * geo_width) / map_vp.width;

        if (candidate_lon > INT32_MAX) candidate_lon = INT32_MAX;
        if (candidate_lon < INT32_MIN) candidate_lon = INT32_MIN;"""

new_content = content.replace(search, replace)
if content == new_content:
    print("Failed to patch app_fsm.c")
else:
    with open("src/core/app_fsm.c", "w") as f:
        f.write(new_content)
    print("Patched app_fsm.c")
