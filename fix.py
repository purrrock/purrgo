content = open("tests/core/test_gfx_text.c").read()

bad1 = """static bool test_out_of_bounds() {
    printf("Running test_out_of_bounds...\\n");
static bool test_single_char_string() {"""
good1 = """static bool test_out_of_bounds() {
    printf("Running test_out_of_bounds...\\n");
    return true;
}
static bool test_single_char_string() {"""
content = content.replace(bad1, good1)

bad2 = """static bool test_extended_ascii() {
    printf("Running test_extended_ascii...\\n");
    gfx_draw_string(&ctx, 10, 10, "A");"""
good2 = """static bool test_extended_ascii() {
    printf("Running test_extended_ascii...\\n");
    gfx_context_t ctx;
    gfx_init(&ctx, WIDTH, HEIGHT, framebuffer, draw_pixel, read_pixel);
    reset_test_state(&ctx);
    gfx_draw_string(&ctx, 10, 10, "A");"""
content = content.replace(bad2, good2)

import re
content = re.sub(r'static bool test_multiple_newlines\(\) \{', r'static bool test_multiple_newlines_1() {', content, count=1)
content = re.sub(r'if \(!test_multiple_newlines\(\)\) success = false;', r'if (!test_multiple_newlines_1()) success = false;', content, count=1)

open("tests/core/test_gfx_text.c", "w").write(content)
