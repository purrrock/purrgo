import re

with open("src/core/app_fsm.c", "r") as f:
    content = f.read()

# Let's clean up the fallback comment and remove the extra blank line
search = """        } else {
            // Fallback (should theoretically not happen, but safe practice)

            map_center_lat_1e7 = fix->lat_1e7;"""

replace = """        } else {
            // Fallback (should theoretically not happen, but safe practice)
            map_center_lat_1e7 = fix->lat_1e7;"""
content = content.replace(search, replace)

with open("src/core/app_fsm.c", "w") as f:
    f.write(content)
