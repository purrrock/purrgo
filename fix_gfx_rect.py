content = open("tests/core/test_gfx_rect.c").read()

bad1 = """static int pixels_drawn = 0;
static int draw_calls = 0;
static int pixels_drawn = 0;"""
good1 = """static int pixels_drawn = 0;
static int draw_calls = 0;"""

bad2 = """static void reset_test_state(gfx_context_t *ctx) {
    memset(framebuffer, 0, sizeof(framebuffer));
    pixels_drawn = 0;
    draw_calls = 0;
    ctx->color_bg = 2; // Use a specific color for fill
    ctx->color_fg = 1; // Use a specific color for draw
    gfx_set_clip(ctx, 10, 10, 80, 80); // Set a clipping region
static void reset_framebuffer(gfx_context_t *ctx) {"""
good2 = """static void reset_framebuffer(gfx_context_t *ctx) {"""

bad3 = """    if (framebuffer[y * WIDTH + x] != expected) {
        printf("FAILED: Pixel at (%d, %d) is %d, expected %d\\n", x, y, framebuffer[y * WIDTH + x], expected);
        return false;
    }
    if (framebuffer[y * WIDTH + x] != expected) {
        printf("FAILED: Pixel at (%d, %d) is %d, expected %d\\n", x, y, framebuffer[y * WIDTH + x], expected);
        return false;
    }"""
good3 = """    if (framebuffer[y * WIDTH + x] != expected) {
        printf("FAILED: Pixel at (%d, %d) is %d, expected %d\\n", x, y, framebuffer[y * WIDTH + x], expected);
        return false;
    }"""

bad4 = """    // Width 20, Height 15 => 20*2 + 13*2 = 40 + 26 = 66 pixels.
    if (draw_calls != 66) {
        printf("FAILED test_draw_rect_basic: Expected 66 draw calls, got %d\\n", draw_calls);
        passed = false;
    }
    if (pixels_drawn != 66) {
        printf("FAILED test_draw_rect_basic: Expected 66 pixels drawn, got %d\\n", pixels_drawn);
    // 10 + 10 + 3 + 3 = 26
    if (pixels_drawn != 26) {
        printf("FAILED test_draw_rect_basic: Expected 26 pixels, got %d\\n", pixels_drawn);
        passed = false;
    }"""
good4 = """    // Width 10, Height 5 => 10 + 10 + 3 + 3 = 26 pixels.
    if (draw_calls != 26) {
        printf("FAILED test_draw_rect_basic: Expected 26 draw calls, got %d\\n", draw_calls);
        passed = false;
    }
    if (pixels_drawn != 26) {
        printf("FAILED test_draw_rect_basic: Expected 26 pixels, got %d\\n", pixels_drawn);
        passed = false;
    }"""

content = content.replace(bad1, good1).replace(bad2, good2).replace(bad3, good3).replace(bad4, good4)
open("tests/core/test_gfx_rect.c", "w").write(content)
