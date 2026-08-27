import re

with open("src/core/app_fsm.c", "r") as f:
    content = f.read()

# Add integer rounding in candidate_lon and candidate_lat calculations.
search = """        int64_t y_term = (int64_t)map_vp.height - target_y + map_vp.offset_y;
        int64_t candidate_lat = (int64_t)fix->lat_1e7 - (y_term * geo_height) / map_vp.height + rad_y;"""

replace = """        int64_t y_term = (int64_t)map_vp.height - target_y + map_vp.offset_y;
        int64_t candidate_lat = (int64_t)fix->lat_1e7 - (y_term * geo_height + map_vp.height / 2) / map_vp.height + rad_y;"""
content = content.replace(search, replace)

search2 = """        int64_t x_term = (int64_t)target_x - map_vp.offset_x;
        int64_t candidate_lon = (int64_t)fix->lon_1e7 - (x_term * geo_width) / map_vp.width + rad_x;"""

replace2 = """        int64_t x_term = (int64_t)target_x - map_vp.offset_x;
        int64_t candidate_lon = (int64_t)fix->lon_1e7 - (x_term * geo_width + map_vp.width / 2) / map_vp.width + rad_x;"""
content = content.replace(search2, replace2)

# Also let's relax the validation by 1 pixel to absorb rounding noise safely without failing back.
search3 = """        if (new_dx <= follow_start_x && new_dy <= follow_start_y) {"""
replace3 = """        if (new_dx <= follow_start_x + 1 && new_dy <= follow_start_y + 1) {"""
content = content.replace(search3, replace3)

with open("src/core/app_fsm.c", "w") as f:
    f.write(content)
