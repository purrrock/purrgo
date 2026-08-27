import re

with open("src/core/app_fsm.c", "r") as f:
    content = f.read()

# Wait, dx < follow_start_x && dy < follow_start_y
# If dx = 12, dy = 0, follow_start_x = 200 - 16 = 184.
# So dx < follow_start_x IS TRUE!
# So WHY did it go to the else block????
# Let's check apply_auto_follow to see if it really went to the else block.
# Ah, maybe I modified something else in app_fsm.c?
# Look at my patch:
