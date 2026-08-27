import re

with open("src/core/app_fsm.c", "r") as f:
    content = f.read()

# Wait, if candidate_lon = 2538000, why does purrgo_app_get_map_center_lon() == dist_start_zone?!
# Did it hit the fallback branch?
# Earlier I added printf("FALLBACK HIT..."). Did I see it in the CTest output?
# CTest output was:
# After setup_test_state, lon is 0
# test_app_fsm: /app/tests/core/test_app_fsm.c:194: test_map_dirty_state: Assertion `purrgo_app_get_map_center_lon() != dist_start_zone' failed.
# Aborted
# Wait, printf might be buffered! Let's fflush(stdout);
