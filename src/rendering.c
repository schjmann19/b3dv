#include <math.h>

#include "raylib.h"
#include "rlgl.h"
#include "rendering.h"
#include "world.h"
#include "vec_math.h"

#define RENDER_WIREFRAMES false

// Calculate light level for a block - check for unobstructed access to sunlight
// Uses early termination and limited search to avoid expensive iteration
float get_block_light_level(World* world, int x, int y, int z)
{
    // Quick check: if we're at height 240 or above, assume fully lit
    if (y >= 240) return 1.0f;

    // Check only up to 30 blocks above (most light is blocked within this range)
    int check_limit = (y + 30 < 256) ? y + 30 : 256;

    for (int check_y = y + 1; check_y < check_limit; check_y++) {
        BlockType above = world_get_block(world, x, check_y, z);
        if (above != BLOCK_AIR) {
            // Hit a solid block - this position is shadowed
            return 0.6f;  // Dim lighting
        }
    }

    // Made it all the way up without hitting obstruction
    return 1.0f;  // Fully lit - direct sky access
}

// Apply lighting to a face based on direction and adjacent air block's sky access
// face_index: 0=+X, 1=-X, 2=+Y(top), 3=-Y(bottom), 4=+Z, 5=-Z
// neighbor_x/y/z: coordinates of the adjacent air block this face is exposed to
Color apply_face_lighting(Color base_color, int face_index, World* world, int neighbor_x, int neighbor_y, int neighbor_z)
{
    float face_brightness = 1.0f;

    if (face_index == 2) {
        // Top face - brightest
        face_brightness = 1.0f;
    } else if (face_index == 3) {
        // Bottom face - darkest
        face_brightness = 0.8f;
    } else {
        // Side faces - nearly full brightness
        face_brightness = 0.95f;
    }

    // Check if the adjacent air block has direct sky access
    // This determines if the face should be shadowed
    float adjacent_light = get_block_light_level(world, neighbor_x, neighbor_y, neighbor_z);

    // Apply both face brightness and adjacent block's light level
    float final_brightness = face_brightness * adjacent_light;

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

// Optimized version with pre-computed FOV values to avoid recalculating tan each block
bool is_block_visible_fast(Vector3 block_pos, Vector3 cam_pos, Vector3 cam_forward,
                           Vector3 cam_right, Vector3 cam_up, float render_distance,
                           float half_vert_tan, float half_horiz_tan)
{
    Vector3 to_block = vec3_sub(block_pos, cam_pos);
    float dist_sq = to_block.x*to_block.x + to_block.y*to_block.y + to_block.z*to_block.z;
    float render_dist_sq = render_distance * render_distance;

    if (dist_sq > render_dist_sq) {
        return false;
    }

    // always render blocks within 15 units of player (exempt from FOV culling)
    if (dist_sq < 225.0f) return true;  // 15^2 = 225

    // normalize direction to block
    float dist = sqrtf(dist_sq);
    if (dist < 0.1f) return true;

    float inv_dist = 1.0f / dist;
    to_block.x *= inv_dist;
    to_block.y *= inv_dist;
    to_block.z *= inv_dist;

    // check if block is in front of camera
    float depth = to_block.x * cam_forward.x + to_block.y * cam_forward.y + to_block.z * cam_forward.z;
    if (depth <= 0) return false;

    // block angular size for margin
    float block_angular_size = atanf(0.5f / (dist > 0.5f ? dist : 0.5f));

    // project block direction onto right and up vectors
    float right_proj = to_block.x * cam_right.x + to_block.y * cam_right.y + to_block.z * cam_right.z;
    float up_proj = to_block.x * cam_up.x + to_block.y * cam_up.y + to_block.z * cam_up.z;

    // check if angles are within FOV bounds using pre-computed tangent values
    if (fabsf(right_proj / depth) > (half_horiz_tan + block_angular_size)) return false;
    if (fabsf(up_proj / depth) > (half_vert_tan + block_angular_size)) return false;

    return true;
}

// draw only the visible faces of a cube (faces pointing toward camera and not occluded by neighbors)
void draw_cube_faces(Vector3 pos, float size, Color color, Vector3 cam_pos, Color wire_color, World* world, int block_x, int block_y, int block_z, BlockType block_type)
{
    Vector3 to_cam = vec3_sub(cam_pos, pos);
    float h = size / 2.0f;

    // Get texture for this block type
    Texture2D texture = world_get_block_texture(world, block_type);
    bool has_texture = texture.id > 0;

    // face normals and vertices (in pairs: v1, v2, v3, v4) with texture coordinates
    // also include neighbor coordinates for occlusion checking
    struct {
        Vector3 normal;
        Vector3 v[4];
        Vector2 uv[4];
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
            {{0, 1}, {0, 0}, {1, 0}, {1, 1}},
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
            {{1, 1}, {1, 0}, {0, 0}, {0, 1}},
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
            {{0, 0}, {1, 0}, {1, 1}, {0, 1}},
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
            {{1, 0}, {0, 0}, {0, 1}, {1, 1}},
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
            {{0, 1}, {1, 1}, {1, 0}, {0, 0}},
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
            {{1, 1}, {0, 1}, {0, 0}, {1, 0}},
            block_x, block_y, block_z - 1,
            5
        }
    };

    // draw each face only if it points toward camera AND has no solid neighbor blocking it
    bool texture_set = false;
    if (has_texture) {
        rlSetTexture(texture.id);
        texture_set = true;
    }

    for (int i = 0; i < 6; i++) {
        float dot = to_cam.x * faces[i].normal.x + to_cam.y * faces[i].normal.y + to_cam.z * faces[i].normal.z;

        // face points toward camera
        if (dot > 0) {
            // check if neighbor is air (face is exposed)
            BlockType neighbor = world_get_block(world, faces[i].neighbor_x, faces[i].neighbor_y, faces[i].neighbor_z);
            if (neighbor == BLOCK_AIR) {
                // Apply lighting based on adjacent air block's sky access
                Color lit_color = apply_face_lighting(color, faces[i].face_index, world, faces[i].neighbor_x, faces[i].neighbor_y, faces[i].neighbor_z);

                if (has_texture) {
                    // Draw textured quad with rlgl - minimal overhead
                    rlBegin(RL_QUADS);
                    rlColor4ub(lit_color.r, lit_color.g, lit_color.b, lit_color.a);

                    rlTexCoord2f(faces[i].uv[0].x, faces[i].uv[0].y);
                    rlVertex3f(faces[i].v[0].x, faces[i].v[0].y, faces[i].v[0].z);

                    rlTexCoord2f(faces[i].uv[1].x, faces[i].uv[1].y);
                    rlVertex3f(faces[i].v[1].x, faces[i].v[1].y, faces[i].v[1].z);

                    rlTexCoord2f(faces[i].uv[2].x, faces[i].uv[2].y);
                    rlVertex3f(faces[i].v[2].x, faces[i].v[2].y, faces[i].v[2].z);

                    rlTexCoord2f(faces[i].uv[3].x, faces[i].uv[3].y);
                    rlVertex3f(faces[i].v[3].x, faces[i].v[3].y, faces[i].v[3].z);

                    rlEnd();
                } else {
                    // Fallback to colored triangles if texture not loaded
                    DrawTriangle3D(faces[i].v[0], faces[i].v[1], faces[i].v[2], lit_color);
                    DrawTriangle3D(faces[i].v[0], faces[i].v[2], faces[i].v[3], lit_color);
                }

                // draw wireframe if enabled
                if (RENDER_WIREFRAMES) {
                    DrawLine3D(faces[i].v[0], faces[i].v[1], wire_color);
                    DrawLine3D(faces[i].v[1], faces[i].v[2], wire_color);
                    DrawLine3D(faces[i].v[2], faces[i].v[3], wire_color);
                    DrawLine3D(faces[i].v[3], faces[i].v[0], wire_color);
                }
            }
        }
    }

    // Unset texture after drawing all faces
    if (texture_set) {
        rlSetTexture(0);
    }
}

