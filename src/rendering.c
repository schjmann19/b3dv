#include "rendering.h"
#include "world.h"
#include <math.h>
#include "vec_math.h"

// Calculate light level for a block - simplified for performance
float get_block_light_level(World* world, int x, int y, int z)
{
    // Quick check: count only 4 blocks directly above (fast)
    int blocks_above = 0;
    for (int check_y = y + 1; check_y <= y + 4 && check_y < 256; check_y++) {
        BlockType above = world_get_block(world, x, check_y, z);
        if (above == BLOCK_AIR) {
            blocks_above++;
        } else {
            break;
        }
    }

    // If we have direct sky access from above (all 4 blocks clear)
    if (blocks_above >= 4) {
        return 1.0f;  // Fully lit
    }

    // Otherwise dim lighting
    return 0.6f;
}

// Apply lighting to a face based on direction and light level
// face_index: 0=+X, 1=-X, 2=+Y(top), 3=-Y(bottom), 4=+Z, 5=-Z
Color apply_face_lighting(Color base_color, int face_index, float light_level, World* world, int neighbor_x, int neighbor_y, int neighbor_z)
{
    float brightness = 1.0f;

    if (face_index == 2) {
        // Top face - brightest
        brightness = 1.0f;
    } else if (face_index == 3) {
        // Bottom face - darkest
        brightness = 0.5f;
    } else {
        // Side faces - medium brightness
        brightness = 0.75f;
    }

    // Apply brightness
    float final_brightness = brightness;

    // Clamp between 0.25 and 1.0 to ensure minimum visibility
    if (final_brightness < 0.25f) final_brightness = 0.25f;
    if (final_brightness > 1.0f) final_brightness = 1.0f;

    // Apply brightness to color
    Color lit_color;
    lit_color.r = (unsigned char)(base_color.r * final_brightness);
    lit_color.g = (unsigned char)(base_color.g * final_brightness);
    lit_color.b = (unsigned char)(base_color.b * final_brightness);
    lit_color.a = base_color.a;

    return lit_color;
}

// check if a block has any face visible (exposed to air)
bool has_visible_face(World* world, int x, int y, int z, Vector3 block_pos, Vector3 cam_pos)
{
    // Check all 6 neighbors - if any is air, this block has an exposed face
    if (world_get_block(world, x+1, y, z) == BLOCK_AIR ||
        world_get_block(world, x-1, y, z) == BLOCK_AIR ||
        world_get_block(world, x, y+1, z) == BLOCK_AIR ||
        world_get_block(world, x, y-1, z) == BLOCK_AIR ||
        world_get_block(world, x, y, z+1) == BLOCK_AIR ||
        world_get_block(world, x, y, z-1) == BLOCK_AIR) {
        return true;  // has at least one exposed face
    }

    return false;  // completely surrounded by solid blocks
}

// check if a block is occluded (completely surrounded by other blocks)
bool is_block_occluded(World* world, int x, int y, int z)
{
    // check all 6 neighbors - if all are stone, this block is completely hidden
    if (world_get_block(world, x+1, y, z) != BLOCK_AIR &&
        world_get_block(world, x-1, y, z) != BLOCK_AIR &&
        world_get_block(world, x, y+1, z) != BLOCK_AIR &&
        world_get_block(world, x, y-1, z) != BLOCK_AIR &&
        world_get_block(world, x, y, z+1) != BLOCK_AIR &&
        world_get_block(world, x, y, z-1) != BLOCK_AIR) {
        return true;  // block is completely surrounded, don't render
    }
    return false;
}

// check if a block is visible in the camera frustum
bool is_block_visible(Vector3 block_pos, Vector3 cam_pos, Vector3 cam_forward,
                      Vector3 cam_right, Vector3 cam_up, float render_distance,
                      float fovy, float aspect)
{
    Vector3 to_block = vec3_sub(block_pos, cam_pos);
    float dist_sq = to_block.x*to_block.x + to_block.y*to_block.y + to_block.z*to_block.z;
    float render_dist_sq = render_distance * render_distance;

    if (dist_sq > render_dist_sq) {
        return false;
    }

    // always render blocks within 15 units of player (exempt from FOV culling)
    if (dist_sq < 225.0f) return true;  // 15^2 = 225

    // normalize direction to block (need actual distance for normalized direction)
    float dist = sqrtf(dist_sq);
    if (dist < 0.1f) return true;

    float inv_dist = 1.0f / dist;
    to_block.x *= inv_dist;
    to_block.y *= inv_dist;
    to_block.z *= inv_dist;

    // check if block is in front of camera
    float depth = to_block.x * cam_forward.x + to_block.y * cam_forward.y + to_block.z * cam_forward.z;
    if (depth <= 0) return false;

    // convert FOV to radians
    float fovy_rad = fovy * 3.14159265f / 180.0f;

    // much smaller block margin - just account for 0.5 block radius, not 1.2
    float block_angular_size = atanf(0.5f / (dist > 0.5f ? dist : 0.5f));

    // calculate angle thresholds - use actual tangent-based FOV bounds
    float half_vert_tan = tanf(fovy_rad / 2.0f);
    float half_horiz_tan = half_vert_tan * aspect;

    // project block direction onto right and up vectors
    float right_proj = to_block.x * cam_right.x + to_block.y * cam_right.y + to_block.z * cam_right.z;
    float up_proj = to_block.x * cam_up.x + to_block.y * cam_up.y + to_block.z * cam_up.z;

    // check if angles are within FOV bounds (compare tan of angles)
    // tan(angle) = opposite / adjacent, so right_proj / depth gives tan of horizontal angle
    if (fabsf(right_proj / depth) > (half_horiz_tan + block_angular_size)) return false;
    if (fabsf(up_proj / depth) > (half_vert_tan + block_angular_size)) return false;

    return true;
}

