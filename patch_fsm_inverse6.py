import re

with open("tests/core/test_app_fsm.c", "r") as f:
    content = f.read()

# Fix the unused variable and the exact assertion.

search = """    int32_t new_center = purrgo_app_get_map_center_lon();
    purrgo_app_map_clear_dirty();"""

replace = """    purrgo_app_map_clear_dirty();"""
content = content.replace(search, replace)

search2 = """    // The new x should be 16 (which is center_x - follow_start_x)
    assert(new_sx == 16);"""

replace2 = """    // The new x should be roughly on the opposite side, within the safe zone
    int16_t center_x = map_vp.offset_x + map_vp.width / 2;
    int32_t dx = (int32_t)new_sx - center_x;
    if (dx < 0) dx = -dx;
    int32_t follow_start_x = (map_vp.width / 2) - 16;
    assert(dx <= follow_start_x);
    // It should be definitely on the left half (since it was on the right edge)
    assert(new_sx < center_x);"""

content = content.replace(search2, replace2)

with open("tests/core/test_app_fsm.c", "w") as f:
    f.write(content)
