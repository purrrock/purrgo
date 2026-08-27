import re

with open("tests/core/test_app_fsm.c", "r") as f:
    content = f.read()

# Ah! purrgo_config_init() sets last_lat_1e7 to 537135000 and 284199000!
# Not 0!
# So when setup_test_state(0, 0, scale) does purrgo_app_init(), map center is initially 537135000!
# Then it calls purrgo_app_update(&fix) with fix(0, 0).
# Since 0,0 is very far from 537135000, it triggers Reached or exceeded FOLLOW_START zone.
# BEFORE, the exact recentering meant it would just set map_center to fix(0,0)!
# NOW, the opposite-side logic calculates a new map_center that is NOT 0,0!
# So it ends up at some weird coordinate like -18409.
# And then the test expects it to be 0,0.

# How to fix setup_test_state?
# Instead of relying on auto-follow to exact-center, we can just manipulate app_config before purrgo_app_init!
# Oh wait, purrgo_app_init() calls purrgo_config_load() which calls purrgo_config_init() which overwrites our app_config changes!!!
# Yes! `purrgo_config_init` unconditionally overwrites it.

# How can we force the map center?
# We can't access `map_center_lat_1e7` because it's static.
# We can add a test-only function to app_fsm.c or we can just send multiple purrgo_app_update until it converges? No, it won't converge to exactly 0,0.
# Wait, if we use the manual pan button, manual pan modifies the center, but we don't have absolute positioning.

# Wait, the only way to set the center is to let auto-follow do it. But auto-follow no longer centers exactly!
# Wait, if `map_center` is static, how is the test checking it?
# `purrgo_app_get_map_center_lon()`!
# We can add `purrgo_app_set_map_center_for_test(lat, lon)` or we can just change the default config? No, it's compiled in.
# Can we change `purrgo_config_load` to not overwrite if a test flag is set? No.

# Look at test_app_fsm.c again. We have #include "purrgo/app_fsm.h"
# What if we include a setter in app_fsm.c?
