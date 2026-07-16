#ifndef B3DV_AUX_H
#define B3DV_AUX_H

#include <raylib.h>

#include "neutrino_detect.h"

extern bool neutrino_detection;

void do_args(int argc, char** argv);

extern bool sdf_font_is_sdf;

Font load_font_variant(const char* font_family, const char* font_variant);
Font load_font_by_name(const char* font_name);

void DrawTextExCustom(Font font, const char *text, Vector2 position, float fontSize, float spacing, Color tint);

const char* get_cardinal_direction(float heading_deg);
void draw_compass_hud(Font font, float yaw_radians);

#endif /* B3DV_AUX_H */