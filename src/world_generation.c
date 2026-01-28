#include "world.h"
#include "vec_math.h"
#include "raylib.h"
#include <stdlib.h>
#include <math.h>

// Create and allocate a new world
World* world_create(void)
{
    World* world = (World*)malloc(sizeof(World));

    // Initialize all blocks to air
    for (int y = 0; y < WORLD_HEIGHT; y++) {
        for (int z = 0; z < WORLD_DEPTH; z++) {
            for (int x = 0; x < WORLD_WIDTH; x++) {
                world->blocks[y][z][x].type = BLOCK_AIR;
            }
        }
    }

    return world;
}

// Free world memory
void world_free(World* world)
{
    if (world) {
        free(world);
    }
}

// Set block at position
void world_set_block(World* world, int x, int y, int z, BlockType type)
{
    if (x >= 0 && x < WORLD_WIDTH &&
        y >= 0 && y < WORLD_HEIGHT &&
        z >= 0 && z < WORLD_DEPTH) {
        world->blocks[y][z][x].type = type;
    }
}

// Get block at position
BlockType world_get_block(World* world, int x, int y, int z)
{
    if (x >= 0 && x < WORLD_WIDTH &&
        y >= 0 && y < WORLD_HEIGHT &&
        z >= 0 && z < WORLD_DEPTH) {
        return world->blocks[y][z][x].type;
    }
    return BLOCK_AIR;
}

// Get color for block type
Color world_get_block_color(BlockType type)
{
    switch (type) {
        case BLOCK_STONE:
            return (Color){128, 128, 128, 255};  // Grey
        case BLOCK_AIR:
        default:
            return (Color){0, 0, 0, 0};  // Transparent
    }
}

// Generate a simple prism world (hollow rectangular prism filled with stone)
void world_generate_prism(World* world)
{
    // Create a filled rectangular prism of stone
    for (int y = 0; y < WORLD_HEIGHT; y++) {
        for (int z = 0; z < WORLD_DEPTH; z++) {
            for (int x = 0; x < WORLD_WIDTH; x++) {
                // Fill the entire prism with stone
                world_set_block(world, x, y, z, BLOCK_STONE);
            }
        }
    }
}

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
    Vector3 move = { 0, 0, 0 };

    if (IsKeyDown(KEY_W)) {
        move = vec3_add(move, vec3_scale(forward, PLAYER_SPEED));
    }
    if (IsKeyDown(KEY_S)) {
        move = vec3_add(move, vec3_scale(forward, -PLAYER_SPEED));
    }
    if (IsKeyDown(KEY_D)) {
        move = vec3_add(move, vec3_scale(right, PLAYER_SPEED));
    }
    if (IsKeyDown(KEY_A)) {
        move = vec3_add(move, vec3_scale(right, -PLAYER_SPEED));
    }

    // Smooth jump: detect key held down with state tracking
    if (IsKeyDown(KEY_SPACE)) {
        // Space is held - check if we can jump
        if (player->on_ground && !player->jump_used) {
            player->velocity.y = JUMP_FORCE;
            player->on_ground = false;
            player->jump_used = true;  // Mark jump as used for this key press
        }
    } else {
        // Space is not held - reset jump state for next press
        player->jump_used = false;
    }

    player->velocity.x = move.x;
    player->velocity.z = move.z;
}
bool world_check_collision_sphere(World* world, Vector3 pos, float radius)
{
    // Convert world position to block indices, accounting for world offset
    float local_x = pos.x - WORLD_OFFSET_X;
    float local_z = pos.z - WORLD_OFFSET_Z;

    // Check blocks around the position with expanded bounds to catch all potential collisions
    int min_x = (int)floorf(local_x - radius - 1.0f);
    int max_x = (int)ceilf(local_x + radius + 1.0f);
    int min_y = (int)floorf(pos.y - radius - 1.0f);
    int max_y = (int)ceilf(pos.y + radius + 1.0f);
    int min_z = (int)floorf(local_z - radius - 1.0f);
    int max_z = (int)ceilf(local_z + radius + 1.0f);

    for (int y = min_y; y <= max_y; y++) {
        for (int z = min_z; z <= max_z; z++) {
            for (int x = min_x; x <= max_x; x++) {
                BlockType block = world_get_block(world, x, y, z);
                if (block != BLOCK_AIR) {
                    // Clamp position to block bounds and check distance
                    // Block indices map to world coordinates with offset
                    float block_min_x = (float)x + WORLD_OFFSET_X;
                    float block_max_x = (float)(x + 1) + WORLD_OFFSET_X;
                    float block_min_y = (float)y;
                    float block_max_y = (float)(y + 1);
                    float block_min_z = (float)z + WORLD_OFFSET_Z;
                    float block_max_z = (float)(z + 1) + WORLD_OFFSET_Z;

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

// Update player physics
void player_update(Player* player, World* world, float dt)
{
    // Apply gravity
    player->velocity.y -= GRAVITY * dt;

    // Clamp velocity to prevent excessive falling
    if (player->velocity.y < -50.0f) {
        player->velocity.y = -50.0f;
    }

    // Try to move with velocity
    Vector3 new_pos = (Vector3){
        player->position.x + player->velocity.x * dt,
        player->position.y + player->velocity.y * dt,
        player->position.z + player->velocity.z * dt
    };

    // Check collision and resolve
    if (!world_check_collision_sphere(world, new_pos, PLAYER_RADIUS)) {
        player->position = new_pos;
        player->on_ground = false;
    } else {
        // Collision detected, try sliding along axes
        Vector3 slide_pos = player->position;

        // Try X axis
        Vector3 test_x = (Vector3){
            player->position.x + player->velocity.x * dt,
            player->position.y,
            player->position.z
        };
        if (!world_check_collision_sphere(world, test_x, PLAYER_RADIUS)) {
            slide_pos.x = test_x.x;
        }

        // Try Y axis
        Vector3 test_y = (Vector3){
            slide_pos.x,
            player->position.y + player->velocity.y * dt,
            player->position.z
        };
        if (!world_check_collision_sphere(world, test_y, PLAYER_RADIUS)) {
            slide_pos.y = test_y.y;
        } else {
            // Hit something on Y, stop vertical velocity and allow jumping again
            if (player->velocity.y < 0) {
                player->on_ground = true;
                player->jump_used = false;  // Reset jump when landing
            }
            player->velocity.y = 0;
        }

        // Try Z axis
        Vector3 test_z = (Vector3){
            slide_pos.x,
            slide_pos.y,
            player->position.z + player->velocity.z * dt
        };
        if (!world_check_collision_sphere(world, test_z, PLAYER_RADIUS)) {
            slide_pos.z = test_z.z;
        }

        player->position = slide_pos;

        // Check if on ground after sliding
        Vector3 below = (Vector3){ player->position.x, player->position.y - 0.1f, player->position.z };
        if (world_check_collision_sphere(world, below, PLAYER_RADIUS)) {
            player->on_ground = true;
            player->velocity.y = 0;
            player->jump_used = false;  // Reset jump when on ground
        }
    }
}
