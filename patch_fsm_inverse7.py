import re

with open("tests/core/test_app_fsm.c", "r") as f:
    content = f.read()

# The failure in test_app_fsm is that purrgo_app_get_map_center_lon() is not 0
# for the dist_stop_zone test.
# Why wouldn't it be 0? It must have gone into the apply_auto_follow else block and updated the map center!
# Why would it go into the else block?
# dx = abs(sx - center_x).
# If fix.lon_1e7 = dist_stop_zone = geo_width / 32, then:
# projected_x = (lon - (-geo_width/2)) * vp_width / geo_width + vp_offset_x
# = (geo_width/32 + geo_width/2) * vp_width / geo_width
# = (1/32 + 1/2) * vp_width
# = (17/32) * vp_width.
# center_x = vp_width / 2 = 16/32 * vp_width.
# So dx = 1/32 * vp_width.
# follow_stop_x = vp_width / AUTO_FOLLOW_STOP_MARGIN_DIV (which is 16).
# So follow_stop_x = 1/16 * vp_width = 2/32 * vp_width.
# dx (1/32) <= follow_stop_x (2/32) should be true!
# So why did it go into the else block?
# Because dy!
# In the test: fix.lat_1e7 = 0;
# What was the original map_center_lat_1e7?
# setup_test_state(0, 0, PURRGO_MAP_SCALE_10KM);
# So it's 0.
# So projected_y = vp.height - (0 - (-geo_height/2)) * vp.height / geo_height
# = vp.height - (geo_height/2) * vp.height / geo_height
# = vp.height - vp.height / 2 = vp.height / 2.
# So sy = center_y. dy = 0.
# So dy <= follow_stop_y is true!
# Then WHY did it fail?
# Ah! Look at the first test in test_map_dirty_state:
#     assert(purrgo_app_get_map_center_lon() == 0); // Line 173

# Maybe setup_test_state doesn't correctly set center to 0?
# Let's check setup_test_state definition.
