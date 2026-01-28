#include "rendering.h"
#include "world.h"
#include <math.h>
#include "vec_math.h"

// check if a block has any face visible toward the camera
bool has_visible_face(World* world, int x, int y, int z, Vector3 block_pos, Vector3 cam_pos)
{
    Vector3 to_camera = vec3_sub(cam_pos, block_pos);

    // face normals (outward from block)
    Vector3 faces[6] = {
        {1, 0, 0},   // right (+X)
        {-1, 0, 0},  // left (-X)
        {0, 1, 0},   // top (+Y)
        {0, -1, 0},  // bottom (-Y)
        {0, 0, 1},   // front (+Z)
        {0, 0, -1}   // back (-Z)
    };

    int neighbor_x[6] = {1, -1, 0, 0, 0, 0};
    int neighbor_y[6] = {0, 0, 1, -1, 0, 0};
    int neighbor_z[6] = {0, 0, 0, 0, 1, -1};

    // check each face
    for (int i = 0; i < 6; i++) {
        // only care about faces pointing toward camera with meaningful angle
        float dot = to_camera.x * faces[i].x + to_camera.y * faces[i].y + to_camera.z * faces[i].z;

        if (dot > 0.5f) {
            // this face points toward camera - check if neighbor is air
            BlockType neighbor = world_get_block(world,
                x + neighbor_x[i],
                y + neighbor_y[i],
                z + neighbor_z[i]);

            if (neighbor == BLOCK_AIR) {
                return true;  // this face is exposed and visible
            }
        }
    }

    return false;  // no visible faces toward camera
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
    float dist = sqrtf(dist_sq);

    if (dist > render_distance) {
        return false;
    }

    // always render blocks within 15 units of player (exempt from FOV culling)
    if (dist < 15.0f) return true;

    if (dist < 0.1f) return true;

    // normalize direction to block
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

// draw only the visible faces of a cube (faces pointing toward camera)
void draw_cube_faces(Vector3 pos, float size, Color color, Vector3 cam_pos, Color wire_color)
{
    Vector3 to_cam = vec3_sub(cam_pos, pos);
    float h = size / 2.0f;

    // face normals and vertices (in pairs: v1, v2, v3, v4)
    struct {
        Vector3 normal;
        Vector3 v[4];
    } faces[6] = {
        // right (+X)
        {
            {1, 0, 0},
            {
                {pos.x + h, pos.y - h, pos.z - h},
                {pos.x + h, pos.y + h, pos.z - h},
                {pos.x + h, pos.y + h, pos.z + h},
                {pos.x + h, pos.y - h, pos.z + h}
            }
        },
        // left (-X)
        {
            {-1, 0, 0},
            {
                {pos.x - h, pos.y - h, pos.z + h},
                {pos.x - h, pos.y + h, pos.z + h},
                {pos.x - h, pos.y + h, pos.z - h},
                {pos.x - h, pos.y - h, pos.z - h}
            }
        },
        // top (+Y)
        {
            {0, 1, 0},
            {
                {pos.x - h, pos.y + h, pos.z + h},
                {pos.x + h, pos.y + h, pos.z + h},
                {pos.x + h, pos.y + h, pos.z - h},
                {pos.x - h, pos.y + h, pos.z - h}
            }
        },
        // bottom (-Y)
        {
            {0, -1, 0},
            {
                {pos.x - h, pos.y - h, pos.z + h},
                {pos.x + h, pos.y - h, pos.z + h},
                {pos.x + h, pos.y - h, pos.z - h},
                {pos.x - h, pos.y - h, pos.z - h}
            }
        },
        // front (+Z)
        {
            {0, 0, 1},
            {
                {pos.x - h, pos.y - h, pos.z + h},
                {pos.x + h, pos.y - h, pos.z + h},
                {pos.x + h, pos.y + h, pos.z + h},
                {pos.x - h, pos.y + h, pos.z + h}
            }
        },
        // back (-Z)
        {
            {0, 0, -1},
            {
                {pos.x + h, pos.y - h, pos.z - h},
                {pos.x - h, pos.y - h, pos.z - h},
                {pos.x - h, pos.y + h, pos.z - h},
                {pos.x + h, pos.y + h, pos.z - h}
            }
        }
    };

    // draw each face only if it points toward camera
    for (int i = 0; i < 6; i++) {
        float dot = to_cam.x * faces[i].normal.x + to_cam.y * faces[i].normal.y + to_cam.z * faces[i].normal.z;

        if (dot > 0) {  // face points toward camera
            DrawTriangle3D(faces[i].v[0], faces[i].v[1], faces[i].v[2], color);
            DrawTriangle3D(faces[i].v[0], faces[i].v[2], faces[i].v[3], color);

            // draw wireframe
            DrawLine3D(faces[i].v[0], faces[i].v[1], wire_color);
            DrawLine3D(faces[i].v[1], faces[i].v[2], wire_color);
            DrawLine3D(faces[i].v[2], faces[i].v[3], wire_color);
            DrawLine3D(faces[i].v[3], faces[i].v[0], wire_color);
        }
    }
}

