content = open("tests/core/test_gfx_rect.c").read()

bad1 = """    if (!check_pixel(10, 20, 0)) passed = false;

    if (draw_calls != 40) {
        printf("FAILED test_draw_rect_basic: Expected 40 draw calls, got %d\\n", draw_calls);
        passed = false;
    }

    if (!passed) {
        printf("FAILED test_draw_rect_basic\\n");
    } else {
        printf("PASSED test_draw_rect_basic\\n");
    }
    return passed;
"""
if bad1 not in content:
    print("bad1 not found!")

# wait, I can just restore it to HEAD and apply my halo optimization again since `test_gfx_rect.c` was untouched by me. No wait, maybe another branch corrupted it.
