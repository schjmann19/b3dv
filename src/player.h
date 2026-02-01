#ifndef PLAYER_H
#define PLAYER_H

#include "raylib.h"
#include "world.h"

// Player physics constants
#define PLAYER_HEIGHT 1.9f
#define PLAYER_RADIUS 0.35f
#define PLAYER_SPEED 5.5f     // blocks per second
#define GRAVITY 35.0f         // blocks per second squared
#define JUMP_FORCE 11.9f      // blocks per second

// Player structure
typedef struct {
    Vector3 position;    // Head position
    Vector3 velocity;    // Current velocity
    bool on_ground;      // Is player touching ground
    bool jump_used;      // Has jump been used in this key press
    BlockType selected_block;  // Currently selected block type for placement
} Player;

// Function declarations
Player* player_create(float x, float y, float z);
void player_free(Player* player);
void player_move_input(Player* player, Vector3 forward, Vector3 right, float dt);
void player_update(Player* player, World* world, float dt);
bool world_check_collision_box(World* world, Vector3 center_pos, float width, float height, float depth);

#endif