// draw only the visible faces of a cube (faces pointing toward camera and not occluded by neighbors)
void draw_cube_faces(Vector3 pos, float size, Color color, Vector3 cam_pos, Color wire_color, World* world, int block_x, int block_y, int block_z)
{
    Vector3 to_cam = vec3_sub(cam_pos, pos);
    float h = size / 2.0f;

    // Get lighting for this block
    float light_level = get_block_light_level(world, block_x, block_y, block_z);

    // face normals and vertices (in pairs: v1, v2, v3, v4)
    // also include neighbor coordinates for occlusion checking
    struct {
        Vector3 normal;
        Vector3 v[4];
        int neighbor_x, neighbor_y, neighbor_z;
        int face_index;  // for lighting calculation
    } faces[6] = {
        // right (+X)
        {
            {1, 0, 0},
            {
                {pos.x + h, pos.y - h, pos.z - h},
                {pos.x + h, pos.y + h, pos.z - h},
                {pos.x + h, pos.y + h, pos.z + h},
                {pos.x + h, pos.y - h, pos.z + h}
            },
            block_x + 1, block_y, block_z,
            0
        },
        // left (-X)
        {
            {-1, 0, 0},
            {
                {pos.x - h, pos.y - h, pos.z + h},
                {pos.x - h, pos.y + h, pos.z + h},
                {pos.x - h, pos.y + h, pos.z - h},
                {pos.x - h, pos.y - h, pos.z - h}
            },
            block_x - 1, block_y, block_z,
            1
        },
        // top (+Y)
        {
            {0, 1, 0},
            {
                {pos.x - h, pos.y + h, pos.z + h},
                {pos.x + h, pos.y + h, pos.z + h},
                {pos.x + h, pos.y + h, pos.z - h},
                {pos.x - h, pos.y + h, pos.z - h}
            },
            block_x, block_y + 1, block_z,
            2
        },
        // bottom (-Y)
        {
            {0, -1, 0},
            {
                {pos.x + h, pos.y - h, pos.z + h},
                {pos.x - h, pos.y - h, pos.z + h},
                {pos.x - h, pos.y - h, pos.z - h},
                {pos.x + h, pos.y - h, pos.z - h}
            },
            block_x, block_y - 1, block_z,
            3
        },
        // front (+Z)
        {
            {0, 0, 1},
            {
                {pos.x - h, pos.y - h, pos.z + h},
                {pos.x + h, pos.y - h, pos.z + h},
                {pos.x + h, pos.y + h, pos.z + h},
                {pos.x - h, pos.y + h, pos.z + h}
            },
            block_x, block_y, block_z + 1,
            4
        },
        // back (-Z)
        {
            {0, 0, -1},
            {
                {pos.x + h, pos.y - h, pos.z - h},
                {pos.x - h, pos.y - h, pos.z - h},
                {pos.x - h, pos.y + h, pos.z - h},
                {pos.x + h, pos.y + h, pos.z - h}
            },
            block_x, block_y, block_z - 1,
            5
        }
    };

    // draw each face only if it points toward camera AND has no solid neighbor blocking it
    for (int i = 0; i < 6; i++) {
        float dot = to_cam.x * faces[i].normal.x + to_cam.y * faces[i].normal.y + to_cam.z * faces[i].normal.z;

        // face points toward camera
        if (dot > 0) {
            // check if neighbor is air (face is exposed)
            BlockType neighbor = world_get_block(world, faces[i].neighbor_x, faces[i].neighbor_y, faces[i].neighbor_z);
            if (neighbor == BLOCK_AIR) {
                // Apply lighting to the base color, passing neighbor coordinates for side faces
                Color lit_color = apply_face_lighting(color, faces[i].face_index, light_level, world, faces[i].neighbor_x, faces[i].neighbor_y, faces[i].neighbor_z);

                // render the face with lit color
                DrawTriangle3D(faces[i].v[0], faces[i].v[1], faces[i].v[2], lit_color);
                DrawTriangle3D(faces[i].v[0], faces[i].v[2], faces[i].v[3], lit_color);

                // draw wireframe
                DrawLine3D(faces[i].v[0], faces[i].v[1], wire_color);
                DrawLine3D(faces[i].v[1], faces[i].v[2], wire_color);
                DrawLine3D(faces[i].v[2], faces[i].v[3], wire_color);
                DrawLine3D(faces[i].v[3], faces[i].v[0], wire_color);
            }
        }
    }
}

