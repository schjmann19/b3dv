#ifndef CLOUDS_H
#define CLOUDS_H

#include "raylib.h"

// Cloud system structure
typedef struct {
    int grid_size;        // Size of cloud grid (e.g., 12x12 clouds)
    float cloud_height;   // Y position of clouds
    float cloud_spacing;  // Distance between cloud blocks
    float cloud_size;     // Size of each cloud segment
    Vector2 grid_offset;  // Offset for smooth scrolling
    Texture2D cloud_texture;  // Cloud image texture
    bool texture_loaded;  // Whether texture was successfully loaded
} CloudSystem;

// Function declarations
CloudSystem* clouds_create(const char* cloud_image_path);
void clouds_free(CloudSystem* clouds);
void clouds_update(CloudSystem* clouds, Vector3 player_pos);
void clouds_draw(CloudSystem* clouds, Vector3 camera_pos, Vector3 camera_offset);
void clouds_reset(CloudSystem* clouds);

#endif
