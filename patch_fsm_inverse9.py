import re

with open("tests/core/test_app_fsm.c", "r") as f:
    content = f.read()

# setup_test_state should forcefully set the center if we want to ensure it's (lat, lon).
# But there is no setter.
# So instead of relying on auto-follow to exact-center, we can just edit setup_test_state!

search = """void setup_test_state(int32_t lat, int32_t lon, purrgo_map_scale_t scale) {
    purrgo_app_init();

    // Simulate setting internal state (not directly exposed via public API setters for these tests,
    // but we can manipulate config to some extent and rely on initial values if needed,
    // or just let the button handlers move it and verify).
    // Let's use GPS fix update to set position initially.
    purrgo_gnss_solution_t fix = {0};
    fix.valid = true;
    fix.lat_1e7 = lat;
    fix.lon_1e7 = lon;

    purrgo_app_update(&fix);"""

replace = """void setup_test_state(int32_t lat, int32_t lon, purrgo_map_scale_t scale) {
    purrgo_app_init();

    // We must ensure the camera is exactly at (lat, lon).
    // Since auto-follow no longer centers exactly, we should just pan until we're close,
    // or better, manipulate the config before init!
    app_config.last_lat_1e7 = lat;
    app_config.last_lon_1e7 = lon;
    purrgo_app_init(); // re-init to apply the config

    purrgo_gnss_solution_t fix = {0};
    fix.valid = true;
    fix.lat_1e7 = lat;
    fix.lon_1e7 = lon;

    purrgo_app_update(&fix);"""

content = content.replace(search, replace)
if content != replace:
    print("Replaced!")
with open("tests/core/test_app_fsm.c", "w") as f:
    f.write(content)
