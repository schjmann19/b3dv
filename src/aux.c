#include <dirent.h>
#include <math.h>
#include <raylib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common_utils/args.h"

#include "aux.h"

#include "neutrino_detect.h"

bool neutrino_detection = false;

void do_args(int argc, char **argv) {
    // if no args, print version and help
    if (argc == 1) {
        // puts("b3dv version 0.0.22-beta");
        puts("Usage:");
        puts("       b3dv [--version, -v] - version info");
        puts("       b3dv run             - launch game");
        exit(0);
    }

    // version argument
    if (arg_is_present("version", argc, &*argv) || arg_is_present("v", argc, &*argv)) {
        puts("b3dv version 0.0.22-beta");
        puts("By Jimena Neumann; BSD-3-Clause License");
#ifdef DEBUG
        puts("compiled on " __DATE__ " at " __TIME__);
#endif
        exit(0);
    }

    // neutrino detection mode

    if (arg_is_present("neutrino_detect", argc, &*argv)) {
        neutrino_detection = true;
    }

    // run
    if (argc > 1 && arg_is_present("run", argc, &*argv)) {
        /* */
    } else {
        fprintf(stderr, "Unknown or invalid argument(s): %s\n", all_args_to_string(argc, &*argv, ',', true).data);
    }
}

bool sdf_font_is_sdf = false;

// Helper function to load a specific font variant
Font load_font_variant(const char *font_family, const char *font_variant) {
    char font_path[512];
    snprintf(font_path, sizeof(font_path), "./assets/fonts/%s/ttf/%s", font_family, font_variant);

    // Build codepoint array with all ASCII and common extended characters
    int codepoints[1024] = {0};
    int codepoint_count = 0;

    // Full ASCII range (0-127)
    for (int i = 0; i < 128; i++) {
        codepoints[codepoint_count++] = i;
    }
    // Latin-1 Supplement (128-255)
    for (int i = 128; i < 256; i++) {
        codepoints[codepoint_count++] = i;
    }
    // Latin Extended-A (256-383)
    for (int i = 256; i < 384; i++) {
        codepoints[codepoint_count++] = i;
    }
    // Cyrillic block (0x0400-0x04FF)
    for (int i = 0x0400; i <= 0x04FF && codepoint_count < 1024; i++) {
        codepoints[codepoint_count++] = i;
    }

    // Load font at a large base_size to generate high-quality glyphs
    // Now that HIGHDPI is disabled, we can use a moderate base_size for smooth scaling.
    int base_size = 256;
    Font font = LoadFontEx(font_path, base_size, codepoints, codepoint_count);
    if (font.glyphCount > 0) {
        // Use bilinear filtering for smooth downsampling
        SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
        GenTextureMipmaps(&font.texture);
        sdf_font_is_sdf = false;
        TraceLog(LOG_INFO, "Loaded high-quality font %s size=%d glyphs=%d (bilinear + mipmaps)", font_path, base_size, font.glyphCount);
        return font;
    }

    // Fallback to default if font not found
    sdf_font_is_sdf = false;
    TraceLog(LOG_WARNING, "Failed to load font %s, using default font", font_path);
    return GetFontDefault();
}

// Helper function to load a font by family (uses first variant found)
Font load_font_by_name(const char *font_name) {
    char ttf_dir[512];
    snprintf(ttf_dir, sizeof(ttf_dir), "./assets/fonts/%s/ttf", font_name);

    // Open the ttf directory and find the first .ttf file
    DIR *dir = opendir(ttf_dir);

    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir))) {
            // Look for .ttf files
            char *dot = strrchr(entry->d_name, '.');
            if (dot && strcmp(dot, ".ttf") == 0) {
                // Found a .ttf file, try to load it
                closedir(dir);
                return load_font_variant(font_name, entry->d_name);
            }
        }
        closedir(dir);
    }

    // Fallback to default if no font found
    return GetFontDefault();
}

// Wrapper that draws `font` - fonts rasterized at 256px with mipmaps for smooth scaling
void DrawTextExCustom(Font font, const char *text, Vector2 position, float fontSize, float spacing, Color tint) {
    // High-quality rasterization with mipmaps produces crisp text at various sizes
    DrawTextEx(font, text, position, fontSize, spacing, tint);
}

const char *get_cardinal_direction(float heading_deg) {
    if (heading_deg < 22.5f || heading_deg >= 337.5f) {
        return "N";
    }
    if (heading_deg < 67.5f) {
        return "NE";
    }
    if (heading_deg < 112.5f) {
        return "E";
    }
    if (heading_deg < 157.5f) {
        return "SE";
    }
    if (heading_deg < 202.5f) {
        return "S";
    }
    if (heading_deg < 247.5f) {
        return "SW";
    }
    if (heading_deg < 292.5f) {
        return "W";
    }
    return "NW";
}

void draw_compass_hud(Font font, float yaw_radians) {
    float heading_deg = yaw_radians * (180.0f / 3.14159265f);
    heading_deg = fmodf(heading_deg, 360.0f);
    if (heading_deg < 0.0f) {
        heading_deg += 360.0f;
    }

    const char *cardinal = get_cardinal_direction(heading_deg);
    char heading_text[64];
    snprintf(heading_text, sizeof(heading_text), "Heading: %.0f° %s", heading_deg, cardinal);

    int screen_w = GetScreenWidth();
    Vector2 text_size = MeasureTextEx(font, heading_text, 28, 1);
    Vector2 text_pos = {(float)(screen_w - (int)text_size.x - 20), 20.0f};

    DrawRectangle((int)text_pos.x - 10, (int)text_pos.y - 8, (int)text_size.x + 20, (int)text_size.y + 16, (Color){0, 0, 0, 150});
    DrawTextExCustom(font, heading_text, text_pos, 28, 1, WHITE);
}
