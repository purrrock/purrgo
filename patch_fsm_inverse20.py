import re

with open("tests/core/test_app_fsm.c", "r") as f:
    content = f.read()

# Wait, if purrgo_app_get_map_center_lon() changed, WHEN did it change?
# Is it possible that `setup_test_state` did NOT change it to 0,0??
# Let's check test_app_fsm.c setup_test_state!

# In setup_test_state:
#     purrgo_app_set_map_center_for_test(lat, lon);
# This sets map_center_lat_1e7 = lat, map_center_lon_1e7 = lon.
# Then purrgo_gnss_solution_t fix = {0}; fix.valid = true; fix.lat_1e7 = lat; fix.lon_1e7 = lon;
# Then purrgo_app_update(&fix);
# Then it returns.
# Wait! In setup_test_state, we call purrgo_app_handle_button(PURRGO_BTN_PLUS) and MINUS!
# These set map_dirty = true, but do they modify map_center_lat_1e7?
# No!
# Let's add a print statement to setup_test_state to verify what map_center is at the end of it.

search = """void setup_test_state(int32_t lat, int32_t lon, purrgo_map_scale_t scale) {"""

replace = """void setup_test_state(int32_t lat, int32_t lon, purrgo_map_scale_t scale) {"""
content = content.replace(search, replace) # Noop
with open("tests/core/test_app_fsm.c", "w") as f:
    f.write(content)
