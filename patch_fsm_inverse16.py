import re

# Look closely at test_map_dirty_state logic
# purrgo_app_handle_button(PURRGO_BTN_UP); -> this pans the map UP!
# So manual_pan_active becomes true.
# Then setup_test_state(0, 0, PURRGO_MAP_SCALE_10KM) is called!
# In setup_test_state, we did purrgo_app_init(), which sets manual_pan_active = false!
# Wait, manual pan up shifts the map lat. So map_center_lat_1e7 changes.
# Then purrgo_app_init() sets it back to app_config.last_lat_1e7.
# Then setup_test_state does:
# purrgo_app_update(&fix); (where fix.lat=0, fix.lon=0)
# THIS CALLS apply_auto_follow(&fix)!
# What does apply_auto_follow do if fix = 0, 0, and map_center = app_config.last ?
# If app_config.last is 0, 0, it doesn't move.
# BUT wait! Test before this was test_pan_coordinate_bounds_clamping() !!!
# Which called setup_test_state(-2000000000, -2000000000)!
# And THEN purrgo_config_save() might have been called?
# Wait! purrgo_app_init() calls purrgo_config_load()!
# Does test_pan_coordinate_bounds_clamping write to PURRGO.CFG?
# If purrgo_config_save is called ANYWHERE, it writes the last known coords!
# YES! The mock filesystem says purrgo_fs_open returns NULL, so purrgo_config_save fails.
# But `app_config` is a global variable!
# When test_pan_coordinate_bounds_clamping ends, `app_config.last_lat_1e7` is probably still whatever it was!
# Wait, `purrgo_config_load` sets `app_config` to defaults ONLY if it fails to read, BUT it first checks if file exists, if not it sets defaults. Wait, it only sets defaults ONCE? Let's check config.c.

with open("src/core/config.c", "r") as f:
    config_content = f.read()

print("config.c defaults:")
lines = config_content.split('\n')
for i, line in enumerate(lines):
    if "void purrgo_config_load" in line:
        for j in range(i, i+15):
            print(lines[j])
        break
