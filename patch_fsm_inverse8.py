import re

with open("tests/core/test_app_fsm.c", "r") as f:
    content = f.read()

# Let's fix test_auto_follow_opposite_side in test_app_fsm.c!
# And let's find out why map_center_lon_1e7 could be non-zero in test_map_dirty_state.
# Wait, I changed tests/core/test_app_fsm.c in a previous step to:
# assert(purrgo_app_get_map_center_lon() != dist_start_zone);
# assert(purrgo_app_get_map_center_lon() != 0);

# Ah! In test_auto_follow_opposite_side, I called purrgo_app_update(&fix) when fix.lon_1e7 = dist_start_zone.
# This modifies map_center_lon_1e7.
# Then I test test_map_clean_refresh_skips_render().
# Then in test_map_dirty_state, wait, test_auto_follow_opposite_side is run LAST!
# So test_map_dirty_state is failing on line 173 BEFORE test_auto_follow_opposite_side even runs!

# Why does line 173 fail?
# Line 173 is: assert(purrgo_app_get_map_center_lon() == 0);
# But wait, earlier in the same test:
# purrgo_app_handle_button(PURRGO_BTN_UP); // sets manual pan
# purrgo_gnss_solution_t fix = {0}; fix.valid = true;
# setup_test_state(0, 0, PURRGO_MAP_SCALE_10KM); -> this calls purrgo_app_init()! Which resets current_state, draft_tz, and map_center!
# purrgo_app_map_clear_dirty();
# ...
# fix.lon_1e7 = dist_stop_zone; fix.lat_1e7 = 0; purrgo_app_update(&fix);
# Then assert(purrgo_app_get_map_center_lon() == 0);
# If it fails here, it must be because setup_test_state did NOT set it to 0!
# Why wouldn't setup_test_state(0, 0) set it to 0?
# Let's look at setup_test_state:
#     purrgo_app_init();
#     purrgo_gnss_solution_t fix = {0};
#     fix.valid = true;
#     fix.lat_1e7 = lat;
#     fix.lon_1e7 = lon;
#     purrgo_app_update(&fix); // THIS TRIGGERS AUTO FOLLOW!!!
# Because map_center is initialized to app_config.last_lat_1e7 etc in purrgo_app_init()!
# And what is app_config? It might be whatever was saved!
# But wait, in apply_auto_follow, if the config had it at some value, and we update with 0,0,
# it might trigger auto_follow and set it to 0,0.
# UNLESS the difference is between STOP and START! Then it doesn't move it to 0!
# Or wait, if we changed the auto-follow logic from "recenter exactly" to "opposite side",
# now when setup_test_state calls update(&fix), if the distance is outside START,
# it will move the camera to the *opposite side*, NOT exactly to fix (0,0)!
# THAT IS WHY!
