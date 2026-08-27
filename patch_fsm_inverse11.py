import re

with open("tests/core/test_app_fsm.c", "r") as f:
    content = f.read()

search = """    assert(purrgo_app_map_is_dirty() == false); // Should remain clean, inside STOP
    assert(purrgo_app_get_map_center_lon() == 0);"""

replace = """    assert(purrgo_app_map_is_dirty() == false); // Should remain clean, inside STOP
    if (purrgo_app_get_map_center_lon() != 0) {
        printf("FAILED! Map center lon is %d\\n", purrgo_app_get_map_center_lon());
    }
    assert(purrgo_app_get_map_center_lon() == 0);"""

content = content.replace(search, replace)
with open("tests/core/test_app_fsm.c", "w") as f:
    f.write(content)
