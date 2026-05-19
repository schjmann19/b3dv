#!/usr/bin/env python3

# texturawasd - 10/02/2026

# restore the default settings to the settings file (options.conf)

default_settings: str = \
"# B3DV Default Game Settings\n\
render_distance=64\n\
clouds_enabled=true\n\
compass_enabled=true\n\
clouds_render_distance=128.0\n\
max_fps=0 # 0 means unlimited\n\
language=en\n\
# do not change fonts manually i made a nice little interface for that :c\n\
font_family=Inter\n\
font_variant=Inter_28pt-Light.ttf\n\
nickname=Player\n\
"


FILE: str = "options.conf"

def restore_default_settings():
    with open(FILE, "w") as file:
        file.write(default_settings)
    print(f"restored default settingss to {FILE}")

if __name__ == "__main__":
    restore_default_settings()
