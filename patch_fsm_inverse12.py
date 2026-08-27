import re

with open("tests/core/test_app_fsm.c", "r") as f:
    content = f.read()

search = """    if (purrgo_app_get_map_center_lon() != 0) {
        printf("FAILED! Map center lon is %d\\n", purrgo_app_get_map_center_lon());
    }"""

replace = """    if (purrgo_app_get_map_center_lon() != 0) {
        printf("FAILED! Map center lon is %d\\n", purrgo_app_get_map_center_lon());
    }"""
content = content.replace(search, replace) # Noop, just to check my print statement

with open("tests/core/test_app_fsm.c", "w") as f:
    f.write(content)
