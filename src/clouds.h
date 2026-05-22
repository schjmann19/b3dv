#if 0
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
    Vector2 anchor_pos;   // World X/Z position clouds are anchored to (player)
    Texture2D cloud_texture;  // Cloud image texture (legacy, unused)
    bool texture_loaded;  // Whether texture was successfully loaded
    float render_distance;  // Cloud render distance
    bool enabled;         // Whether clouds are enabled
    float time_offset;    // Time offset for cloud movement (eastward drift)
    float scroll_speed;   // Cells per second for cloud movement
    int cloud_map_size;   // Size of the persistent cloud mask
    unsigned char cloud_map[128][128]; // Persistent cloud occupancy mask
} CloudSystem;

// Function declarations
CloudSystem* clouds_create(const char* cloud_image_path);
void clouds_free(CloudSystem* clouds);
void clouds_update(CloudSystem* clouds, Vector3 player_pos);
void clouds_draw(CloudSystem* clouds, Vector3 camera_pos, Vector3 camera_offset);
void clouds_reset(CloudSystem* clouds);

#endif
#endif
