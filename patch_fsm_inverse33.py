import re

with open("tests/core/test_app_fsm.c", "r") as f:
    content = f.read()

# Clean up debug prints
search = """    if (purrgo_app_get_map_center_lon() != 0) {
        fprintf(stderr, "FAILED! Map center lon is %d\\n", purrgo_app_get_map_center_lon());
    }"""
content = content.replace(search, "")

search2 = """    fprintf(stderr, "After setup_test_state, lon is %d\\n", purrgo_app_get_map_center_lon());"""
content = content.replace(search2, "")

with open("tests/core/test_app_fsm.c", "w") as f:
    f.write(content)

with open("src/core/app_fsm.c", "r") as f:
    content = f.read()

search3 = """            fprintf(stderr, "FALLBACK HIT! new_dx=%d, new_dy=%d, follow_start_x=%d, follow_start_y=%d\\n", new_dx, new_dy, follow_start_x, follow_start_y);"""
content = content.replace(search3, "")

with open("src/core/app_fsm.c", "w") as f:
    f.write(content)
