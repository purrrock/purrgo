import re

with open("src/core/app_fsm.c", "r") as f:
    content = f.read()

# Add a printf to the fallback branch to see if we hit it.
search = """        } else {
            // Fallback (should theoretically not happen, but safe practice)
            map_center_lat_1e7 = fix->lat_1e7;
            map_center_lon_1e7 = fix->lon_1e7;
            map_dirty = true;
        }"""

replace = """        } else {
            // Fallback (should theoretically not happen, but safe practice)
            printf("FALLBACK HIT! new_dx=%d, new_dy=%d, follow_start_x=%d, follow_start_y=%d\\n", new_dx, new_dy, follow_start_x, follow_start_y);
            map_center_lat_1e7 = fix->lat_1e7;
            map_center_lon_1e7 = fix->lon_1e7;
            map_dirty = true;
        }"""
content = content.replace(search, replace)
with open("src/core/app_fsm.c", "w") as f:
    f.write(content)
