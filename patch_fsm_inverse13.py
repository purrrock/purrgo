import re

with open("tests/core/test_app_fsm.c", "r") as f:
    content = f.read()

# Let's remove the print and assert because if printf isn't showing, it might be buffered.
search = """    if (purrgo_app_get_map_center_lon() != 0) {
        printf("FAILED! Map center lon is %d\\n", purrgo_app_get_map_center_lon());
    }
    assert(purrgo_app_get_map_center_lon() == 0);"""

replace = """    if (purrgo_app_get_map_center_lon() != 0) {
        fprintf(stderr, "FAILED! Map center lon is %d\\n", purrgo_app_get_map_center_lon());
    }
    assert(purrgo_app_get_map_center_lon() == 0);"""

content = content.replace(search, replace)
with open("tests/core/test_app_fsm.c", "w") as f:
    f.write(content)
