import re

with open("src/core/app_fsm.c", "r") as f:
    content = f.read()

# Let's completely replace the candidate calculation in apply_auto_follow.
# We want to find candidate_lat, candidate_lon such that if we center at candidate_lat, candidate_lon,
# then project_to_screen(fix.lat, fix.lon) gives exactly target_x, target_y.

# From project_to_screen:
# projected_x = (lon - cam.min_x) * vp.width / width + vp.offset_x
# projected_y = vp.height - (lat - cam.min_y) * vp.height / height + vp.offset_y
# where width = camera_span_x(cam) and height = cam.max_y - cam.min_y

# Notice that cam.min_x = candidate_lon - width/2
# So projected_x = (lon - (candidate_lon - width/2)) * vp.width / width + vp.offset_x
# (projected_x - vp.offset_x) * width / vp.width = lon - candidate_lon + width/2
# candidate_lon = lon + width/2 - (projected_x - vp.offset_x) * width / vp.width

search = """        // Exact inverse projection from GNSS coordinate to find new camera center
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

replace = """        // Exact inverse projection from GNSS coordinate to find new camera center
        // From project_to_screen:
        // sy = map_vp.height - (lat - cam.min_y) * map_vp.height / height + map_vp.offset_y
        // target_y - map_vp.offset_y = map_vp.height - (fix->lat_1e7 - (candidate_lat - height/2)) * map_vp.height / height
        // Let Y_diff = map_vp.height - (target_y - map_vp.offset_y)
        // Y_diff * height / map_vp.height = fix->lat_1e7 - candidate_lat + height/2
        // candidate_lat = fix->lat_1e7 + height/2 - Y_diff * height / map_vp.height

        int64_t geo_height = (int64_t)dynamic_cam.max_y - (int64_t)dynamic_cam.min_y;
        int64_t rad_y = geo_height / 2;
        int64_t Y_diff = (int64_t)map_vp.height - ((int64_t)target_y - map_vp.offset_y);
        int64_t candidate_lat = (int64_t)fix->lat_1e7 + rad_y - (Y_diff * geo_height) / map_vp.height;

        if (candidate_lat > 900000000LL) candidate_lat = 900000000LL;
        if (candidate_lat < -900000000LL) candidate_lat = -900000000LL;

        // Calculate exact width at the new latitude to handle correct longitudinal scaling
        purrgo_bbox_t temp_cam;
        purrgo_geo_bbox_from_center(
            (int32_t)candidate_lat,
            map_center_lon_1e7, // Lon doesn't matter for width calculation
            purrgo_app_get_map_scale_width_m(),
            &map_vp,
            &temp_cam
        );

        int64_t geo_width = camera_span_x(&temp_cam);
        int64_t rad_x = geo_width / 2;
        int64_t X_diff = (int64_t)target_x - map_vp.offset_x;
        int64_t candidate_lon = (int64_t)fix->lon_1e7 + rad_x - (X_diff * geo_width) / map_vp.width;

        // Wrap candidate_lon cleanly within WGS84 +/- 180 (1.8e9) if needed, or simply clamp
        // Consistent with manual panning clamping:
        if (candidate_lon > INT32_MAX) candidate_lon = INT32_MAX;
        if (candidate_lon < INT32_MIN) candidate_lon = INT32_MIN;"""

new_content = content.replace(search, replace)
if content == new_content:
    print("Failed to patch app_fsm.c")
else:
    with open("src/core/app_fsm.c", "w") as f:
        f.write(new_content)
    print("Patched app_fsm.c")
