#ifndef WORLD_H
#define WORLD_H

#include <stdint.h>
#include "raylib.h"

// Block types
typedef enum {
    BLOCK_AIR = 0,
    BLOCK_STONE = 1
} BlockType;

// World dimensions
#define WORLD_WIDTH 100
#define WORLD_HEIGHT 50
#define WORLD_DEPTH 100

// World offsets (center the world at 0, y, 0)
#define WORLD_OFFSET_X (-(WORLD_WIDTH / 2))
#define WORLD_OFFSET_Z (-(WORLD_DEPTH / 2))

// Block structure
typedef struct {
    BlockType type;
} Block;

// World structure
typedef struct {
    Block blocks[WORLD_HEIGHT][WORLD_DEPTH][WORLD_WIDTH];
} World;

// Player physics constants
#define PLAYER_HEIGHT 2.0f
#define PLAYER_RADIUS 0.4f
#define PLAYER_SPEED 10.0f    // blocks per second
#define GRAVITY 25.0f         // blocks per second squared
#define JUMP_FORCE 9.0f      // blocks per second

// Player structure
typedef struct {
    Vector3 position;    // Head position
    Vector3 velocity;    // Current velocity
    bool on_ground;      // Is player touching ground
    bool jump_used;      // Has jump been used in this key press
} Player;

// Function declarations
World* world_create(void);
void world_free(World* world);
void world_set_block(World* world, int x, int y, int z, BlockType type);
BlockType world_get_block(World* world, int x, int y, int z);
Color world_get_block_color(BlockType type);
void world_generate_prism(World* world);
void world_system_init(void);
bool world_save(World* world, const char* world_name);
bool world_load(World* world, const char* world_name);

// Physics functions
Player* player_create(float x, float y, float z);
void player_free(Player* player);
bool world_check_collision_sphere(World* world, Vector3 pos, float radius);
void player_update(Player* player, World* world, float dt);
void player_move_input(Player* player, Vector3 forward, Vector3 right, float dt);

#endif
