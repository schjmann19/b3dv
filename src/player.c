#include <stdlib.h>
#include <math.h>

#include "player.h"
#include "world.h"
#include "vec_math.h"

// Create a player
Player* player_create(float x, float y, float z)
{
    Player* player = (Player*)malloc(sizeof(Player));
    player->position = (Vector3){ x, y, z };
    player->prev_position = player->position;
    player->velocity = (Vector3){ 0, 0, 0 };
    player->on_ground = false;
    player->jump_used = false;
    player->selected_block = BLOCK_STONE;  // Default to stone
    return player;
}

// Free player memory
void player_free(Player* player)
{
    if (player) {
        free(player);
    }
}

// Handle player movement input
void player_move_input(Player* player, Vector3 forward, Vector3 right)
{
    // Extract horizontal (XZ) component of right vector and normalize it.
    // Then derive a stable horizontal forward from world up and right to avoid issues when
    // the camera is looking straight up/down (forward XZ component near zero).
    Vector3 right_horiz = (Vector3){ right.x, 0, right.z };
    float right_len = sqrtf(right_horiz.x * right_horiz.x + right_horiz.y * right_horiz.y + right_horiz.z * right_horiz.z);
    if (right_len < 1e-6f) {
        // Fallback: if right is degenerate, use world X axis
        right_horiz = (Vector3){ 1.0f, 0.0f, 0.0f };
    } else {
        right_horiz.x /= right_len;
        right_horiz.y /= right_len;
        right_horiz.z /= right_len;
    }

    // Derive forward_horiz as cross(world_up, right_horiz) which gives a horizontal forward
    // consistent with yaw even when the camera is looking vertically.
    Vector3 forward_horiz = vec3_cross((Vector3){0, 1, 0}, right_horiz);
    forward_horiz = vec3_normalize(forward_horiz);



    // Shifting (sneak) and sprinting support
    player->shifting = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    bool sprinting = IsKeyDown(KEY_LEFT_CONTROL);
    float move_speed = PLAYER_SPEED;
    if (player->shifting) {
        move_speed *= 0.5f;
    } else if (sprinting) {
        move_speed *= 1.5f;
    }

    Vector3 move = { 0, 0, 0 };
    if (IsKeyDown(KEY_W)) {
        move = vec3_add(move, vec3_scale(forward_horiz, move_speed));
    }
    if (IsKeyDown(KEY_S)) {
        move = vec3_add(move, vec3_scale(forward_horiz, -move_speed));
    }
    if (IsKeyDown(KEY_D)) {
        move = vec3_add(move, vec3_scale(right_horiz, move_speed));
    }
    if (IsKeyDown(KEY_A)) {
        move = vec3_add(move, vec3_scale(right_horiz, -move_speed));
    }

    // Normalize movement to prevent diagonal speedup
    float move_len = sqrtf(move.x * move.x + move.z * move.z);
    if (move_len > move_speed) {
        float scale = move_speed / move_len;
        move.x *= scale;
        move.z *= scale;
    }

    if (IsKeyDown(KEY_SPACE)) {
        if (player->on_ground && !player->jump_used) {
            player->velocity.y = JUMP_FORCE;
            player->on_ground = false;
            player->jump_used = true;
        }
    } else {
        player->jump_used = false;
    }

    player->velocity.x = move.x;
    player->velocity.z = move.z;
}

