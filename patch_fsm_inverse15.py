import re

with open("tests/core/test_app_fsm.c", "r") as f:
    content = f.read()

# Let's fix test_auto_follow_opposite_side to reset app_config so it doesn't affect other tests.
# Wait, test_auto_follow_opposite_side is the LAST test! It runs after test_map_dirty_state.
# Then why does test_map_dirty_state fail at line 173 with -18409?!
# Oh wait!
# "FAILED! Map center lon is -18409"
# Why -18409???
# Where does -18409 come from?
# -18409 is app_config.last_lon_1e7!
# Wait, let's grep for 18409 or app_config initialization!
