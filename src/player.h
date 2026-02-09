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
#define FLY_SPEED 8.0f        // blocks per second (flying speed)
#define DOUBLE_TAP_THRESHOLD 0.3f  // time in seconds to detect double-tap

// Player structure
typedef struct {
    Vector3 position;    // Head position
    Vector3 prev_position; // Previous position (for actual movement)
    Vector3 velocity;    // Current velocity
    bool on_ground;      // Is player touching ground
    bool jump_used;      // Has jump been used in this key press
    BlockType selected_block;  // Currently selected block type for placement
    bool shifting;       // Is player holding shift (sneak)
    bool is_flying;      // Is player currently flying
    bool no_clip;        // Is player in no-clip mode (ignores collision)
    float space_press_time;  // Time since space was last pressed (for double-tap detection)
} Player;

// Function declarations
Player* player_create(float x, float y, float z);
void player_free(Player* player);
void player_move_input(Player* player, Vector3 forward, Vector3 right, bool flight_enabled);
void player_update(Player* player, World* world, float dt, bool flight_enabled);
bool world_check_collision_box(World* world, Vector3 center_pos, float width, float height, float depth);

#endif
