import re

with open("src/core/app_fsm.c", "r") as f:
    content = f.read()

# Let's fix the logic for candidate_lat and candidate_lon in apply_auto_follow.
# The original code:
# sx = (fix.lon_1e7 - cam.min_x) * map_vp.width / width + map_vp.offset_x
# sy = map_vp.height - (fix.lat_1e7 - cam.min_y) * map_vp.height / height + map_vp.offset_y

# Inverse: we want target_x, target_y.
# map_vp.width and map_vp.height are integers.
# Let's simply write an exact inverse of project_to_screen.
# project_to_screen is:
# projected_x = (lon - min_x) * vp.width / width + vp.offset_x
# projected_y = vp.height - (lat - min_y) * vp.height / height + vp.offset_y

# So to solve for the new camera center given we want GNSS (lat, lon) to land at target_x, target_y:
# Let's look at what we actually need. We need min_x and min_y.
# From the Y equation:
# (target_y - vp.offset_y) = vp.height - (lat - min_y) * vp.height / height
# (lat - min_y) * vp.height / height = vp.height - (target_y - vp.offset_y)
# lat - min_y = (vp.height - target_y + vp.offset_y) * height / vp.height
# min_y = lat - (vp.height - target_y + vp.offset_y) * height / vp.height
# The new camera center is min_y + height/2.
# So candidate_lat = lat - (vp.height - target_y + vp.offset_y) * height / vp.height + height/2

# Let's check this calculation:
search = """        int64_t geo_height = (int64_t)dynamic_cam.max_y - (int64_t)dynamic_cam.min_y;
        int64_t rad_y = geo_height / 2;
        int64_t Y_diff = (int64_t)map_vp.height - ((int64_t)target_y - map_vp.offset_y);
        int64_t candidate_lat = (int64_t)fix->lat_1e7 + rad_y - (Y_diff * geo_height) / map_vp.height;"""

replace = """        int64_t geo_height = (int64_t)dynamic_cam.max_y - (int64_t)dynamic_cam.min_y;
        int64_t rad_y = geo_height / 2;
        // From project_to_screen:
        // projected_y = vp.height - (lat - min_y) * vp.height / height + vp.offset_y
        // We want projected_y == target_y
        // target_y - vp.offset_y = vp.height - (lat - min_y) * vp.height / height
        // (lat - min_y) * vp.height / height = vp.height - target_y + vp.offset_y
        // lat - min_y = (vp.height - target_y + vp.offset_y) * height / vp.height
        // min_y = lat - (vp.height - target_y + vp.offset_y) * height / vp.height
        // new_center_y = min_y + height/2
        // new_center_y = lat - (vp.height - target_y + vp.offset_y) * height / vp.height + height/2

        int64_t y_term = (int64_t)map_vp.height - target_y + map_vp.offset_y;
        int64_t candidate_lat = (int64_t)fix->lat_1e7 - (y_term * geo_height) / map_vp.height + rad_y;"""

content = content.replace(search, replace)

# From X equation:
# projected_x = (lon - min_x) * vp.width / width + vp.offset_x
# target_x - vp.offset_x = (lon - min_x) * vp.width / width
# (target_x - vp.offset_x) * width / vp.width = lon - min_x
# min_x = lon - (target_x - vp.offset_x) * width / vp.width
# new_center_x = min_x + width/2
# new_center_x = lon - (target_x - vp.offset_x) * width / vp.width + width/2

search2 = """        int64_t geo_width = camera_span_x(&temp_cam);
        int64_t rad_x = geo_width / 2;
        int64_t X_diff = (int64_t)target_x - map_vp.offset_x;
        int64_t candidate_lon = (int64_t)fix->lon_1e7 + rad_x - (X_diff * geo_width) / map_vp.width;"""

replace2 = """        int64_t geo_width = camera_span_x(&temp_cam);
        int64_t rad_x = geo_width / 2;
        int64_t x_term = (int64_t)target_x - map_vp.offset_x;
        int64_t candidate_lon = (int64_t)fix->lon_1e7 - (x_term * geo_width) / map_vp.width + rad_x;"""

content = content.replace(search2, replace2)

with open("src/core/app_fsm.c", "w") as f:
    f.write(content)
print("Patched app_fsm.c logic")
