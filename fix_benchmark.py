content = open("tests/core/test_benchmark_halo.c").read()
content = content.replace("static void draw_pixel(int16_t x, int16_t y, gfx_color_t color) {", "static void draw_pixel(void *userdata, int16_t x, int16_t y, gfx_color_t color) {")
content = content.replace("static gfx_color_t read_pixel(int16_t x, int16_t y) {", "static gfx_color_t read_pixel(void *userdata, int16_t x, int16_t y) {")
open("tests/core/test_benchmark_halo.c", "w").write(content)
