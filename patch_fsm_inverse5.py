import re

with open("tests/core/test_app_fsm.c", "r") as f:
    content = f.read()

# Let's see what happened in test_map_dirty_state.
# It sets setup_test_state(0, 0, PURRGO_MAP_SCALE_10KM);
# Which sets center_lon to 0.
# Then fix.lon_1e7 = dist_stop_zone; purrgo_app_update(&fix);
# Then assert(purrgo_app_get_map_center_lon() == 0); -> FAILS!
# Why does it fail? It means map_center_lon_1e7 changed!
# Why did it change? If purrgo_app_update triggered apply_auto_follow,
# and it went into the `else` block (Reached or exceeded FOLLOW_START zone).
# But wait, dx <= follow_stop_x && dy <= follow_stop_y should be true!
# Unless sx and sy calculations were affected.
# Did I change anything before apply_auto_follow?
# No.
# Could it be that fix.valid is false?
# test_map_dirty_state:
# purrgo_gnss_solution_t fix = {0};
# fix.valid = true;
# setup_test_state(0, 0, ...); -> this might have cleared fix? No, fix is local.
# Ah, I replaced something in app_fsm.c that broke it.
# Let's check diff of app_fsm.c.
