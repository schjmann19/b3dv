#ifndef MENU_H
#define MENU_H

#include "raylib.h"
#include "world.h"

// Menu state enumeration
typedef enum {
    MENU_STATE_MAIN,
    MENU_STATE_WORLD_SELECT,
    MENU_STATE_CREATE_WORLD,
    MENU_STATE_GAME
} MenuState;

// World info structure for display in world select menu
typedef struct {
    char name[256];
    char created[64];
    int chunk_count;
} WorldInfo;

// Menu system state
typedef struct {
    MenuState current_state;
    MenuState previous_state;
    WorldInfo* available_worlds;
    int world_count;
    int selected_world_index;
    bool should_start_game;
    char selected_world_name[256];
    // Create world dialog
    char new_world_name[256];
    int new_world_name_len;
    bool create_world_error;
    char create_world_error_msg[256];
} MenuSystem;

// Function declarations
MenuSystem* menu_system_create(void);
void menu_system_free(MenuSystem* menu);
void menu_scan_worlds(MenuSystem* menu);
void menu_draw_main(MenuSystem* menu, Font font);
void menu_draw_world_select(MenuSystem* menu, Font font);
void menu_draw_create_world(MenuSystem* menu, Font font);
void menu_update_input(MenuSystem* menu);

#endif
