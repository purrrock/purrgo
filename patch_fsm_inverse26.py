import re

with open("tests/core/test_app_fsm.c", "r") as f:
    content = f.read()

# Ah! dist_start_zone test failed at assertion:
# `purrgo_app_get_map_center_lon() != dist_start_zone` failed.
# This means `purrgo_app_get_map_center_lon() == dist_start_zone` is true!
# So the fallback WAS hit! (Or it was calculated exactly as dist_start_zone).
# Why wasn't FALLBACK HIT printed? Because the log was not flushed? Wait, `printf` flushes on `\n`.
# Let's add fflush(stdout); to the printf!
# Actually, wait. I can check why it equals dist_start_zone!
# Wait! In the calculation:
# target_x = 2 * center_x - sx;
# If sx = 392, center_x = 200, target_x = 2 * 200 - 392 = 400 - 392 = 8.
# min_x = 200 - 184 = 16.
# target_x is clamped to min_x (16).
# So target_x = 16.
# Then we do the inverse projection.
# x_term = target_x - offset_x = 16.
# candidate_lon = fix->lon_1e7 - (x_term * geo_width) / map_vp.width + rad_x
# fix->lon_1e7 = dist_start_zone.
# rad_x = geo_width / 2.
# x_term = 16. map_vp.width = 400.
# candidate_lon = dist_start_zone - (16 * geo_width) / 400 + geo_width / 2
# geo_width / 400 = 6750.
# 16 * 6750 = 108000.
# dist_start_zone = 1296000.
# candidate_lon = 1296000 - 108000 + 1350000 = 2538000.
# Then we check if new_dx <= follow_start_x.
# Wait!
