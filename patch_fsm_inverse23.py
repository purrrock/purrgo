import re

with open("tests/core/test_app_fsm.c", "r") as f:
    content = f.read()

# Wait, my patch_fsm_inverse18.py didn't replace it correctly because the string was slightly different!
# In patch_fsm_inverse18:
# search = "purrgo_gnss_solution_t fix = {0};\n    fix.valid = true;\n    fix.lat_1e7 = lat;\n    fix.lon_1e7 = lon;\n    \n    purrgo_app_update(&fix);"
# But the file actually had a comment in between, or something.
# Let's fix setup_test_state properly this time!

search = """    purrgo_gnss_solution_t fix = {0};
    fix.valid = true;
    fix.lat_1e7 = lat;
    fix.lon_1e7 = lon;

    // To set scale, we can just press MINUS or PLUS from default scale.
    // Default is 500m (index 5)

    purrgo_app_update(&fix);"""

replace = """    extern void purrgo_app_set_map_center_for_test(int32_t lat, int32_t lon);
    purrgo_app_set_map_center_for_test(lat, lon);"""

content = content.replace(search, replace)
if content == replace:
    print("FAILED TO PATCH!")
else:
    with open("tests/core/test_app_fsm.c", "w") as f:
        f.write(content)
    print("Patched setup_test_state successfully")
