#include <math.h>

#include "raylib.h"
#include "../../include/rendering.h"
#include "rlgl.h"
#include "../../include/vec_math.h"
#include "../../include/world.h"

// Rendering constants
#define BLOCK_NEAR_EXEMPTION_DIST_SQ 225.0f // 15^2
#define BLOCK_MIN_DIST 0.1f
#define BLOCK_RADIUS 0.5f

// check if a block has any face visible (exposed to air)
bool has_visible_face(World *world, int x, int y, int z, Vector3 block_pos, Vector3 cam_pos) {
    BlockType current = world_get_block(world, x, y, z);
    if (current == BLOCK_AIR) {
        return false;
    }
    // Check all 6 neighbors - if any face is exposed, this block has a visible face
    if (block_face_is_exposed(current, world_get_block(world, x + 1, y, z)) ||
        block_face_is_exposed(current, world_get_block(world, x - 1, y, z)) ||
        block_face_is_exposed(current, world_get_block(world, x, y + 1, z)) ||
        block_face_is_exposed(current, world_get_block(world, x, y - 1, z)) ||
        block_face_is_exposed(current, world_get_block(world, x, y, z + 1)) ||
        block_face_is_exposed(current, world_get_block(world, x, y, z - 1))) {
        return true; // has at least one exposed face
    }

    return false; // completely surrounded by non-exposed neighbors
}

// check if a block is occluded (completely surrounded by other blocks)
bool is_block_occluded(World *world, int x, int y, int z) {
    BlockType current = world_get_block(world, x, y, z);
    if (current == BLOCK_AIR) {
        return true;
    }
    // check all 6 neighbors - if any face is exposed, the block is not occluded
    if (block_face_is_exposed(current, world_get_block(world, x + 1, y, z)) ||
        block_face_is_exposed(current, world_get_block(world, x - 1, y, z)) ||
        block_face_is_exposed(current, world_get_block(world, x, y + 1, z)) ||
        block_face_is_exposed(current, world_get_block(world, x, y - 1, z)) ||
        block_face_is_exposed(current, world_get_block(world, x, y, z + 1)) ||
        block_face_is_exposed(current, world_get_block(world, x, y, z - 1))) {
        return false;
    }
    return true;
}

// Check if a block is visible in the camera frustum
// Uses precomputed FOV tangent values if provided for performance
bool is_block_visible_fast(Vector3 block_pos, Vector3 cam_pos, Vector3 cam_forward,
                           Vector3 cam_right, Vector3 cam_up, float render_distance,
                           float half_vert_tan, float half_horiz_tan) {
    Vector3 to_block = vec3_sub(block_pos, cam_pos);
    float dist_sq = to_block.x * to_block.x + to_block.y * to_block.y + to_block.z * to_block.z;
    float render_dist_sq = render_distance * render_distance;

    if (dist_sq > render_dist_sq) {
        return false;
    }

    // Always render blocks within exemption distance (exempt from FOV culling)
    if (dist_sq < BLOCK_NEAR_EXEMPTION_DIST_SQ) {
        return true;
    }

    // Normalize direction to block
    float dist = sqrtf(dist_sq);
    if (dist < BLOCK_MIN_DIST) {
        return true;
    }

    float inv_dist = 1.0f / dist;
    to_block.x *= inv_dist;
    to_block.y *= inv_dist;
    to_block.z *= inv_dist;

    // Check if block is in front of camera
    float depth = to_block.x * cam_forward.x + to_block.y * cam_forward.y + to_block.z * cam_forward.z;
    if (depth <= 0) {
        return false;
    }

    // Block angular size for margin
    float block_angular_size = atanf(BLOCK_RADIUS / (dist > BLOCK_RADIUS ? dist : BLOCK_RADIUS));

    // Project block direction onto right and up vectors
    float right_proj = to_block.x * cam_right.x + to_block.y * cam_right.y + to_block.z * cam_right.z;
    float up_proj = to_block.x * cam_up.x + to_block.y * cam_up.y + to_block.z * cam_up.z;

    // Check if angles are within FOV bounds using pre-computed tangent values
    if (fabsf(right_proj / depth) > (half_horiz_tan + block_angular_size)) {
        return false;
    }
    if (fabsf(up_proj / depth) > (half_vert_tan + block_angular_size)) {
        return false;
    }

    return true;
}

