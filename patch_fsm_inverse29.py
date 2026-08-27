import re

with open("src/core/app_fsm.c", "r") as f:
    content = f.read()

search = """            printf("FALLBACK HIT! new_dx=%d, new_dy=%d, follow_start_x=%d, follow_start_y=%d\\n", new_dx, new_dy, follow_start_x, follow_start_y);
            map_center_lat_1e7 = fix->lat_1e7;"""

replace = """            fprintf(stderr, "FALLBACK HIT! new_dx=%d, new_dy=%d, follow_start_x=%d, follow_start_y=%d\\n", new_dx, new_dy, follow_start_x, follow_start_y);
            map_center_lat_1e7 = fix->lat_1e7;"""
content = content.replace(search, replace)
with open("src/core/app_fsm.c", "w") as f:
    f.write(content)
