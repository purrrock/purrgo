import re

with open("src/core/app_fsm.c", "r") as f:
    content = f.read()

# Let's fix the inverse math to be simpler.
# projected_y = (height > 0) ? ((int64_t)vp->height - (dy / height)) : 0;
# dy = (lat - min_y) * vp->height
# So: projected_y - vp->offset_y = vp->height - (lat - min_y) * vp->height / height
# vp->height - (projected_y - vp->offset_y) = (lat - min_y) * vp->height / height
# (vp->height - projected_y + vp->offset_y) * height / vp->height = lat - min_y
# min_y = lat - (vp->height - projected_y + vp->offset_y) * height / vp->height
# new_center_lat = min_y + height / 2

search = """        int64_t y_term = (int64_t)map_vp.height - target_y + map_vp.offset_y;
        int64_t candidate_lat = (int64_t)fix->lat_1e7 - (y_term * geo_height) / map_vp.height + rad_y;"""

replace = """        int64_t y_term = (int64_t)map_vp.height - target_y + map_vp.offset_y;
        int64_t candidate_lat = (int64_t)fix->lat_1e7 - (y_term * geo_height) / map_vp.height + rad_y;"""

# Wait, why was test_map_dirty_state failing?
# purrgo_app_get_map_center_lon() == 0 failed.
# This means something updated the map center earlier in the test!
# Let's look at the failing test lines:
# fix.lon_1e7 = dist_stop_zone; fix.lat_1e7 = 0; purrgo_app_update(&fix);
# assert(purrgo_app_get_map_center_lon() == 0); // FAILED!

# This means dx <= follow_stop_x && dy <= follow_stop_y failed!
# Or dx, dy calculation changed?
# I did not change dx, dy calculation in apply_auto_follow.
# Let's check apply_auto_follow dx, dy.