// draw only the visible faces of a cube (faces pointing toward camera and not occluded by neighbors)
void draw_cube_faces(Vector3 pos, float size, Color color, Vector3 cam_pos, Color wire_color, World *world, int block_x, int block_y, int block_z, BlockType block_type, uint8_t exposed_faces, bool show_wireframe) {
    Vector3 to_cam = vec3_sub(cam_pos, pos);
    float h = size / 2.0f;

    // Face normals for backface culling (pointing outward)
    Vector3 face_normals[6] = {
        {1, 0, 0},  // +X (right)
        {-1, 0, 0}, // -X (left)
        {0, 1, 0},  // +Y (top)
        {0, -1, 0}, // -Y (bottom)
        {0, 0, 1},  // +Z (front)
        {0, 0, -1}  // -Z (back)
    };

    Vector3 face_positions[6][4] = {
        // right (+X)
        {{pos.x + h, pos.y - h, pos.z - h}, {pos.x + h, pos.y + h, pos.z - h}, {pos.x + h, pos.y + h, pos.z + h}, {pos.x + h, pos.y - h, pos.z + h}},
        // left (-X)
        {{pos.x - h, pos.y - h, pos.z + h}, {pos.x - h, pos.y + h, pos.z + h}, {pos.x - h, pos.y + h, pos.z - h}, {pos.x - h, pos.y - h, pos.z - h}},
        // top (+Y)
        {{pos.x - h, pos.y + h, pos.z + h}, {pos.x + h, pos.y + h, pos.z + h}, {pos.x + h, pos.y + h, pos.z - h}, {pos.x - h, pos.y + h, pos.z - h}},
        // bottom (-Y)
        {{pos.x + h, pos.y - h, pos.z + h}, {pos.x - h, pos.y - h, pos.z + h}, {pos.x - h, pos.y - h, pos.z - h}, {pos.x + h, pos.y - h, pos.z - h}},
        // front (+Z)
        {{pos.x - h, pos.y - h, pos.z + h}, {pos.x + h, pos.y - h, pos.z + h}, {pos.x + h, pos.y + h, pos.z + h}, {pos.x - h, pos.y + h, pos.z + h}},
        // back (-Z)
        {{pos.x + h, pos.y - h, pos.z - h}, {pos.x - h, pos.y - h, pos.z - h}, {pos.x - h, pos.y + h, pos.z - h}, {pos.x + h, pos.y + h, pos.z - h}}};

    Vector2 face_uv[6][4] = {
        {{0, 1}, {0, 0}, {1, 0}, {1, 1}}, // +X
        {{1, 1}, {1, 0}, {0, 0}, {0, 1}}, // -X
        {{0, 0}, {1, 0}, {1, 1}, {0, 1}}, // +Y
        {{1, 0}, {0, 0}, {0, 1}, {1, 1}}, // -Y
        {{0, 1}, {1, 1}, {1, 0}, {0, 0}}, // +Z
        {{1, 1}, {0, 1}, {0, 0}, {1, 0}}  // -Z
    };

    // Face shading: how bright each face appears based on orientation
    float face_shading[6] = {
        0.85f, // +X (right side) - medium
        0.85f, // -X (left side) - medium
        1.0f,  // +Y (top) - fully lit
        0.6f,  // -Y (bottom) - dark
        0.9f,  // +Z (front) - slightly brighter
        0.8f   // -Z (back) - slightly darker
    };

    bool is_transparent = (block_type == BLOCK_GLASS);
    Color face_colors[6];
    Color face_texture_tints[6];
    unsigned char texture_alpha = 255;

    for (int i = 0; i < 6; i++) {
        float face_brightness = face_shading[i];
        face_colors[i].r = (unsigned char)(color.r * face_brightness);
        face_colors[i].g = (unsigned char)(color.g * face_brightness);
        face_colors[i].b = (unsigned char)(color.b * face_brightness);
        face_colors[i].a = is_transparent ? 0 : color.a;

        unsigned char brightness = (unsigned char)(255.0f * face_brightness);
        face_texture_tints[i].r = brightness;
        face_texture_tints[i].g = brightness;
        face_texture_tints[i].b = brightness;
        face_texture_tints[i].a = texture_alpha;
    }

    int face_order[6];
    int face_count = 0;

    if (is_transparent) {
        // Sort transparent faces back-to-front based on face center distance to camera.
        typedef struct {
            int face;
            float dist_sq;
        } FaceSortEntry;

        FaceSortEntry face_sort[6];

        for (int face = 0; face < 6; face++) {
            if (!(exposed_faces & (1 << face))) {
                continue;
            }
            Vector3 center = {
                (face_positions[face][0].x + face_positions[face][1].x + face_positions[face][2].x + face_positions[face][3].x) * 0.25f,
                (face_positions[face][0].y + face_positions[face][1].y + face_positions[face][2].y + face_positions[face][3].y) * 0.25f,
                (face_positions[face][0].z + face_positions[face][1].z + face_positions[face][2].z + face_positions[face][3].z) * 0.25f
            };
            Vector3 diff = vec3_sub(center, cam_pos);
            face_sort[face_count].face = face;
            face_sort[face_count].dist_sq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
            face_count++;
        }

        for (int i = 0; i < face_count; i++) {
            for (int j = i + 1; j < face_count; j++) {
                if (face_sort[j].dist_sq > face_sort[i].dist_sq) {
                    FaceSortEntry temp = face_sort[i];
                    face_sort[i] = face_sort[j];
                    face_sort[j] = temp;
                }
            }
        }

        for (int i = 0; i < face_count; i++) {
            face_order[i] = face_sort[i].face;
        }
    } else {
        for (int face = 0; face < 6; face++) {
            if (!(exposed_faces & (1 << face))) {
                continue;
            }
            face_order[face_count++] = face;
        }
    }

    if (is_transparent) {
        BeginBlendMode(BLEND_ALPHA);
        rlDisableDepthMask();
    }

    rlDisableBackfaceCulling();

    for (int face_index = 0; face_index < face_count; face_index++) {
        int face = face_order[face_index];

        // Backface culling is disabled for block faces here to avoid transient terrain holes
        // when the camera passes near a block plane edge during jumps.
        BlockFace face_type;
        switch (face) {
        case 2: // +Y top
            face_type = BLOCK_FACE_TOP;
            break;
        case 3: // -Y bottom
            face_type = BLOCK_FACE_BOTTOM;
            break;
        default:
            face_type = BLOCK_FACE_SIDE;
            break;
        }

        Texture2D face_texture = world_get_block_face_texture(world, block_type, face_type);
        bool face_has_texture = face_texture.id > 0;

        if (face_has_texture) {
            rlSetTexture(face_texture.id);
            rlBegin(RL_QUADS);
            rlColor4ub(face_texture_tints[face].r, face_texture_tints[face].g, face_texture_tints[face].b, face_texture_tints[face].a);

            for (int v = 0; v < 4; v++) {
                rlTexCoord2f(face_uv[face][v].x, face_uv[face][v].y);
                rlVertex3f(face_positions[face][v].x, face_positions[face][v].y, face_positions[face][v].z);
            }
            rlEnd();
            rlSetTexture(0);
        } else {
            DrawTriangle3D(face_positions[face][0], face_positions[face][1], face_positions[face][2], face_colors[face]);
            DrawTriangle3D(face_positions[face][0], face_positions[face][2], face_positions[face][3], face_colors[face]);
        }
    }

    rlEnableBackfaceCulling();

    if (is_transparent) {
        rlEnableDepthMask();
        EndBlendMode();
    }

    // Draw wireframe if enabled
    if (show_wireframe) {
        for (int face = 0; face < 6; face++) {
            // Skip faces that are not exposed or face away from camera
            if (!(exposed_faces & (1 << face))) {
                continue; // Face is occluded by neighbor
            }

            // Backface culling: skip faces facing away from camera
            float dot = to_cam.x * face_normals[face].x + to_cam.y * face_normals[face].y + to_cam.z * face_normals[face].z;
            if (dot <= 0.0f) {
                continue; // Face points away from camera
            }

            // Draw wireframe edges for this face (quad outline)
            // Draw 4 edges of the quad
            Vector3 *verts = face_positions[face];
            // Edge 0-1
            DrawLine3D(verts[0], verts[1], wire_color);
            // Edge 1-2
            DrawLine3D(verts[1], verts[2], wire_color);
            // Edge 2-3
            DrawLine3D(verts[2], verts[3], wire_color);
            // Edge 3-0
            DrawLine3D(verts[3], verts[0], wire_color);
        }
    }
}

