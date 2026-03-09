#include <math.h>
#include <stdlib.h>
#include "clouds.h"
#include "rlgl.h"

CloudSystem* clouds_create(const char* cloud_image_path) {
    CloudSystem* clouds = (CloudSystem*)malloc(sizeof(CloudSystem));
    if (clouds) {
        clouds->grid_size = 12;           // 12x12 grid of cloud segments
        clouds->cloud_height = 80.0f;     // Lower than before for better visibility
        clouds->cloud_spacing = 64.0f;    // Space between cloud centers
        clouds->cloud_size = 64.0f;       // Same as spacing so clouds touch seamlessly
        clouds->grid_offset = (Vector2){0.0f, 0.0f};
        clouds->texture_loaded = false;

        // Try to load cloud image
        if (cloud_image_path) {
            clouds->cloud_texture = LoadTexture(cloud_image_path);
            if (clouds->cloud_texture.id > 0) {
                clouds->texture_loaded = true;
            }
        }
    }
    return clouds;
}

void clouds_free(CloudSystem* clouds) {
    if (clouds) {
        if (clouds->texture_loaded) {
            UnloadTexture(clouds->cloud_texture);
        }
        free(clouds);
    }
}

void clouds_update(CloudSystem* clouds, Vector3 player_pos) {
    // Move clouds relative to player for Minecraft-like parallax effect
    // Clouds move at 1/10th the player speed for a subtle drift
    clouds->grid_offset.x = (player_pos.x / clouds->cloud_spacing) * 0.1f;
    clouds->grid_offset.y = (player_pos.z / clouds->cloud_spacing) * 0.1f;
}

void clouds_draw(CloudSystem* clouds, Vector3 camera_pos, Vector3 camera_offset) {
    if (!clouds || !clouds->texture_loaded) {
        return;
    }

    // Set texture for rendering
    rlSetTexture(clouds->cloud_texture.id);

    BeginBlendMode(BLEND_ALPHA);

    // Calculate which tiles to render based on camera position
    float tile_size = 512.0f;  // Size of each cloud texture tile in world units
    int center_tile_x = (int)floorf(camera_pos.x / tile_size);
    int center_tile_z = (int)floorf(camera_pos.z / tile_size);

    // Render a 3x3 grid of tiles around the camera
    for (int tile_dx = -1; tile_dx <= 1; tile_dx++) {
        for (int tile_dz = -1; tile_dz <= 1; tile_dz++) {
            int tile_x = center_tile_x + tile_dx;
            int tile_z = center_tile_z + tile_dz;

            // Calculate tile position
            float world_x = (float)(tile_x * tile_size) - camera_offset.x;
            float world_y = clouds->cloud_height - camera_offset.y;
            float world_z = (float)(tile_z * tile_size) - camera_offset.z;

            // Draw textured quad using rlgl
            rlBegin(RL_QUADS);
            rlColor4ub(255, 255, 255, 220);  // Semi-transparent white

            // Bottom-left
            rlTexCoord2f(0.0f, 0.0f);
            rlVertex3f(world_x, world_y, world_z);

            // Bottom-right
            rlTexCoord2f(1.0f, 0.0f);
            rlVertex3f(world_x + tile_size, world_y, world_z);

            // Top-right
            rlTexCoord2f(1.0f, 1.0f);
            rlVertex3f(world_x + tile_size, world_y, world_z + tile_size);

            // Top-left
            rlTexCoord2f(0.0f, 1.0f);
            rlVertex3f(world_x, world_y, world_z + tile_size);

            rlEnd();

            // Draw underside (darker)
            rlBegin(RL_QUADS);
            rlColor4ub(180, 180, 180, 160);

            float bottom_y = world_y - 2.0f;

            // Bottom face (reversed winding for correct facing)
            rlTexCoord2f(0.0f, 0.0f);
            rlVertex3f(world_x, bottom_y, world_z + tile_size);

            rlTexCoord2f(1.0f, 0.0f);
            rlVertex3f(world_x + tile_size, bottom_y, world_z + tile_size);

            rlTexCoord2f(1.0f, 1.0f);
            rlVertex3f(world_x + tile_size, bottom_y, world_z);

            rlTexCoord2f(0.0f, 1.0f);
            rlVertex3f(world_x, bottom_y, world_z);

            rlEnd();
        }
    }

    EndBlendMode();

    // Reset texture
    rlSetTexture(0);
}

void clouds_reset(CloudSystem* clouds) {
    if (clouds) {
        clouds->grid_offset = (Vector2){0.0f, 0.0f};
    }
}
