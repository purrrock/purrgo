sed -i 's/bool sun_initialized//g' src/core/app_ui.c
sed -i 's/const purrgo_sun_info_t\* sun,/const purrgo_sun_info_t\* sun/g' src/core/app_ui.c
sed -i 's/&global_gfx_ctx/gfx/g' src/core/app_ui.c
sed -i 's/gnss_solution./gnss->/g' src/core/app_ui.c
sed -i 's/sun_info./sun->/g' src/core/app_ui.c
sed -i 's/if (sun_initialized)/if (sun != NULL)/g' src/core/app_ui.c
sed -i 's/COLOR_BLACK/0/g' src/core/app_ui.c
sed -i 's/COLOR_WHITE/3/g' src/core/app_ui.c
sed -i 's/DISPLAY_WIDTH/128/g' src/core/app_ui.c
sed -i 's/DISPLAY_HEIGHT/296/g' src/core/app_ui.c
sed -i 's/#include "emu_fs.h"/extern uint32_t emu_fs_read(void* handle, void* buffer, uint32_t size);\nextern bool emu_fs_seek(void* handle, uint32_t offset);/g' src/core/app_ui.c