// Check if a chunk's bounding box is within the camera frustum
// This is the most efficient culling - eliminates entire chunks at once before iterating blocks
bool is_chunk_in_frustum(Chunk *chunk, Vector3 cam_pos, Vector3 cam_forward,
                         Vector3 cam_right, Vector3 cam_up, float render_distance,
                         float half_vert_tan, float half_horiz_tan, Vector3 camera_offset) {
    // Chunk world bounds
    float chunk_min_x = chunk->chunk_x * CHUNK_WIDTH - camera_offset.x;
    float chunk_max_x = chunk_min_x + CHUNK_WIDTH;
    float chunk_min_y = chunk->chunk_y * CHUNK_HEIGHT - camera_offset.y;
    float chunk_max_y = chunk_min_y + CHUNK_HEIGHT;
    float chunk_min_z = chunk->chunk_z * CHUNK_DEPTH - camera_offset.z;
    float chunk_max_z = chunk_min_z + CHUNK_DEPTH;

    // Chunk center and radius for distance/back-plane checks
    float chunk_center_x = (chunk_min_x + chunk_max_x) * 0.5f;
    float chunk_center_y = (chunk_min_y + chunk_max_y) * 0.5f;
    float chunk_center_z = (chunk_min_z + chunk_max_z) * 0.5f;

    float dx = chunk_center_x - cam_pos.x;
    float dy = chunk_center_y - cam_pos.y;
    float dz = chunk_center_z - cam_pos.z;
    float dist_sq = dx * dx + dy * dy + dz * dz;

    // Hard distance limit
    float render_dist_sq = render_distance * render_distance;
    if (dist_sq > render_dist_sq) {
        return false;
    }

    float dist = sqrtf(dist_sq);

    // Back-plane culling: if chunk is behind camera, skip it
    float depth = dx * cam_forward.x + dy * cam_forward.y + dz * cam_forward.z;
    if (depth < -CHUNK_WIDTH) { // Account for chunk size
        return false;
    }

    // If very close to camera, always render (inside frustum)
    if (dist < BLOCK_NEAR_EXEMPTION_DIST_SQ) {
        return true;
    }

    // Simple bounding sphere frustum test: check if chunk bounding sphere is within FOV
    float inv_dist = 1.0f / (dist > BLOCK_MIN_DIST ? dist : BLOCK_MIN_DIST);
    float norm_dx = dx * inv_dist;
    float norm_dy = dy * inv_dist;
    float norm_dz = dz * inv_dist;

    // Angular size of chunk (using diagonal for safety)
    float chunk_radius = sqrtf(CHUNK_WIDTH * CHUNK_WIDTH + CHUNK_HEIGHT * CHUNK_HEIGHT + CHUNK_DEPTH * CHUNK_DEPTH) * 0.5f;
    float chunk_angular_size = atanf(chunk_radius / dist);

    // Check horizontal FOV
    float right_proj = norm_dx * cam_right.x + norm_dy * cam_right.y + norm_dz * cam_right.z;
    if (fabsf(right_proj / depth) > (half_horiz_tan + chunk_angular_size)) {
        return false;
    }

    // Check vertical FOV
    float up_proj = norm_dx * cam_up.x + norm_dy * cam_up.y + norm_dz * cam_up.z;
    if (fabsf(up_proj / depth) > (half_vert_tan + chunk_angular_size)) {
        return false;
    }

    return true; // Chunk is in frustum
}

// Raycast from camera to find the block being looked at
// Returns true if a block was hit, false otherwise
// out_block_x/y/z: the coordinates of the block hit
// out_adjacent_x/y/z: the coordinates where a new block would be placed (adjacent to hit block)
bool raycast_block(World *world, Camera3D camera, float max_distance,
                   int *out_block_x, int *out_block_y, int *out_block_z,
                   int *out_adjacent_x, int *out_adjacent_y, int *out_adjacent_z) {
    Vector3 ray_origin = camera.position;
    Vector3 ray_dir = vec3_normalize(vec3_sub(camera.target, camera.position));

    const float step = 0.1f; // Step size for raycast iterations
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

    return false; // No block hit
}
