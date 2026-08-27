cat << 'DIFF' | patch -p1
--- a/src/core/ui/ui_map.c
+++ b/src/core/ui/ui_map.c
@@ -364,10 +364,15 @@
     int16_t h = (state->max_y - state->min_y + 1) + 2 * margin;

     // Clip to screen to prevent buffer overflow
-    if (x < 0) { w += x; x = 0; }
-    if (y < 0) { h += y; y = 0; }
-    if (x + w > PURRGO_HW_DISPLAY_WIDTH_PX) { w = PURRGO_HW_DISPLAY_WIDTH_PX - x; }
-    if (y + h > PURRGO_HW_DISPLAY_HEIGHT_PX) { h = PURRGO_HW_DISPLAY_HEIGHT_PX - y; }
+    // Map viewport offset_y is 9 and height is HEIGHT - 18
+    // So viewport bounds are y: 9 to HEIGHT - 9.
+    int16_t map_vp_offset_y = 9;
+    int16_t map_vp_height = PURRGO_HW_DISPLAY_HEIGHT_PX - 18;
+
+    if (x < 0) { w += x; x = 0; }
+    if (y < map_vp_offset_y) { h -= (map_vp_offset_y - y); y = map_vp_offset_y; }
+    if (x + w > PURRGO_HW_DISPLAY_WIDTH_PX) { w = PURRGO_HW_DISPLAY_WIDTH_PX - x; }
+    if (y + h > map_vp_offset_y + map_vp_height) { h = map_vp_offset_y + map_vp_height - y; }

     if (w <= 0 || h <= 0 || w > MARKER_BG_CACHE_W || h > MARKER_BG_CACHE_H) {
         // Can't fit in cache or invalid size, invalidate cache
@@ -382,11 +387,21 @@
     marker_bg_cache.valid = true;

     int idx = 0;
+
+    // We need to bypass the current clipping area of gfx_context to read background
+    // because it might be clipped to the union region during a partial refresh.
+    // However, we just clamped the read rectangle to the map viewport, so we are safe
+    // to read directly.
+    int16_t old_clip_x = gfx->clip_x;
+    int16_t old_clip_y = gfx->clip_y;
+    int16_t old_clip_w = gfx->clip_w;
+    int16_t old_clip_h = gfx->clip_h;
+    gfx_reset_clip(gfx);
+
     for (int16_t cy = 0; cy < h; cy++) {
         for (int16_t cx = 0; cx < w; cx++) {
             marker_bg_cache.pixels[idx++] = gfx_read_pixel(gfx, x + cx, y + cy);
         }
     }
+
+    gfx_set_clip(gfx, old_clip_x, old_clip_y, old_clip_w, old_clip_h);

     PURRGO_LOG("MARKER BG SAVE x=%d y=%d w=%d h=%d\n", x, y, w, h);
 }
@@ -402,11 +417,17 @@
     gfx_color_t old_fg = gfx->color_fg;

     int idx = 0;
+    int16_t old_clip_x = gfx->clip_x;
+    int16_t old_clip_y = gfx->clip_y;
+    int16_t old_clip_w = gfx->clip_w;
+    int16_t old_clip_h = gfx->clip_h;
+    gfx_reset_clip(gfx);
+
     for (int16_t cy = 0; cy < marker_bg_cache.h; cy++) {
         for (int16_t cx = 0; cx < marker_bg_cache.w; cx++) {
             gfx->color_fg = marker_bg_cache.pixels[idx++];
             gfx_draw_pixel(gfx, marker_bg_cache.x + cx, marker_bg_cache.y + cy);
         }
     }
+    gfx_set_clip(gfx, old_clip_x, old_clip_y, old_clip_w, old_clip_h);

     gfx->color_fg = old_fg;
DIFF
