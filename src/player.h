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

// Inventory constants
#define INVENTORY_SIZE 9      // Number of hotbar slots
#define INVENTORY_MAX_STACK 64 // Max items per stack
#define BIG_INVENTORY_ROWS 4  // Rows in big inventory (below hotbar)
#define BIG_INVENTORY_COLS 9  // Columns in big inventory
#define BIG_INVENTORY_SIZE (BIG_INVENTORY_ROWS * BIG_INVENTORY_COLS)  // 36 slots

// Inventory slot structure
typedef struct {
    BlockType type;      // Block type in this slot
    int count;           // Number of items in this stack
} InventorySlot;

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
    // Inventory
    InventorySlot inventory[INVENTORY_SIZE];  // Hotbar slots (0-8)
    InventorySlot big_inventory[BIG_INVENTORY_SIZE];  // Full inventory (36 slots)
    int selected_slot;  // Currently selected hotbar slot (0-8)
    bool inventory_open;  // Is big inventory UI open
    // UI drag-and-drop state
    InventorySlot held_slot; // Item held by cursor in inventory UI
    bool holding_item;
} Player;

// Function declarations
Player* player_create(float x, float y, float z);
void player_free(Player* player);
void player_move_input(Player* player, Vector3 forward, Vector3 right, bool flight_enabled);
void player_update(Player* player, World* world, float dt, bool flight_enabled);
bool world_check_collision_box(World* world, Vector3 center_pos, float width, float height, float depth);

// Inventory functions
void inventory_init(Player* player);
bool inventory_add_block(Player* player, BlockType block_type);
bool inventory_remove_block(Player* player, BlockType block_type);
int inventory_get_count(Player* player, BlockType block_type);
BlockType inventory_get_selected_block(Player* player);
void inventory_toggle_big(Player* player);
bool inventory_is_big_open(Player* player);
// Give items to player: returns true on success
bool inventory_give(Player* player, BlockType block_type, int count);

#endif
