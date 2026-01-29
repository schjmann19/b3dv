#ifndef PLAYER_H
#define PLAYER_H

#include "raylib.h"
#include "world.h"

// Player physics constants
#define PLAYER_HEIGHT 1.9f
#define PLAYER_RADIUS 0.35f
#define PLAYER_SPEED 5.5f     // blocks per second
#define GRAVITY 35.0f         // blocks per second squared
#define JUMP_FORCE 9.0f       // blocks per second

// Player structure
typedef struct {
    Vector3 position;    // Head position
    Vector3 velocity;    // Current velocity
    bool on_ground;      // Is player touching ground
    bool jump_used;      // Has jump been used in this key press
} Player;

// Function declarations
Player* player_create(float x, float y, float z);
void player_free(Player* player);
void player_move_input(Player* player, Vector3 forward, Vector3 right, float dt);
void player_update(Player* player, World* world, float dt);
bool world_check_collision_sphere(World* world, Vector3 pos, float radius);
bool world_check_collision_capsule(World* world, Vector3 center_pos, float height, float radius);

#endif
