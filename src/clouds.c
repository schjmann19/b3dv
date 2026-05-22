#if 0
#include <math.h>
#include <stdlib.h>
#include "clouds.h"
#include "rlgl.h"
#include <GL/gl.h>

static float fract(float x) {
    return x - floorf(x);
}
// Simple pseudo-random number generator for cloud noise
static float hash1(float n) {
    return fract(sinf(n) * 43758.5453f);
}

// 2D noise-like function for cloud density
static float cloud_noise_2d(float x, float z) {
    float i = floorf(x);
    float f = fract(x);
    float j = floorf(z);
    float g = fract(z);
    
    // Smooth interpolation (Hermite curve)
    float u = f * f * (3.0f - 2.0f * f);
    float v = g * g * (3.0f - 2.0f * g);
    
    // Hash and blend
    float n00 = hash1(i * 73.0f + j * 37.0f);
    float n10 = hash1((i + 1.0f) * 73.0f + j * 37.0f);
    float n01 = hash1(i * 73.0f + (j + 1.0f) * 37.0f);
    float n11 = hash1((i + 1.0f) * 73.0f + (j + 1.0f) * 37.0f);
    
    float nx0 = n00 * (1.0f - u) + n10 * u;
    float nx1 = n01 * (1.0f - u) + n11 * u;
    float result = nx0 * (1.0f - v) + nx1 * v;
    
    return result;
}

static bool cloud_map_occupied(const CloudSystem* clouds, int x, int z) {
    int size = clouds->cloud_map_size;
    x %= size;
    if (x < 0) x += size;
    z %= size;
    if (z < 0) z += size;
    return clouds->cloud_map[z][x] != 0;
}

static void cloud_generate_map(CloudSystem* clouds) {
    int size = clouds->cloud_map_size;
    const float noise_scale = 0.03f;
    const float threshold = 0.55f;

    for (int z = 0; z < size; z++) {
        for (int x = 0; x < size; x++) {
            float sample_x = (float)x * noise_scale;
            float sample_z = (float)z * noise_scale;
            float value = cloud_noise_2d(sample_x, sample_z);
            clouds->cloud_map[z][x] = (value > threshold) ? 1 : 0;
        }
    }
}

CloudSystem* clouds_create(const char* cloud_image_path) {
    (void)cloud_image_path;  // Unused - we generate procedurally now
    
    CloudSystem* clouds = (CloudSystem*)malloc(sizeof(CloudSystem));
    if (clouds) {
        clouds->grid_size = 12;
        clouds->cloud_height = 128.0f;
        clouds->cloud_spacing = 8.0f;
        clouds->cloud_size = 8.0f;
        clouds->grid_offset = (Vector2){0.0f, 0.0f};
        clouds->anchor_pos = (Vector2){0.0f, 0.0f};
        clouds->texture_loaded = true;  // Mark as ready for procedural generation
        clouds->render_distance = 128.0f;
        clouds->enabled = true;
        clouds->time_offset = 0.0f;
        clouds->scroll_speed = 0.125f; // ~1 block per second on 8-unit tiles
        clouds->cloud_map_size = 128;
        cloud_generate_map(clouds);
    }
    return clouds;
}

void clouds_free(CloudSystem* clouds) {
    if (clouds) {
        free(clouds);
    }
}

void clouds_update(CloudSystem* clouds, Vector3 player_pos) {
    if (!clouds) return;
    
    if (!clouds) return;

    clouds->time_offset += GetFrameTime() * clouds->scroll_speed;
    if (clouds->time_offset > clouds->cloud_map_size) {
        clouds->time_offset -= clouds->cloud_map_size;
    }
    
    (void)player_pos;
}

