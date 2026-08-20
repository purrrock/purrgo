1. *Delete `sdl_draw_text`*
   - Remove the `sdl_draw_text` function implementation from `apps/emulator/src/main.c`.
2. *Create a new draw pixel callback for SDL*
   - Implement `emulator_sdl_draw_pixel_cb` in `main.c` that maps `COLOR_BLACK` to drawing a point via `SDL_RenderDrawPoint` on the `SDL_Renderer`, and ignores `COLOR_WHITE` (background) to simulate transparency, just like the old `sdl_draw_text` did.
3. *Initialize a `gfx_context_t` for the SDL renderer*
   - In `main`, initialize a new `gfx_context_t sdl_gfx_ctx` using `gfx_init`, passing the `renderer` and `emulator_sdl_draw_pixel_cb`.
4. *Replace usage of `sdl_draw_text`*
   - Replace the `sdl_draw_text` call with a call to `gfx_set_color` and `gfx_draw_string` using `sdl_gfx_ctx`.
5. *Complete pre-commit steps*
   - Complete pre-commit steps to ensure proper testing, verification, review, and reflection are done.
6. *Submit the change*
   - Submit the branch `replace-sdl-draw-text` with an appropriate message.
