import re

with open("src/core/app_fsm.c", "r") as f:
    content = f.read()

# Add a test-only setter
setter = """
void purrgo_app_set_map_center_for_test(int32_t lat, int32_t lon) {
    map_center_lat_1e7 = lat;
    map_center_lon_1e7 = lon;
    map_dirty = true;
}
"""

if "purrgo_app_set_map_center_for_test" not in content:
    content += setter
    with open("src/core/app_fsm.c", "w") as f:
        f.write(content)

with open("tests/core/test_app_fsm.c", "r") as f:
    test_content = f.read()

# Update setup_test_state to use it
search = """    purrgo_gnss_solution_t fix = {0};
    fix.valid = true;
    fix.lat_1e7 = lat;
    fix.lon_1e7 = lon;

    purrgo_app_update(&fix);"""

replace = """    extern void purrgo_app_set_map_center_for_test(int32_t lat, int32_t lon);
    purrgo_app_set_map_center_for_test(lat, lon);"""

test_content = test_content.replace(search, replace)
with open("tests/core/test_app_fsm.c", "w") as f:
    f.write(test_content)

print("Patched setup_test_state")
