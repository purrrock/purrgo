import re

with open("apps/emulator/src/main.c", "r") as f:
    data = f.read()

start_marker = "switch (purrgo_app_get_state()) {"
end_marker = "render_fb_to_texture(fb_texture);"
start_idx = data.find(start_marker)
end_idx = data.find(end_marker, start_idx)

switch_block = data[start_idx:end_idx].strip()

# Find the map definitions inside main
fixed_cam_start = data.find("purrgo_bbox_t fixed_cam = {")
fixed_cam_end = data.find("};", fixed_cam_start) + 2
fixed_cam_block = data[fixed_cam_start:fixed_cam_end]

map_vp_start = data.find("purrgo_viewport_t map_vp = {")
map_vp_end = data.find("};", map_vp_start) + 2
map_vp_block = data[map_vp_start:map_vp_end]

app_ui_c_content = f"""#include <purrgo/app_ui.h>
#include <purrgo/app_fsm.h>
#include <purrgo/gfx_text.h>
#include <purrgo/map.h>
#include <purrgo/fs_hal.h>
#include <purrgo/config.h>
#include "emu_fs.h"
#include <stdio.h>
#include <string.h>

void purrgo_app_ui_render(
    gfx_context_t* gfx,
    const purrgo_gnss_solution_t* gnss,
    const purrgo_sun_info_t* sun,
    bool sun_initialized
) {{
    {fixed_cam_block}

    {map_vp_block}

    char buf[64];
    purrgo_gnss_solution_t gnss_solution = *gnss;
    purrgo_sun_info_t sun_info;
    if (sun != NULL) sun_info = *sun;
    gfx_context_t global_gfx_ctx = *gfx;

    {switch_block}
}}
"""

with open("src/core/app_ui.c", "w") as f:
    f.write(app_ui_c_content)
