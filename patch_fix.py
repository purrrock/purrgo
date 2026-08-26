import re

with open("src/core/ui/ui_map.c", "r") as f:
    code = f.read()

# I already did the Update Map Rendering logic in the previous step during the refactoring script (`patch2.py`).
# The map rendering correctly restores map background for partial update and calls `display_refresh()` for a full redraw. Let's make sure it's correctly applied.

print(re.search(r'display_refresh_region', code) is not None)
print(re.search(r'purrgo_map_render_viewport.*clip', code, re.DOTALL) is not None)
