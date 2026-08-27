import re

with open("src/core/app_fsm.c", "r") as f:
    content = f.read()

# Let's clean up the comments in app_fsm.c
search = """        // Exact inverse projection from GNSS coordinate to find new camera center
        // From project_to_screen:
        // sy = map_vp.height - (lat - cam.min_y) * map_vp.height / height + map_vp.offset_y
        // target_y - map_vp.offset_y = map_vp.height - (fix->lat_1e7 - (candidate_lat - height/2)) * map_vp.height / height
        // Let Y_diff = map_vp.height - (target_y - map_vp.offset_y)
        // Y_diff * height / map_vp.height = fix->lat_1e7 - candidate_lat + height/2
        // candidate_lat = fix->lat_1e7 + height/2 - Y_diff * height / map_vp.height

        int64_t geo_height = (int64_t)dynamic_cam.max_y - (int64_t)dynamic_cam.min_y;
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

        int64_t y_term = (int64_t)map_vp.height - target_y + map_vp.offset_y;"""

replace = """        // Exact inverse projection from GNSS coordinate to find new camera center
        // From project_to_screen:
        // projected_y = vp.height - (lat - min_y) * vp.height / height + vp.offset_y
        // By substituting min_y = candidate_lat - rad_y and solving for candidate_lat:

        int64_t geo_height = (int64_t)dynamic_cam.max_y - (int64_t)dynamic_cam.min_y;
        int64_t rad_y = geo_height / 2;
        int64_t y_term = (int64_t)map_vp.height - target_y + map_vp.offset_y;"""

content = content.replace(search, replace)
with open("src/core/app_fsm.c", "w") as f:
    f.write(content)