// Check collision for a rectangular prism (box) - AABB collision
bool world_check_collision_box(World* world, Vector3 center_pos, float width, float height, float depth)
{
    float half_width = width / 2.0f;
    float half_height = height / 2.0f;
    float half_depth = depth / 2.0f;

    float box_min_x = center_pos.x - half_width;
    float box_max_x = center_pos.x + half_width;
    float box_min_y = center_pos.y - half_height;
    float box_max_y = center_pos.y + half_height;
    float box_min_z = center_pos.z - half_depth;
    float box_max_z = center_pos.z + half_depth;

    int min_x = (int)floorf(box_min_x - 1.0f);
    int max_x = (int)ceilf(box_max_x + 1.0f);
    int min_y = (int)floorf(box_min_y - 1.0f);
    int max_y = (int)ceilf(box_max_y + 1.0f);
    int min_z = (int)floorf(box_min_z - 1.0f);
    int max_z = (int)ceilf(box_max_z + 1.0f);

    for (int y = min_y; y <= max_y; y++) {
        for (int z = min_z; z <= max_z; z++) {
            for (int x = min_x; x <= max_x; x++) {
                BlockType block = world_get_block(world, x, y, z);
                if (block != BLOCK_AIR) {
                    float block_min_x = (float)x;
                    float block_max_x = (float)(x + 1);
                    float block_min_y = (float)y;
                    float block_max_y = (float)(y + 1);
                    float block_min_z = (float)z;
                    float block_max_z = (float)(z + 1);

                    // AABB to AABB collision check
                    if (box_max_x > block_min_x && box_min_x < block_max_x &&
                        box_max_y > block_min_y && box_min_y < block_max_y &&
                        box_max_z > block_min_z && box_min_z < block_max_z) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

// Update player physics
void player_update(Player* player, World* world, float dt)
{
    // Store previous position for speedometer
    player->prev_position = player->position;
    player->velocity.y -= GRAVITY * dt;

    if (player->velocity.y < -50.0f) {
        player->velocity.y = -50.0f;
    }

    Vector3 new_pos = (Vector3){
        player->position.x + player->velocity.x * dt,
        player->position.y + player->velocity.y * dt,
        player->position.z + player->velocity.z * dt
    };

    // Edge safety: if shifting, prevent walking off ledges
    if (player->shifting && player->on_ground) {
        // Predict next foot position (center of feet, 0.1 below player)
        Vector3 foot_pos = new_pos;
        foot_pos.y -= PLAYER_HEIGHT - 0.1f;
        // Check a grid under the player's feet (collision box)
        float half = 0.3f; // half width of collision box
        float step = 0.08f; // grid step (smaller = smoother)
        bool any_supported = false;
        for (float dx = -half; dx <= half; dx += step) {
            for (float dz = -half; dz <= half; dz += step) {
                int bx = (int)floorf(foot_pos.x + dx);
                int by = (int)floorf(foot_pos.y - 0.05f);
                int bz = (int)floorf(foot_pos.z + dz);
                if (world_get_block(world, bx, by, bz) != BLOCK_AIR) {
                    any_supported = true;
                    break;
                }
            }
            if (any_supported) break;
        }
        if (!any_supported) {
            // Don't allow movement if the entire area under the box is unsupported
            new_pos.x = player->position.x;
            new_pos.z = player->position.z;
        }
    }

    // Use box collision (0.6 blocks wide and deep)
    if (!world_check_collision_box(world, new_pos, 0.6f, PLAYER_HEIGHT, 0.6f)) {
        player->position = new_pos;
        player->on_ground = false;
    } else {
        Vector3 slide_pos = player->position;

        // Slide X
        Vector3 test_x = (Vector3){
            player->position.x + player->velocity.x * dt,
            player->position.y,
            player->position.z
        };
        bool allow_x = !world_check_collision_box(world, test_x, 0.6f, PLAYER_HEIGHT, 0.6f);
        if (allow_x && player->shifting && player->on_ground) {
            // Edge safety for X: only allow if player would still be supported after moving
            Vector3 below_test = (Vector3){ test_x.x, test_x.y - 0.1f, test_x.z };
            if (!world_check_collision_box(world, below_test, 0.6f, PLAYER_HEIGHT, 0.6f)) {
                allow_x = false;  // Would fall, don't allow
            }
        }
        if (allow_x) {
            slide_pos.x = test_x.x;
        }

        // Slide Y
        Vector3 test_y = (Vector3){
            slide_pos.x,
            player->position.y + player->velocity.y * dt,
            player->position.z
        };
        if (!world_check_collision_box(world, test_y, 0.6f, PLAYER_HEIGHT, 0.6f)) {
            slide_pos.y = test_y.y;
        } else {
            if (player->velocity.y < 0) {
                player->on_ground = true;
                player->jump_used = false;
            }
            player->velocity.y = 0;
        }

        // Slide Z
        Vector3 test_z = (Vector3){
            slide_pos.x,
            slide_pos.y,
            player->position.z + player->velocity.z * dt
        };
        bool allow_z = !world_check_collision_box(world, test_z, 0.6f, PLAYER_HEIGHT, 0.6f);
        if (allow_z && player->shifting && player->on_ground) {
            // Edge safety for Z: only allow if player would still be supported after moving
            Vector3 below_test = (Vector3){ test_z.x, test_z.y - 0.1f, test_z.z };
            if (!world_check_collision_box(world, below_test, 0.6f, PLAYER_HEIGHT, 0.6f)) {
                allow_z = false;  // Would fall, don't allow
            }
        }
        if (allow_z) {
            slide_pos.z = test_z.z;
        }

        player->position = slide_pos;

        Vector3 below = (Vector3){ player->position.x, player->position.y - 0.1f, player->position.z };
        if (world_check_collision_box(world, below, 0.6f, PLAYER_HEIGHT, 0.6f)) {
            player->on_ground = true;
            player->velocity.y = 0;
            player->jump_used = false;
        }
    }
}
