import re

with open('tests/core/test_config.c', 'r') as f:
    text = f.read()

text = text.replace("    test_map_layers_load_save();\\n", "    test_map_layers_load_save();\\n    test_map_details_load_save();\\n")
text = text.replace("    test_map_layers_load_save();\n", "    test_map_layers_load_save();\n    test_map_details_load_save();\n")
with open('tests/core/test_config.c', 'w') as f:
    f.write(text)

with open('tests/core/test_config_controller.c', 'r') as f:
    text = f.read()

text = text.replace("    test_map_layers_navigation_and_toggles();\\n", "    test_map_layers_navigation_and_toggles();\\n    test_map_details_toggling();\\n    test_map_layers_opening();\\n")
text = text.replace("    test_map_layers_navigation_and_toggles();\n", "    test_map_layers_navigation_and_toggles();\n    test_map_details_toggling();\n    test_map_layers_opening();\n")
with open('tests/core/test_config_controller.c', 'w') as f:
    f.write(text)