// Raycast from camera to find the block being looked at
// Returns true if a block was hit, false otherwise
// out_block_x/y/z: the coordinates of the block hit
// out_adjacent_x/y/z: the coordinates where a new block would be placed (adjacent to hit block)
bool raycast_block(World* world, Camera3D camera, float max_distance,
                   int* out_block_x, int* out_block_y, int* out_block_z,
                   int* out_adjacent_x, int* out_adjacent_y, int* out_adjacent_z)
{
    Vector3 ray_origin = camera.position;
    Vector3 ray_dir = vec3_normalize(vec3_sub(camera.target, camera.position));

    float step = 0.05f;  // Smaller step size for accuracy
    float distance = 0.0f;

    Vector3 prev_pos = ray_origin;

    while (distance < max_distance) {
        Vector3 current_pos = vec3_add(ray_origin, vec3_scale(ray_dir, distance));

        // Use proper floor for negative coordinates
        int block_x = (int)floorf(current_pos.x);
        int block_y = (int)floorf(current_pos.y);
        int block_z = (int)floorf(current_pos.z);

        // Check if we're in a block
        BlockType block = world_get_block(world, block_x, block_y, block_z);
        if (block != BLOCK_AIR) {
            // We hit a block - the hit block is the current one
            *out_block_x = block_x;
            *out_block_y = block_y;
            *out_block_z = block_z;

            // Adjacent block is where we came from (previous block)
            int prev_block_x = (int)floorf(prev_pos.x);
            int prev_block_y = (int)floorf(prev_pos.y);
            int prev_block_z = (int)floorf(prev_pos.z);

            *out_adjacent_x = prev_block_x;
            *out_adjacent_y = prev_block_y;
            *out_adjacent_z = prev_block_z;

            return true;
        }

        prev_pos = current_pos;
        distance += step;
    }

    return false;  // No block hit
}

