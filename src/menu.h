#ifndef MENU_H
#define MENU_H

#include "raylib.h"
#include "world.h"

// Menu state enumeration
typedef enum {
    MENU_STATE_MAIN,
    MENU_STATE_WORLD_SELECT,
    MENU_STATE_CREATE_WORLD,
    MENU_STATE_CREDITS,
    MENU_STATE_SETTINGS,
    MENU_STATE_GAME
} MenuState;

// World info structure for display in world select menu
typedef struct {
    char name[256];
    char created[64];
    int chunk_count;
} WorldInfo;

// Game text localization (used in-game for HUD, pause menu, etc)
typedef struct {
    char move_controls[256];
    char metrics_help[256];
    char mouse_help[256];
    char look_help[256];
    char pause_help[256];
    char paused[64];
    char resume[64];
    char back_to_menu[64];
    char perf_metrics[256];
    char system_info[256];
    char player_info[256];
    char fps_label[64];
    char coord_label[64];
    char version[256];
    // Settings menu text
    char settings[64];
    char render_dist_label[256];
    char max_fps_label[256];
    char font_family_label[256];
    char font_variant_label[256];
    char uncapped[64];
    // Credits & Info text
    char press_esc_to_return[256];
    // Chat feedback messages
    char msg_quitting[128];
    char msg_teleported[256];
    char msg_teleport_usage[128];
    char msg_world_saved[256];
    char msg_world_save_failed[256];
    char msg_world_loaded[256];
    char msg_world_load_failed[256];
    char msg_invalid_world_name[256];
    char msg_block_selected[128];
    char msg_unknown_block[512];
    char msg_flight_enabled[256];
    char msg_flight_disabled[128];
    char msg_fly_usage[256];
    char msg_noclip_enabled[256];
    char msg_noclip_disabled[128];
    char msg_noclip_usage[256];
    char msg_block_set[256];
    char msg_out_of_bounds[256];
    char msg_setblock_usage[256];
    char msg_unknown_command[256];
} GameText;

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
    // Background image
    Texture2D background_texture;
    bool background_loaded;
    // Localization
    char current_language[32];
    char available_languages[16][32];  // Up to 16 language codes
    int available_languages_count;
    int current_language_index;
    // Menu text
    char text_select_world[256];
    char text_create_world[256];
    char text_credits_info[256];
    char text_quit[256];
    char text_back[256];
    char text_world_name_label[512];
    char text_create_btn[128];
    char text_cancel_btn[128];
    char text_error_empty_name[256];
    char text_error_exists[256];
    char text_no_worlds[256];
    char text_title_create_world[256];
    char text_title_select_world[256];
    char text_last[128];
    char text_chunks[128];
    // Game text (in-game HUD, pause menu, etc)
    GameText game_text;
    // Credits & info text
    char credits_text[4096];
    // Settings
    float render_distance;
    int max_fps;
    // Font selection
    char font_families[16][256];  // Up to 16 font families (folder names)
    int font_families_count;
    int current_font_family_index;
    // Font variants for current family
    char font_variants[32][256];  // Up to 32 variants per family
    int font_variants_count;
    int current_font_variant_index;
} MenuSystem;

// Function declarations
MenuSystem* menu_system_create(void);
void menu_system_free(MenuSystem* menu);
void menu_scan_worlds(MenuSystem* menu);
void menu_scan_fonts(MenuSystem* menu);
void menu_scan_font_variants(MenuSystem* menu, const char* font_family);
void menu_draw_main(MenuSystem* menu, Font font);
void menu_draw_world_select(MenuSystem* menu, Font font);
void menu_draw_create_world(MenuSystem* menu, Font font);
void menu_draw_credits(MenuSystem* menu, Font font);
void menu_draw_settings(MenuSystem* menu, Font font);
void menu_update_input(MenuSystem* menu);
void menu_load_language(MenuSystem* menu, const char* language);
bool menu_load_text_file(const char* language, const char* filename, char* out_buffer, int buffer_size);
void menu_load_settings(MenuSystem* menu);
void menu_save_settings(MenuSystem* menu);

#endif
