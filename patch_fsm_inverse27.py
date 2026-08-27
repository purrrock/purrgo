import re

with open("src/core/app_fsm.c", "r") as f:
    content = f.read()

# Wait, if new_sx = 16 and dx = 184, follow_start_x is 184.
# So new_dx <= follow_start_x is TRUE (184 <= 184)!
# new_dy is 0 <= 184.
# So it DOES NOT fallback.
# So candidate_lon = 2538000.
# Then map_center_lon_1e7 = 2538000.
# But wait! Why did the test fail with "test_map_dirty_state: Assertion `purrgo_app_get_map_center_lon() != dist_start_zone' failed"??
# Wait, let's look at the CTest output from build:
# `FAILED! Map center lon is -18409`
# `test_app_fsm: /app/tests/core/test_app_fsm.c:176: test_map_dirty_state: Assertion \`purrgo_app_get_map_center_lon() == 0' failed.`
# Ah! It failed at line 176!
# Line 176 is `assert(purrgo_app_get_map_center_lon() == 0);`
# It's NOT failing at the dist_start_zone assertion!
# Wait, look at the last ctest output:
# test_app_fsm: /app/tests/core/test_app_fsm.c:194: test_map_dirty_state: Assertion `purrgo_app_get_map_center_lon() != dist_start_zone' failed.
# YES! AFTER I FIXED setup_test_state, it failed at line 194!
# Let's see what is on line 194!
