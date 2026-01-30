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
    player->velocity = (Vector3){ 0, 0, 0 };
    player->on_ground = false;
    player->jump_used = false;
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
void player_move_input(Player* player, Vector3 forward, Vector3 right, float dt)
{
    (void)dt;  // unused

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

    Vector3 move = { 0, 0, 0 };

    if (IsKeyDown(KEY_W)) {
        move = vec3_add(move, vec3_scale(forward_horiz, PLAYER_SPEED));
    }
    if (IsKeyDown(KEY_S)) {
        move = vec3_add(move, vec3_scale(forward_horiz, -PLAYER_SPEED));
    }
    if (IsKeyDown(KEY_D)) {
        move = vec3_add(move, vec3_scale(right_horiz, PLAYER_SPEED));
    }
    if (IsKeyDown(KEY_A)) {
        move = vec3_add(move, vec3_scale(right_horiz, -PLAYER_SPEED));
    }

    // Normalize movement to prevent diagonal speedup
    float move_len = sqrtf(move.x * move.x + move.z * move.z);
    if (move_len > PLAYER_SPEED) {
        float scale = PLAYER_SPEED / move_len;
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

// Check collision with a sphere
bool world_check_collision_sphere(World* world, Vector3 pos, float radius)
{
    int min_x = (int)floorf(pos.x - radius - 1.0f);
    int max_x = (int)ceilf(pos.x + radius + 1.0f);
    int min_y = (int)floorf(pos.y - radius - 1.0f);
    int max_y = (int)ceilf(pos.y + radius + 1.0f);
    int min_z = (int)floorf(pos.z - radius - 1.0f);
    int max_z = (int)ceilf(pos.z + radius + 1.0f);

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

                    float closest_x = (pos.x < block_min_x) ? block_min_x : (pos.x > block_max_x) ? block_max_x : pos.x;
                    float closest_y = (pos.y < block_min_y) ? block_min_y : (pos.y > block_max_y) ? block_max_y : pos.y;
                    float closest_z = (pos.z < block_min_z) ? block_min_z : (pos.z > block_max_z) ? block_max_z : pos.z;

                    float dx = pos.x - closest_x;
                    float dy = pos.y - closest_y;
                    float dz = pos.z - closest_z;
                    float dist_sq = dx*dx + dy*dy + dz*dz;

                    if (dist_sq < radius * radius) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

// Check collision for a capsule (pill shape) - checks multiple points along vertical height
bool world_check_collision_capsule(World* world, Vector3 center_pos, float height, float radius)
{
    // Sample collision at 5 points along the player's height
    int num_samples = 5;
    float sample_spacing = height / (float)(num_samples - 1);

    for (int i = 0; i < num_samples; i++) {
        float offset_y = -height / 2.0f + i * sample_spacing;
        Vector3 sample_pos = center_pos;
        sample_pos.y += offset_y;

        if (world_check_collision_sphere(world, sample_pos, radius)) {
            return true;
        }
    }

    return false;
}

// Update player physics
void player_update(Player* player, World* world, float dt)
{
    player->velocity.y -= GRAVITY * dt;

    if (player->velocity.y < -50.0f) {
        player->velocity.y = -50.0f;
    }

    Vector3 new_pos = (Vector3){
        player->position.x + player->velocity.x * dt,
        player->position.y + player->velocity.y * dt,
        player->position.z + player->velocity.z * dt
    };

    // Use capsule collision (0.7 blocks wide = 0.35 radius)
    if (!world_check_collision_capsule(world, new_pos, PLAYER_HEIGHT, 0.35f)) {
        player->position = new_pos;
        player->on_ground = false;
    } else {
        Vector3 slide_pos = player->position;

        Vector3 test_x = (Vector3){
            player->position.x + player->velocity.x * dt,
            player->position.y,
            player->position.z
        };
        if (!world_check_collision_capsule(world, test_x, PLAYER_HEIGHT, 0.35f)) {
            slide_pos.x = test_x.x;
        }

        Vector3 test_y = (Vector3){
            slide_pos.x,
            player->position.y + player->velocity.y * dt,
            player->position.z
        };
        if (!world_check_collision_capsule(world, test_y, PLAYER_HEIGHT, 0.35f)) {
            slide_pos.y = test_y.y;
        } else {
            if (player->velocity.y < 0) {
                player->on_ground = true;
                player->jump_used = false;
            }
            player->velocity.y = 0;
        }

        Vector3 test_z = (Vector3){
            slide_pos.x,
            slide_pos.y,
            player->position.z + player->velocity.z * dt
        };
        if (!world_check_collision_capsule(world, test_z, PLAYER_HEIGHT, 0.35f)) {
            slide_pos.z = test_z.z;
        }

        player->position = slide_pos;

        Vector3 below = (Vector3){ player->position.x, player->position.y - 0.1f, player->position.z };
        if (world_check_collision_capsule(world, below, PLAYER_HEIGHT, 0.35f)) {
            player->on_ground = true;
            player->velocity.y = 0;
            player->jump_used = false;
        }
    }
}
