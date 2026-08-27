import re

with open("tests/core/test_app_fsm.c", "r") as f:
    content = f.read()

# Now `dist_start_zone` test is failing because `purrgo_app_get_map_center_lon() != dist_start_zone` is failing!
# Which means `purrgo_app_get_map_center_lon() == dist_start_zone` is true!
# Why would it equal dist_start_zone? That would mean it exactly centered!
# But we changed the auto-follow logic to opposite side!
# Why did it exactly center?
# Let's check app_fsm.c to see if the fallback was hit.
# "if (new_dx <= follow_start_x && new_dy <= follow_start_y)"
# Was the new position outside the safe zone?
# If so, it falls back to exact center.