void clouds_draw(CloudSystem* clouds, Vector3 camera_pos, Vector3 camera_offset) {
    if (!clouds || !clouds->enabled) {
        return;
    }

    rlDisableBackfaceCulling();
    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE); // Depth-test but don't write depth for clouds

    // Cloud grid parameters
    float grid_spacing = clouds->cloud_spacing;
    float cloud_size = clouds->cloud_size;
    float render_dist = clouds->render_distance;

    // Calculate which grid cells are visible around the camera
    float cam_x = camera_pos.x;
    float cam_z = camera_pos.z;
    
    int grid_start_x = (int)floorf((cam_x - render_dist) / grid_spacing);
    int grid_end_x = (int)floorf((cam_x + render_dist) / grid_spacing);
    int grid_start_z = (int)floorf((cam_z - render_dist) / grid_spacing);
    int grid_end_z = (int)floorf((cam_z + render_dist) / grid_spacing);

    const int size = clouds->cloud_map_size;
    float total_offset = clouds->time_offset;
    int offset_cells = (int)floorf(total_offset);
    float offset_frac = total_offset - (float)offset_cells;
    float offset_world = offset_frac * grid_spacing;
    float world_y = clouds->cloud_height - camera_offset.y;
    float bottom_y_local = world_y - 4.0f;

    // Batch top faces
    rlBegin(RL_QUADS);
    rlColor4ub(255, 255, 255, 255);
    for (int grid_x = grid_start_x; grid_x <= grid_end_x; grid_x++) {
        for (int grid_z = grid_start_z; grid_z <= grid_end_z; grid_z++) {
            int map_x = grid_x + offset_cells;
            map_x %= size; if (map_x < 0) map_x += size;
            int map_z = grid_z % size; if (map_z < 0) map_z += size;
            if (!clouds->cloud_map[map_z][map_x]) continue;

            float draw_x = (float)grid_x * grid_spacing - camera_offset.x - offset_world;
            float draw_z = (float)grid_z * grid_spacing - camera_offset.z;

            rlVertex3f(draw_x, world_y, draw_z);
            rlVertex3f(draw_x + cloud_size, world_y, draw_z);
            rlVertex3f(draw_x + cloud_size, world_y, draw_z + cloud_size);
            rlVertex3f(draw_x, world_y, draw_z + cloud_size);
        }
    }
    rlEnd();

    // Batch bottom faces
    rlBegin(RL_QUADS);
    rlColor4ub(220, 220, 220, 255);
    for (int grid_x = grid_start_x; grid_x <= grid_end_x; grid_x++) {
        for (int grid_z = grid_start_z; grid_z <= grid_end_z; grid_z++) {
            int map_x = grid_x + offset_cells;
            map_x %= size; if (map_x < 0) map_x += size;
            int map_z = grid_z % size; if (map_z < 0) map_z += size;
            if (!clouds->cloud_map[map_z][map_x]) continue;

            float draw_x = (float)grid_x * grid_spacing - camera_offset.x - offset_world;
            float draw_z = (float)grid_z * grid_spacing - camera_offset.z;

            // Draw bottom face with reversed winding (ensure it's front-facing when viewed from below)
            rlVertex3f(draw_x, bottom_y_local, draw_z);
            rlVertex3f(draw_x + cloud_size, bottom_y_local, draw_z);
            rlVertex3f(draw_x + cloud_size, bottom_y_local, draw_z + cloud_size);
            rlVertex3f(draw_x, bottom_y_local, draw_z + cloud_size);
        }
    }
    rlEnd();

    // Batch side faces on cloud edges
    rlBegin(RL_QUADS);
    rlColor4ub(230, 230, 230, 255);
    for (int grid_x = grid_start_x; grid_x <= grid_end_x; grid_x++) {
        for (int grid_z = grid_start_z; grid_z <= grid_end_z; grid_z++) {
            int map_x = grid_x + offset_cells;
            map_x %= size; if (map_x < 0) map_x += size;
            int map_z = grid_z % size; if (map_z < 0) map_z += size;
            if (!clouds->cloud_map[map_z][map_x]) continue;

            float draw_x = (float)grid_x * grid_spacing - camera_offset.x - offset_world;
            float draw_z = (float)grid_z * grid_spacing - camera_offset.z;

            int left_x = map_x - 1; if (left_x < 0) left_x += size;
            int right_x = map_x + 1; if (right_x >= size) right_x -= size;
            int front_z = map_z + 1; if (front_z >= size) front_z -= size;
            int back_z = map_z - 1; if (back_z < 0) back_z += size;

            bool occupied_left = clouds->cloud_map[map_z][left_x];
            bool occupied_right = clouds->cloud_map[map_z][right_x];
            bool occupied_front = clouds->cloud_map[front_z][map_x];
            bool occupied_back = clouds->cloud_map[back_z][map_x];

            if (!occupied_left) {
                // West-facing face (normal -X)
                rlVertex3f(draw_x, bottom_y_local, draw_z);
                rlVertex3f(draw_x, bottom_y_local, draw_z + cloud_size);
                rlVertex3f(draw_x, world_y, draw_z + cloud_size);
                rlVertex3f(draw_x, world_y, draw_z);
            }
            if (!occupied_right) {
                // East-facing face (normal +X)
                rlVertex3f(draw_x + cloud_size, bottom_y_local, draw_z + cloud_size);
                rlVertex3f(draw_x + cloud_size, bottom_y_local, draw_z);
                rlVertex3f(draw_x + cloud_size, world_y, draw_z);
                rlVertex3f(draw_x + cloud_size, world_y, draw_z + cloud_size);
            }
            if (!occupied_back) {
                // North-facing face (normal -Z)
                rlVertex3f(draw_x + cloud_size, bottom_y_local, draw_z);
                rlVertex3f(draw_x, bottom_y_local, draw_z);
                rlVertex3f(draw_x, world_y, draw_z);
                rlVertex3f(draw_x + cloud_size, world_y, draw_z);
            }
            if (!occupied_front) {
                // South-facing face (normal +Z)
                rlVertex3f(draw_x, bottom_y_local, draw_z + cloud_size);
                rlVertex3f(draw_x + cloud_size, bottom_y_local, draw_z + cloud_size);
                rlVertex3f(draw_x + cloud_size, world_y, draw_z + cloud_size);
                rlVertex3f(draw_x, world_y, draw_z + cloud_size);
            }
        }
    }
    rlEnd();

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    rlEnableBackfaceCulling();
}

void clouds_reset(CloudSystem* clouds) {
    if (clouds) {
        clouds->time_offset = 0.0f;
    }
}
#endif