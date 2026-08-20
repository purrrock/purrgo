sed -i 's/bool sun_initialized//g' src/core/app_ui.c
sed -i 's/const purrgo_sun_info_t\* sun,/const purrgo_sun_info_t\* sun/g' src/core/app_ui.c
sed -i 's/&global_gfx_ctx/gfx/g' src/core/app_ui.c
sed -i 's/gnss_solution./gnss->/g' src/core/app_ui.c
sed -i 's/sun_info./sun->/g' src/core/app_ui.c
sed -i 's/if (sun_initialized)/if (sun != NULL)/g' src/core/app_ui.c
sed -i 's/COLOR_BLACK/0/g' src/core/app_ui.c
sed -i 's/COLOR_WHITE/3/g' src/core/app_ui.c
