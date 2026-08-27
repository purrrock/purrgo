import re

with open("tests/core/test_app_fsm.c", "r") as f:
    content = f.read()

# Let's print out purrgo_app_get_map_center_lon() right after setup_test_state.

search = """    // Current center is modified by previous tests, so we need to reset it deterministically.
    setup_test_state(0, 0, PURRGO_MAP_SCALE_10KM);
    purrgo_app_map_clear_dirty();"""

replace = """    // Current center is modified by previous tests, so we need to reset it deterministically.
    setup_test_state(0, 0, PURRGO_MAP_SCALE_10KM);
    purrgo_app_map_clear_dirty();
    fprintf(stderr, "After setup_test_state, lon is %d\\n", purrgo_app_get_map_center_lon());"""

content = content.replace(search, replace)
with open("tests/core/test_app_fsm.c", "w") as f:
    f.write(content)
