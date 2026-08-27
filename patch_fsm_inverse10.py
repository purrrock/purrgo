import re

with open("tests/core/test_app_fsm.c", "r") as f:
    content = f.read()

# Why did it STILL fail?
# Because app_config is declared extern? Yes, it's global in config.c.
# But purrgo_app_init reads from PURRGO.CFG using purrgo_config_load()!
# And purrgo_config_load overwrites app_config!
# So setting app_config and then calling purrgo_app_init() doesn't work!
# Let's write a mock for purrgo_config_load or just set it AFTER purrgo_app_init.
# Wait, we can't set it after purrgo_app_init because map_center_lat_1e7 is static in app_fsm.c!

search = """    app_config.last_lat_1e7 = lat;
    app_config.last_lon_1e7 = lon;
    purrgo_app_init(); // re-init to apply the config"""

replace = """    // purrgo_app_init reads from config. So we must set it and ensure it's loaded, but we don't have file access.
    // However, purrgo_app_init() just calls purrgo_config_load(), which uses defaults if file is missing.
    // If file is missing, it sets last_lat/lon to 0.
    // If we want it to be lat, lon, we can pan manually?
    // Wait, earlier tests like test_pan_small_scale did:
    // setup_test_state(0, 0);
    // And it worked? Yes, because default is 0!
    // But for test_pan_coordinate_bounds_clamping, it does setup_test_state(2000000000, 2000000000).
    // How did that work before?
    // Because purrgo_app_update(&fix) triggered exact center!
    // So BEFORE, setup_test_state worked entirely via auto-follow exact centering!
    // Since auto-follow no longer exact centers, setup_test_state is BROKEN for any coords outside safe zone!
"""

content = content.replace(search, replace)
# Let's fix setup_test_state by manipulating the map center manually.
# But there is no setter for map center!
# We could add one, or we can just send multiple purrgo_app_update() calls to walk it there?
# Actually, if we send a fix that is VERY FAR, it will jump by `width`.
# We can just use the manual pan button! But manual pan button takes many presses.

# Or, wait, if we send purrgo_app_update with the target lat/lon, it moves the camera to the opposite side.
# Which means it does move the camera!
# If we just expose a setter for tests. But we don't want to modify app_fsm.h just for this.
# Wait, why was `test_map_dirty_state` failing at `assert(purrgo_app_get_map_center_lon() == 0);`?
# In `test_map_dirty_state`, it calls `setup_test_state(0, 0, PURRGO_MAP_SCALE_10KM);`
# The default config sets it to 0, 0! So `purrgo_app_init()` sets it to 0, 0!
# Then `fix.lat = 0; fix.lon = 0; purrgo_app_update(&fix);`
# `dx = 0, dy = 0`. It is INSIDE FOLLOW_STOP zone! So no camera change!
# So it SHOULD REMAIN 0, 0!
# Why did it change from 0, 0 in test_map_dirty_state when dist_stop_zone was used???
# Ah!
# Let's print out what `purrgo_app_get_map_center_lon()` actually is when it fails!
