#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "raylib.h"
#include "world.h"
#include "player.h"
#include "vec_math.h"
#include "rendering.h"
#include "utils.h"
#include "menu.h"
#include "clouds.h"

// graphics and player constants
#define WINDOW_WIDTH 1200
#define WINDOW_HEIGHT 800
#define TARGET_FPS 144
#define RENDER_DISTANCE 50.0f
#define FOG_START 30.0f
#define CULLING_FOV 110.0f


// Helper function to load a specific font variant
static Font load_font_variant(const char* font_family, const char* font_variant)
{
    char font_path[512];
    snprintf(font_path, sizeof(font_path), "./assets/fonts/%s/ttf/%s", font_family, font_variant);

    // Build codepoint array with all ASCII and common extended characters
    int codepoints[1024] = {0};
    int codepoint_count = 0;

    // Full ASCII range (0-127)
    for (int i = 0; i < 128; i++) {
        codepoints[codepoint_count++] = i;
    }
    // Latin-1 Supplement (128-255)
    for (int i = 128; i < 256; i++) {
        codepoints[codepoint_count++] = i;
    }
    // Latin Extended-A (256-383)
    for (int i = 256; i < 384; i++) {
        codepoints[codepoint_count++] = i;
    }
    // Cyrillic block (0x0400-0x04FF)
    for (int i = 0x0400; i <= 0x04FF && codepoint_count < 1024; i++) {
        codepoints[codepoint_count++] = i;
    }

    Font font = LoadFontEx(font_path, 64, codepoints, codepoint_count);

    if (font.glyphCount > 0) {
        return font;
    }

    // Fallback to default if font not found
    return GetFontDefault();
}

// Helper function to load a font by family (uses first variant found)
static Font load_font_by_name(const char* font_name)
{
    char ttf_dir[512];
    snprintf(ttf_dir, sizeof(ttf_dir), "./assets/fonts/%s/ttf", font_name);

    // Open the ttf directory and find the first .ttf file
    DIR* dir = opendir(ttf_dir);
    Font font = {0};

    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir))) {
            // Look for .ttf files
            char* dot = strrchr(entry->d_name, '.');
            if (dot && strcmp(dot, ".ttf") == 0) {
                // Found a .ttf file, try to load it
                closedir(dir);
                return load_font_variant(font_name, entry->d_name);
            }
        }
        closedir(dir);
    }

    // Fallback to default if no font found
    return GetFontDefault();
}

int b3dv_main(void)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "b3dv 0.0.18-beta-beta");

    // Disable default ESC key behavior (we handle it manually for pause menu)
    SetExitKey(KEY_NULL);

    // Disable Raylib logging spam
    SetTraceLogLevel(LOG_NONE);

    // Initialize world save system (needed for menu world scanning)
    world_system_init();

    // Create menu system
    MenuSystem* menu = menu_system_create();

    // Set target FPS based on menu settings (will be updated dynamically)
    SetTargetFPS(menu->max_fps);

    // Load initial font from menu settings
    Font custom_font = load_font_variant(menu->font_families[menu->current_font_family_index],
                                         menu->font_variants[menu->current_font_variant_index]);
    char last_loaded_family[256];
    char last_loaded_variant[256];
    strncpy(last_loaded_family, menu->font_families[menu->current_font_family_index], sizeof(last_loaded_family) - 1);
    last_loaded_family[sizeof(last_loaded_family) - 1] = '\0';
    strncpy(last_loaded_variant, menu->font_variants[menu->current_font_variant_index], sizeof(last_loaded_variant) - 1);
    last_loaded_variant[sizeof(last_loaded_variant) - 1] = '\0';

    Camera3D camera = {
        .position = (Vector3){ 20, 15, 20 },
        .target = (Vector3){ 8, 4, 8 },
        .up = (Vector3){ 0, 1, 0 },
        .fovy = 90,
        .projection = CAMERA_PERSPECTIVE
    };

    // World and player will be created after menu selection
    World* world = NULL;
    Player* player = NULL;
    CloudSystem* clouds = NULL;

    // enable mouse capture (will be disabled in menu)
    bool mouse_captured = false;

    // HUD mode (0 = default, 1 = performance metrics 2 = player, 3 = system info)
    int hud_mode = 0;
    int prev_hud_mode = -1;  // Track previous mode to detect changes
    char cached_cpu[128] = {0};
    char cached_gpu[128] = {0};
    char cached_kernel[128] = {0};

    // Flight system
    bool flight_enabled = false;

    // Pause state
    bool paused = false;
    bool pause_settings_open = false;
    bool should_quit = false;

    // Chat system
    bool chat_active = false;
    char chat_input[256] = {0};
    int chat_input_len = 0;
    int chat_cursor_pos = 0;  // cursor position in input string
    int history_index = 0;    // 0 means no history entry selected, 1+ means Nth-from-last

    // Chat message display (feedback messages)
    #define CHAT_MESSAGE_BUFFER_SIZE 16
    #define CHAT_MESSAGE_MAX_LEN 512
    char chat_messages[CHAT_MESSAGE_BUFFER_SIZE][CHAT_MESSAGE_MAX_LEN] = {0};
    double chat_message_times[CHAT_MESSAGE_BUFFER_SIZE] = {0};
    int chat_message_count = 0;

    // Macro to add a chat message to the buffer
    #define add_chat_message(message) \
    do { \
        int _msg_idx = chat_message_count % CHAT_MESSAGE_BUFFER_SIZE; \
        strncpy(chat_messages[_msg_idx], (message), CHAT_MESSAGE_MAX_LEN - 1); \
        chat_messages[_msg_idx][CHAT_MESSAGE_MAX_LEN - 1] = '\0'; \
        chat_message_times[_msg_idx] = GetTime(); \
        chat_message_count++; \
    } while(0)

    // Camera angle tracking (in radians)
    float camera_yaw = 0.0f;    // horizontal rotation around Y axis
    float camera_pitch = 0.0f;  // vertical rotation (pitch)

    // Persistent camera basis vectors (to handle gimbal lock)
    Vector3 camera_right = {1, 0, 0};
    Vector3 camera_up = {0, 1, 0};
    // Persistent camera forward (actual look direction)
    Vector3 camera_forward = (Vector3){0, 0, 1};

    // Block highlighting for raycast
    int highlighted_block_x = 0;
    int highlighted_block_y = 0;
    int highlighted_block_z = 0;
    bool has_highlighted_block = false;

    // Initialize camera angles from initial forward direction
    camera_yaw = 0.0f;
    camera_pitch = 0.0f;

    // Raycast caching - only update every 3 frames
    int raycast_frame_counter = 0;

    // FOV values for visibility checks (computed dynamically to handle window resizing)
    float fov_half_vert_tan = 0.0f;
    float fov_half_horiz_tan = 0.0f;

    while (!WindowShouldClose() && !should_quit)
    {
        float dt = GetFrameTime();

        // Check if font family or variant changed in settings and reload if needed
        if (strcmp(menu->font_families[menu->current_font_family_index], last_loaded_family) != 0 ||
            strcmp(menu->font_variants[menu->current_font_variant_index], last_loaded_variant) != 0) {
            // Unload old font if it's not the default
            if (custom_font.glyphCount > 0 && custom_font.glyphCount != GetFontDefault().glyphCount) {
                UnloadFont(custom_font);
            }
            // Load new font variant
            custom_font = load_font_variant(menu->font_families[menu->current_font_family_index],
                                           menu->font_variants[menu->current_font_variant_index]);
            strcpy(last_loaded_family, menu->font_families[menu->current_font_family_index]);
            strcpy(last_loaded_variant, menu->font_variants[menu->current_font_variant_index]);
        }

        // Handle menu state
        if (menu->current_state == MENU_STATE_MAIN) {
            BeginDrawing();
            menu_draw_main(menu, custom_font);
            EndDrawing();
            menu_update_input(menu);
            continue;
        }
        else if (menu->current_state == MENU_STATE_WORLD_SELECT) {
            BeginDrawing();
            menu_draw_world_select(menu, custom_font);
            EndDrawing();
            menu_update_input(menu);
            continue;
        }
        else if (menu->current_state == MENU_STATE_CREATE_WORLD) {
            BeginDrawing();
            menu_draw_create_world(menu, custom_font);
            EndDrawing();
            menu_update_input(menu);
            continue;
        }
        else if (menu->current_state == MENU_STATE_CREDITS) {
            BeginDrawing();
            menu_draw_credits(menu, custom_font);
            EndDrawing();
            menu_update_input(menu);
            continue;
        }
        else if (menu->current_state == MENU_STATE_SETTINGS) {
            BeginDrawing();
            menu_draw_settings(menu, custom_font);
            EndDrawing();
            menu_update_input(menu);
            continue;
        }
        else if (menu->current_state == MENU_STATE_GAME && menu->should_start_game) {
            // Initialize game with selected world
            if (!world) {
                world = world_create();
                // Try to load the selected world, or generate new one
                if (!world_load(world, menu->selected_world_name)) {
                    world_generate_prism(world);
                    world_save(world, menu->selected_world_name);
                }

                // Load block textures
                world_load_textures(world);

                // Create player at saved position (or default if no save)
                player = player_create(world->last_player_position.x,
                                      world->last_player_position.y,
                                      world->last_player_position.z);

                // Create cloud system with cloud texture
                clouds = clouds_create("./assets/textures/clouds.png");

                // Enable mouse capture for gameplay
                mouse_captured = true;
                DisableCursor();

                // Initialize camera forward
                Vector3 initial_forward = vec3_normalize(vec3_sub(camera.target, camera.position));
                camera_forward = initial_forward;
                camera_yaw = atan2f(initial_forward.x, initial_forward.z);
                camera_pitch = asinf(initial_forward.y);
            }
            menu->should_start_game = false;
        }

        // Game logic below (only runs when in GAME state and world is initialized)
        if (!world || !player) {
            continue;
        }

        // Recalculate FOV values based on current window size (handles window resizing)
        float window_aspect = (float)GetScreenWidth() / (float)GetScreenHeight();
        float fovy_rad = CULLING_FOV * 3.14159265f / 180.0f;
        fov_half_vert_tan = tanf(fovy_rad / 2.0f);
        fov_half_horiz_tan = fov_half_vert_tan * window_aspect;

        // Update target FPS from settings
        SetTargetFPS(menu->max_fps);

        // Chat system - handle first to consume all input when active
        if (chat_active) {
            // Handle text input
            int key = GetCharPressed();
            while (key > 0) {
                if ((key >= 32 && key <= 125) && chat_input_len < 255) {
                    // Insert character at cursor position
                    for (int i = chat_input_len; i > chat_cursor_pos; i--) {
                        chat_input[i] = chat_input[i - 1];
                    }
                    chat_input[chat_cursor_pos] = (char)key;
                    chat_input_len++;
                    chat_cursor_pos++;
                    chat_input[chat_input_len] = '\0';
                }
                key = GetCharPressed();
            }

            // Handle backspace
            if (IsKeyPressed(KEY_BACKSPACE) && chat_cursor_pos > 0) {
                for (int i = chat_cursor_pos - 1; i < chat_input_len; i++) {
                    chat_input[i] = chat_input[i + 1];
                }
                chat_input_len--;
                chat_cursor_pos--;
                chat_input[chat_input_len] = '\0';
            }

            // Handle left arrow
            if (IsKeyPressed(KEY_LEFT) && chat_cursor_pos > 0) {
                chat_cursor_pos--;
            }

            // Handle right arrow
            if (IsKeyPressed(KEY_RIGHT) && chat_cursor_pos < chat_input_len) {
                chat_cursor_pos++;
            }

            // Handle up arrow (go back one command in history)
            if (IsKeyPressed(KEY_UP)) {
                history_index++;
                char history_line[256] = {0};
                if (get_chat_history_line(history_index, history_line, sizeof(history_line))) {
                    strncpy(chat_input, history_line, sizeof(chat_input) - 1);
                    chat_input[255] = '\0';
                    chat_input_len = strlen(chat_input);
                    chat_cursor_pos = chat_input_len;
                } else {
                    // No more history, revert to previous index
                    history_index--;
                }
            }

            // Handle down arrow (go forward one command in history, or clear)
            if (IsKeyPressed(KEY_DOWN)) {
                history_index--;
                if (history_index <= 0) {
                    // Clear input if we go past the beginning
                    history_index = 0;
                    chat_input[0] = '\0';
                    chat_input_len = 0;
                    chat_cursor_pos = 0;
                } else {
                    char history_line[256] = {0};
                    if (get_chat_history_line(history_index, history_line, sizeof(history_line))) {
                        strncpy(chat_input, history_line, sizeof(chat_input) - 1);
                        chat_input[255] = '\0';
                        chat_input_len = strlen(chat_input);
                        chat_cursor_pos = chat_input_len;
                    }
                }
            }

            // Handle enter (submit command)
            if (IsKeyPressed(KEY_ENTER)) {
                // Save to history file (only if not empty)
                if (chat_input_len > 0) {
                    FILE* log_file = fopen("./chathistory", "a");
                    if (log_file) {
                        fprintf(log_file, "%s\n", chat_input);
                        fclose(log_file);
                    }
                }

                chat_active = false;
                chat_cursor_pos = 0;
                history_index = 0;

                // Process commands
                if (chat_input[0] == '/') {
                    // Parse command
                    if (strncmp(chat_input, "/quit", 5) == 0) {
                        add_chat_message(menu->game_text.msg_quitting);
                        should_quit = true;
                    } else if (strncmp(chat_input, "/tp ", 4) == 0) {
                        // Parse teleport coordinates
                        float x, y, z;
                        if (sscanf(chat_input, "/tp %f %f %f", &x, &y, &z) == 3) {
                            player->position = (Vector3){ x, y, z };
                            player->velocity = (Vector3){ 0, 0, 0 };
                            char msg[256];
                            snprintf(msg, sizeof(msg), menu->game_text.msg_teleported, x, y, z);
                            add_chat_message(msg);
                        } else {
                            add_chat_message(menu->game_text.msg_teleport_usage);
                        }
                    } else if (strncmp(chat_input, "/save ", 6) == 0) {
                        // Save world: /save worldname
                        char world_name_buf[256];
                        strncpy(world_name_buf, chat_input + 6, sizeof(world_name_buf) - 1);
                        world_name_buf[255] = '\0';
                        trim_string(world_name_buf);
                        const char* world_name = world_name_buf;
                        if (world_save(world, world_name)) {
                            char msg[512];
                            snprintf(msg, sizeof(msg), menu->game_text.msg_world_saved, world_name);
                            add_chat_message(msg);
                        } else {
                            char msg[512];
                            snprintf(msg, sizeof(msg), menu->game_text.msg_world_save_failed, world_name);
                            add_chat_message(msg);
                        }
                    } else if (strncmp(chat_input, "/load ", 6) == 0) {
                        // Load world: /load worldname
                        char world_name_buf[256];
                        strncpy(world_name_buf, chat_input + 6, sizeof(world_name_buf) - 1);
                        world_name_buf[255] = '\0';
                        trim_string(world_name_buf);
                        const char* world_name = world_name_buf;
                        // Save current world first
                        if (strlen(world->world_name) > 0) {
                            world_save(world, world->world_name);
                        }
                        if (world_load(world, world_name)) {
                            char msg[512];
                            snprintf(msg, sizeof(msg), menu->game_text.msg_world_loaded, world_name);
                            add_chat_message(msg);
                        } else {
                            char msg[512];
                            snprintf(msg, sizeof(msg), menu->game_text.msg_world_load_failed, world_name);
                            add_chat_message(msg);
                            world_generate_prism(world);  // Fall back to default world
                        }
                    } else if (strncmp(chat_input, "/createworld ", 13) == 0) {
                        // Create new world: /createworld worldname
                        char world_name_buf[256];
                        strncpy(world_name_buf, chat_input + 13, sizeof(world_name_buf) - 1);
                        world_name_buf[255] = '\0';
                        trim_string(world_name_buf);
                        const char* world_name = world_name_buf;

                        // Validate world name (ASCII alphanumeric + underscore only)
                        bool valid_name = true;
                        if (world_name[0] == '\0') {
                            valid_name = false;
                        } else {
                            for (int i = 0; world_name[i]; i++) {
                                char c = world_name[i];
                                if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
                                      (c >= 'A' && c <= 'Z') || c == '_')) {
                                    valid_name = false;
                                    break;
                                }
                            }
                        }

                        if (!valid_name) {
                            add_chat_message("Invalid world name. Use only alphanumeric characters and underscore.");
                        } else {
                            // Free current world and create new one
                            world_free(world);
                            clouds_reset(clouds);  // Reset cloud positions for new world
                            world = world_create();
                            world_load_textures(world);  // Load textures for new world
                            // Set the world name before generating
                            strncpy(world->world_name, world_name, sizeof(world->world_name) - 1);
                            world->world_name[sizeof(world->world_name) - 1] = '\0';

                            world_generate_prism(world);
                            // Save the newly created world
                            if (world_save(world, world_name)) {
                                char msg[512];
                                snprintf(msg, sizeof(msg), "World '%s' created and saved successfully.", world_name);
                                add_chat_message(msg);
                                // Reset player position to origin
                                player->position = (Vector3){ 0, 105, 0 };
                                player->velocity = (Vector3){ 0, 0, 0 };
                            } else {
                                char msg[512];
                                snprintf(msg, sizeof(msg), "Failed to create world '%s'.", world_name);
                                add_chat_message(msg);
                            }
                        }
                    } else if (strncmp(chat_input, "/loadworld ", 11) == 0) {
                        // Load existing world: /loadworld worldname
                        char world_name_buf[256];
                        strncpy(world_name_buf, chat_input + 11, sizeof(world_name_buf) - 1);
                        world_name_buf[255] = '\0';
                        trim_string(world_name_buf);
                        const char* world_name = world_name_buf;

                        // Validate world name (ASCII alphanumeric + underscore only)
                        bool valid_name = true;
                        if (world_name[0] == '\0') {
                            valid_name = false;
                        } else {
                            for (int i = 0; world_name[i]; i++) {
                                char c = world_name[i];
                                if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
                                      (c >= 'A' && c <= 'Z') || c == '_')) {
                                    valid_name = false;
                                    break;
                                }
                            }
                        }

                        if (!valid_name) {
                            add_chat_message(menu->game_text.msg_invalid_world_name);
                        } else {
                            // Free current world and player, then load the saved one
                            world_free(world);
                            clouds_reset(clouds);  // Reset cloud positions for loaded world
                            world = world_create();
                            world_load_textures(world);  // Reload textures for new world
                            // Recreate player at spawn position calculated from terrain height
                            float spawn_x = 8.0f;
                            float spawn_z = 8.0f;
                            float h1 = sinf(spawn_x * 0.1f) * cosf(spawn_z * 0.1f) * 8.0f;
                            float h2 = sinf(spawn_x * 0.05f) * cosf(spawn_z * 0.05f) * 6.0f;
                            float terrain_h = h1 + h2 + 10.0f + 5.0f;
                            float spawn_y = terrain_h + 1.5f;
                            player = player_create(spawn_x, spawn_y, spawn_z);

                            if (world_load(world, world_name)) {
                                // Reset player position
                                player->position = (Vector3){ 0, 105, 0 };
                                player->velocity = (Vector3){ 0, 0, 0 };
                                char msg[512];
                                snprintf(msg, sizeof(msg), menu->game_text.msg_world_loaded, world_name);
                                add_chat_message(msg);
                            } else {
                                char msg[512];
                                snprintf(msg, sizeof(msg), menu->game_text.msg_world_load_failed, world_name);
                                add_chat_message(msg);
                                world_generate_prism(world);  // Fall back to default world
                            }
                        }
                    } else if (strncmp(chat_input, "/select ", 8) == 0) {
                        // Select block type: /select <stone|grass|dirt|sand|wood>
                        char block_name_buf[32];
                        strncpy(block_name_buf, chat_input + 8, sizeof(block_name_buf) - 1);
                        block_name_buf[31] = '\0';
                        trim_string(block_name_buf);
                        const char* block_name = block_name_buf;

                        if (strcmp(block_name, "stone") == 0) {
                            player->selected_block = BLOCK_STONE;
                            char msg[256];
                            snprintf(msg, sizeof(msg), menu->game_text.msg_block_selected, "stone");
                            add_chat_message(msg);
                        } else if (strcmp(block_name, "dirt") == 0) {
                            player->selected_block = BLOCK_DIRT;
                            char msg[256];
                            snprintf(msg, sizeof(msg), menu->game_text.msg_block_selected, "dirt");
                            add_chat_message(msg);
                        } else if (strcmp(block_name, "grass") == 0) {
                            player->selected_block = BLOCK_GRASS;
                            char msg[256];
                            snprintf(msg, sizeof(msg), menu->game_text.msg_block_selected, "grass");
                            add_chat_message(msg);
                        } else if (strcmp(block_name, "sand") == 0) {
                            player->selected_block = BLOCK_SAND;
                            char msg[256];
                            snprintf(msg, sizeof(msg), menu->game_text.msg_block_selected, "sand");
                            add_chat_message(msg);
                        } else if (strcmp(block_name, "wood") == 0) {
                            player->selected_block = BLOCK_WOOD;
                            char msg[256];
                            snprintf(msg, sizeof(msg), menu->game_text.msg_block_selected, "wood");
                            add_chat_message(msg);
                        } else if (strcmp(block_name, "glowstone") == 0) {
                            player->selected_block = BLOCK_GLOWSTONE;
                            char msg[256];
                            snprintf(msg, sizeof(msg), menu->game_text.msg_block_selected, "glowstone");
                            add_chat_message(msg);
                        } else {
                            char msg[512];
                            snprintf(msg, sizeof(msg), menu->game_text.msg_unknown_block, block_name);
                            add_chat_message(msg);
                        }
                    } else if (strncmp(chat_input, "/fly ", 5) == 0) {
                        // Flight control: /fly enable or /fly disable
                        char fly_cmd_buf[32];
                        strncpy(fly_cmd_buf, chat_input + 5, sizeof(fly_cmd_buf) - 1);
                        fly_cmd_buf[31] = '\0';
                        trim_string(fly_cmd_buf);
                        const char* fly_cmd = fly_cmd_buf;

                        if (strcmp(fly_cmd, "enable") == 0) {
                            flight_enabled = true;
                            add_chat_message(menu->game_text.msg_flight_enabled);
                        } else if (strcmp(fly_cmd, "disable") == 0) {
                            flight_enabled = false;
                            player->is_flying = false;  // Stop flying if currently flying
                            add_chat_message(menu->game_text.msg_flight_disabled);
                        } else {
                            add_chat_message(menu->game_text.msg_fly_usage);
                        }
                    } else if (strncmp(chat_input, "/noclip ", 8) == 0) {
                        // No-clip mode control: /noclip enable or /noclip disable
                        char noclip_cmd_buf[32];
                        strncpy(noclip_cmd_buf, chat_input + 8, sizeof(noclip_cmd_buf) - 1);
                        noclip_cmd_buf[31] = '\0';
                        trim_string(noclip_cmd_buf);
                        const char* noclip_cmd = noclip_cmd_buf;

                        if (strcmp(noclip_cmd, "enable") == 0) {
                            player->no_clip = true;
                            add_chat_message(menu->game_text.msg_noclip_enabled);
                        } else if (strcmp(noclip_cmd, "disable") == 0) {
                            player->no_clip = false;
                            add_chat_message(menu->game_text.msg_noclip_disabled);
                        } else {
                            add_chat_message(menu->game_text.msg_noclip_usage);
                        }
                    } else if (strncmp(chat_input, "/setblock ", 10) == 0) {
                        // Set block: /setblock x y z [block_type]
                        // If block_type is omitted, uses player's selected block
                        // All coordinates treated as world-space (centered at 0,0,0)
                        float fx, fy, fz;
                        char block_name[32] = {0};

                        int parsed = sscanf(chat_input, "/setblock %f %f %f %31s", &fx, &fy, &fz, block_name);

                        if (parsed >= 3) {
                            // Trim block name if provided
                            if (parsed == 4) {
                                trim_string(block_name);
                            }

                            // Convert world-space to block indices (infinite world, no offset needed)
                            int ix = (int)floorf(fx);
                            int iy = (int)floorf(fy);
                            int iz = (int)floorf(fz);

                            BlockType block_type = BLOCK_AIR;
                            const char* type_str = "air";

                            // If no block name provided, use selected block
                            if (parsed == 3) {
                                block_type = player->selected_block;
                                // Get string representation of selected block
                                if (block_type == BLOCK_STONE) type_str = "stone";
                                else if (block_type == BLOCK_DIRT) type_str = "dirt";
                                else if (block_type == BLOCK_GRASS) type_str = "grass";
                                else if (block_type == BLOCK_SAND) type_str = "sand";
                                else if (block_type == BLOCK_WOOD) type_str = "wood";
                                else if (block_type == BLOCK_GLOWSTONE) type_str = "glowstone";
                                else if (block_type == BLOCK_AIR) type_str = "air";
                            } else {
                                // Parse explicit block name
                                if (strcmp(block_name, "stone") == 0) {
                                    block_type = BLOCK_STONE;
                                    type_str = "stone";
                                } else if (strcmp(block_name, "dirt") == 0) {
                                    block_type = BLOCK_DIRT;
                                    type_str = "dirt";
                                } else if (strcmp(block_name, "sand") == 0) {
                                    block_type = BLOCK_SAND;
                                    type_str = "sand";
                                } else if (strcmp(block_name, "wood") == 0) {
                                    block_type = BLOCK_WOOD;
                                    type_str = "wood";
                                } else if (strcmp(block_name, "grass") == 0) {
                                    block_type = BLOCK_GRASS;
                                    type_str = "grass";
                                } else if (strcmp(block_name, "glowstone") == 0) {
                                    block_type = BLOCK_GLOWSTONE;
                                    type_str = "glowstone";
                                } else if (strcmp(block_name, "air") == 0) {
                                    block_type = BLOCK_AIR;
                                    type_str = "air";
                                } else {
                                    char msg[512];
                                    snprintf(msg, sizeof(msg), menu->game_text.msg_unknown_block, block_name);
                                    add_chat_message(msg);
                                    ix = -1;  // Skip application
                                }
                            }

                            // Apply if y is within valid bounds (x and z can be infinite)
                            if (iy >= 0 && iy < 256) {
                                world_set_block(world, ix, iy, iz, block_type);
                                char msg[512];
                                snprintf(msg, sizeof(msg), menu->game_text.msg_block_set, fx, fy, fz, type_str);
                                add_chat_message(msg);
                            } else if (ix != -1) {
                                add_chat_message(menu->game_text.msg_out_of_bounds);
                            }
                        } else {
                            add_chat_message(menu->game_text.msg_setblock_usage);
                        }
                    } else {
                        // New: /give <item> [<count>]
                        if (strncmp(chat_input, "/give ", 6) == 0) {
                            char item_buf[64] = {0};
                            int count = 1;
                            // Try to parse with optional count
                            // Supports: /give stone 10  OR /give stone
                            int parsed = sscanf(chat_input + 6, "%63s %d", item_buf, &count);
                            if (parsed >= 1) {
                                trim_string(item_buf);
                                if (parsed == 1) count = 1;

                                BlockType bt = BLOCK_AIR;
                                if (strcmp(item_buf, "stone") == 0) bt = BLOCK_STONE;
                                else if (strcmp(item_buf, "dirt") == 0) bt = BLOCK_DIRT;
                                else if (strcmp(item_buf, "grass") == 0) bt = BLOCK_GRASS;
                                else if (strcmp(item_buf, "sand") == 0) bt = BLOCK_SAND;
                                else if (strcmp(item_buf, "wood") == 0) bt = BLOCK_WOOD;
                                else if (strcmp(item_buf, "glowstone") == 0) bt = BLOCK_GLOWSTONE;

                                if (bt == BLOCK_AIR) {
                                    char msg[256];
                                    snprintf(msg, sizeof(msg), menu->game_text.msg_unknown_block, item_buf);
                                    add_chat_message(msg);
                                } else {
                                    if (inventory_give(player, bt, count)) {
                                        char msg[256];
                                        if (count == 1)
                                            snprintf(msg, sizeof(msg), "Gave 1 %s.", item_buf);
                                        else
                                            snprintf(msg, sizeof(msg), "Gave %d %s.", count, item_buf);
                                        add_chat_message(msg);
                                    } else {
                                        add_chat_message("Not enough inventory space to give items.");
                                    }
                                }
                            } else {
                                add_chat_message("Usage: /give <item> [count]");
                            }
                        } else {
                        // Unknown command
                        char msg[512];
                        snprintf(msg, sizeof(msg), menu->game_text.msg_unknown_command, chat_input);
                        add_chat_message(msg);
                        }
                    }
                }

                // Recapture mouse if it was captured before chat
                mouse_captured = true;
                DisableCursor();
            }

            // Handle escape to cancel chat
            if (IsKeyPressed(KEY_ESCAPE)) {
                chat_active = false;
                chat_cursor_pos = 0;
                history_index = 0;
                mouse_captured = true;
                DisableCursor();
            }
        } else {
            // Only process game keys when chat is not active
            // HUD mode toggle with F2/F3/F4/F5
        if (IsKeyPressed(KEY_F2)) {
            hud_mode = 0;  // default HUD
        }
        if (IsKeyPressed(KEY_F3)) {
            hud_mode = 1;  // performance metrics
        }
        if (IsKeyPressed(KEY_F4)) {
            hud_mode = 2;  // player
        }
        if (IsKeyPressed(KEY_F5)) {
            hud_mode = 3;  // system info
            // Fetch system info only when entering this mode
            if (prev_hud_mode != 3) {
                get_cpu_model(cached_cpu, sizeof(cached_cpu));
                get_gpu_model(cached_gpu, sizeof(cached_gpu));
                get_kernel_info(cached_kernel, sizeof(cached_kernel));
            }
        }
        prev_hud_mode = hud_mode;

        // If inventory is open and ESC pressed, close inventory (don't toggle pause)
        if (IsKeyPressed(KEY_ESCAPE) && inventory_is_big_open(player)) {
            inventory_toggle_big(player);
            // When closing inventory, recapture mouse if gameplay had it captured
            if (inventory_is_big_open(player)) {
                // still open -> ensure cursor enabled
                mouse_captured = false;
                EnableCursor();
            } else {
                // closed -> restore capture state
                if (!paused && mouse_captured) DisableCursor();
            }
        } else {
            // toggle pause with P key or ESC (only if inventory wasn't handled above)
            if (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_ESCAPE)) {
                paused = !paused;
                // Automatically uncapture mouse when pausing, recapture when resuming
                if (paused) {
                    mouse_captured = false;
                    EnableCursor();
                } else if (mouse_captured) {
                    DisableCursor();
                }
            }
        }

        // Auto-manage mouse capture: captured only when gameplay (not paused/chat/inventory)
        {
            bool should_capture = (!paused && !chat_active && !inventory_is_big_open(player));
            if (should_capture && !mouse_captured) {
                mouse_captured = true;
                DisableCursor();
            } else if (!should_capture && mouse_captured) {
                mouse_captured = false;
                EnableCursor();
            }
        }

        // toggle fullscreen with F11
        if (IsKeyPressed(KEY_F11)) {
            ToggleFullscreen();
        }

        // teleport with R key
        if (IsKeyPressed(KEY_R)) {
            player->position = (Vector3){ 8.0f, 105.0f, 8.0f };
            player->velocity = (Vector3){ 0, 0, 0 };
        }

        // Chat system - open with T key
        if (IsKeyPressed(KEY_T) && !paused) {
            chat_active = true;
            chat_input_len = 0;
            chat_input[0] = '\0';
            history_index = 0;
            mouse_captured = false;
            EnableCursor();
        }
        }  // End of else block for non-chat input

        // handle mouse look (must happen before getting forward/right)
        if (mouse_captured) {
            Vector2 mouse_delta = GetMouseDelta();
            float mouse_speed = 0.005f;

            // Update pitch (vertical rotation) with clamping
            float new_pitch = camera_pitch - mouse_delta.y * mouse_speed;

            // Clamp pitch to very close to ±90 degrees (e.g., 89.9°)
            float pitch_limit = 1.56905f;  // ~89.9 degrees in radians
            if (new_pitch > pitch_limit) new_pitch = pitch_limit;
            if (new_pitch < -pitch_limit) new_pitch = -pitch_limit;
            camera_pitch = new_pitch;

            // Update yaw (horizontal rotation)
            camera_yaw -= mouse_delta.x * mouse_speed;

            // Reconstruct forward vector from angles
            float sin_pitch = sinf(camera_pitch);
            float cos_pitch = cosf(camera_pitch);
            float sin_yaw = sinf(camera_yaw);
            float cos_yaw = cosf(camera_yaw);

            Vector3 new_forward = (Vector3){
                sin_yaw * cos_pitch,
                sin_pitch,
                cos_yaw * cos_pitch
            };
            new_forward = vec3_normalize(new_forward);

            // Store actual look direction
            camera_forward = new_forward;

            // Only update right/up vectors when not at gimbal lock (cos_pitch > 0.01)
            // When in gimbal lock, compute right directly from yaw to keep it consistent
            if (fabsf(cos_pitch) > 0.01f) {
                // Safe to compute from cross product
                camera_right = vec3_cross(new_forward, (Vector3){0, 1, 0});
                camera_right = vec3_normalize(camera_right);

                camera_up = vec3_cross(camera_right, new_forward);
                camera_up = vec3_normalize(camera_up);
            } else {
                // Gimbal lock: compute right directly from current yaw
                // right is perpendicular to world up, pointing in the direction of yaw
                camera_right = (Vector3){cosf(camera_yaw), 0, -sinf(camera_yaw)};
                camera_up = (Vector3){0, 1, 0};  // Keep world up
            }
        }

        // Movement should follow the camera's horizontal basis. Project the computed
        // `camera_right` to the XZ plane and derive a horizontal forward from it.
        // Movement should always be parallel to the XZ plane, regardless of pitch
        // Invert right and forward to match standard FPS controls
        Vector3 right = (Vector3){ -cosf(camera_yaw), 0.0f, sinf(camera_yaw) };
        right = vec3_normalize(right);

        Vector3 forward = (Vector3){ -sinf(camera_yaw), 0.0f, -cosf(camera_yaw) };
        forward = vec3_normalize(forward);

        // Update physics and world always (unless game is paused), even if chat is active
        if (!paused) {
            player_update(player, world, dt, flight_enabled);
            world_update_chunks(world, player->position, camera_forward);  // Load/unload chunks based on player position and camera direction
            clouds_update(clouds, player->position);  // Update cloud positions
        }

        // Handle player input only if chat is not active and inventory is not open
        if (!paused && !chat_active && !inventory_is_big_open(player)) {
            player_move_input(player, forward, right, flight_enabled);

            // Handle block breaking (left click)
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                int hit_x, hit_y, hit_z;
                int adj_x, adj_y, adj_z;
                if (raycast_block(world, camera, 10.0f, &hit_x, &hit_y, &hit_z, &adj_x, &adj_y, &adj_z)) {
                    BlockType broken_block = world_get_block(world, hit_x, hit_y, hit_z);
                    if (broken_block != BLOCK_BEDROCK && broken_block != BLOCK_AIR) {
                        // Add broken block to inventory
                        inventory_add_block(player, broken_block);
                        world_set_block(world, hit_x, hit_y, hit_z, BLOCK_AIR);
                    }
                }
            }

            // Handle block placing (right click)
            if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                int hit_x, hit_y, hit_z;
                int adj_x, adj_y, adj_z;
                if (raycast_block(world, camera, 10.0f, &hit_x, &hit_y, &hit_z, &adj_x, &adj_y, &adj_z)) {
                    // Place block in the adjacent empty space
                    BlockType adjacent_block = world_get_block(world, adj_x, adj_y, adj_z);
                    if (adjacent_block == BLOCK_AIR) {
                        // Check if placing the block would intersect with player hitbox
                        Vector3 block_center = (Vector3){
                            adj_x + 0.5f,
                            adj_y + 0.5f,
                            adj_z + 0.5f
                        };

                        // Simple AABB collision check: block (1x1x1) vs player (radius 0.35 cylinder with height 1.9)
                        // Check if block center is within player's horizontal radius
                        float dx = player->position.x - block_center.x;
                        float dz = player->position.z - block_center.z;
                        float horizontal_dist = sqrtf(dx*dx + dz*dz);

                        // Check vertical overlap (player head at position.y, feet at position.y - PLAYER_HEIGHT)
                        float player_bottom = player->position.y - PLAYER_HEIGHT;
                        float player_top = player->position.y;
                        float block_bottom = adj_y;
                        float block_top = adj_y + 1.0f;

                        bool vertical_overlap = (block_bottom < player_top && block_top > player_bottom);
                        bool horizontal_overlap = (horizontal_dist < PLAYER_RADIUS + 0.5f);  // 0.5 is half block width

                        // Only place if it doesn't intersect with player
                        if (!(vertical_overlap && horizontal_overlap)) {
                            // Get block type from selected inventory slot only
                            BlockType block_to_place = inventory_get_selected_block(player);
                            // Only place if selected slot has blocks (not BLOCK_AIR)
                            if (block_to_place != BLOCK_AIR && inventory_remove_block(player, block_to_place)) {
                                world_set_block(world, adj_x, adj_y, adj_z, block_to_place);
                            }
                        }
                    }
                }
            }

            // Update highlighted block (raycast every 3 frames for performance)
            raycast_frame_counter++;
            if (raycast_frame_counter >= 3) {
                raycast_frame_counter = 0;
                int hit_x, hit_y, hit_z;
                int adj_x, adj_y, adj_z;
                if (raycast_block(world, camera, 10.0f, &hit_x, &hit_y, &hit_z, &adj_x, &adj_y, &adj_z)) {
                    highlighted_block_x = hit_x;
                    highlighted_block_y = hit_y;
                    highlighted_block_z = hit_z;
                    has_highlighted_block = true;
                } else {
                    has_highlighted_block = false;
                }
            }

            // Handle inventory slot selection (keys 1-9)
            if (IsKeyPressed(KEY_ONE)) player->selected_slot = 0;
            if (IsKeyPressed(KEY_TWO)) player->selected_slot = 1;
            if (IsKeyPressed(KEY_THREE)) player->selected_slot = 2;
            if (IsKeyPressed(KEY_FOUR)) player->selected_slot = 3;
            if (IsKeyPressed(KEY_FIVE)) player->selected_slot = 4;
            if (IsKeyPressed(KEY_SIX)) player->selected_slot = 5;
            if (IsKeyPressed(KEY_SEVEN)) player->selected_slot = 6;
            if (IsKeyPressed(KEY_EIGHT)) player->selected_slot = 7;
            if (IsKeyPressed(KEY_NINE)) player->selected_slot = 8;

            // Handle big inventory toggle (I key)
            if (IsKeyPressed(KEY_I)) {
                inventory_toggle_big(player);
                if (inventory_is_big_open(player)) {
                    // Opened inventory: show cursor and stop mouse-look
                    mouse_captured = false;
                    EnableCursor();
                } else {
                    // Closed inventory: restore capture if gameplay expects it
                    if (!paused && mouse_captured) DisableCursor();
                }
            }
        }

        // update camera to follow player (position it at eye level, slightly above center)
        float eye_height = 0.7f;
        camera.position = (Vector3){
            player->position.x,
            player->position.y + eye_height,
            player->position.z
        };
        // Use the actual look direction for camera target (includes pitch)
        camera.target = (Vector3){
            player->position.x + camera_forward.x,
            player->position.y + eye_height + camera_forward.y,
            player->position.z + camera_forward.z
        };

        // (mouse look handled earlier in the loop)

        BeginDrawing();
        ClearBackground(SKYBLUE);

        // Camera-relative rendering: shift camera to block-aligned grid for float precision
        // Align to the nearest block boundary to avoid jittering with negative coordinates
        Vector3 original_camera_pos = camera.position;
        Vector3 camera_offset = (Vector3){
            floorf(camera.position.x),
            0,
            floorf(camera.position.z)
        };

        // Shift camera and target for rendering (only for graphics)
        camera.position.x -= camera_offset.x;
        camera.position.y -= camera_offset.y;
        camera.position.z -= camera_offset.z;
        camera.target.x -= camera_offset.x;
        camera.target.y -= camera_offset.y;
        camera.target.z -= camera_offset.z;

        // Use shifted camera position for visibility checks
        Vector3 shifted_cam_pos = camera.position;

        int blocks_rendered = 0;  // Declared here so it's accessible after the if block

        BeginMode3D(camera);
            // Use the actual camera orientation for rendering/frustum culling
            Vector3 cam_forward = camera_forward;
            Vector3 cam_right = camera_right;

            // CRITICAL: Take a snapshot of chunk pointers while holding cache_mutex
            // This prevents chunks from being unloaded during rendering
            pthread_mutex_lock(&world->cache_mutex);
            int chunk_count_snapshot = world->chunk_cache.chunk_count;
            // Allocate temporary array for chunk pointers
            Chunk** chunks_snapshot = (Chunk**)malloc(chunk_count_snapshot * sizeof(Chunk*));
            for (int i = 0; i < chunk_count_snapshot; i++) {
                chunks_snapshot[i] = &world->chunk_cache.chunks[i];
            }
            pthread_mutex_unlock(&world->cache_mutex);

            // draw all chunks and their blocks with frustum culling and face culling
            for (int c = 0; c < chunk_count_snapshot; c++) {
                Chunk* chunk = chunks_snapshot[c];

                // Skip unloaded chunks
                if (!chunk->loaded) continue;

                // Skip chunks that haven't been generated yet
                if (!chunk->generated) continue;

                // OPTIMIZATION: Iterate only through cached visible blocks instead of all blocks
                // This is the main performance win - avoids triple-nested loop of 2048 blocks per chunk
                // Lock chunk while accessing visible_blocks to avoid race with worker thread
                pthread_mutex_lock(&chunk->mesh_swap_mutex);

                // Get active mesh buffer (updated by worker thread via double-buffer swap)
                // Use atomic load with acquire semantics to ensure we see the latest update
                int active = __atomic_load_n(&chunk->active_mesh, __ATOMIC_ACQUIRE);

                // Safety checks after locking
                // Render even if worker is updating - we see consistent data from active buffer
                // to prevent use-after-free if worker thread updates the other buffer
                int visible_count = chunk->visible_count[active];
                CachedVisibleBlock* visible_blocks_copy = (CachedVisibleBlock*)malloc(visible_count * sizeof(CachedVisibleBlock));
                if (!visible_blocks_copy) {
                    pthread_mutex_unlock(&chunk->mesh_swap_mutex);
                    continue;
                }

                // CRITICAL: Hold lock during memcpy to prevent another thread from updating
                // chunk->visible_blocks[active] or visible_count[active] while we're copying
                // This prevents reading beyond buffer bounds or using freed memory
                if (chunk->visible_blocks[active] != NULL && visible_count > 0) {
                    memcpy(visible_blocks_copy, chunk->visible_blocks[active], visible_count * sizeof(CachedVisibleBlock));
                } else {
                    // Buffer not ready yet, skip rendering this chunk
                    free(visible_blocks_copy);
                    pthread_mutex_unlock(&chunk->mesh_swap_mutex);
                    continue;
                }

                pthread_mutex_unlock(&chunk->mesh_swap_mutex);

                // Render blocks with the copied data (no lock held)
                // Pre-compute render distance thresholds for LOD
                float aggressive_lod_dist = menu->render_distance * 0.75f;
                float aggressive_lod_dist_sq = aggressive_lod_dist * aggressive_lod_dist;
                float render_dist_sq = menu->render_distance * menu->render_distance;

                for (int i = 0; i < visible_count; i++) {
                    int x = visible_blocks_copy[i].x;
                    int y = visible_blocks_copy[i].y;
                    int z = visible_blocks_copy[i].z;
                    BlockType block = world_chunk_get_block(chunk, x, y, z);

                    // Calculate world coordinates
                    int world_x = chunk->chunk_x * CHUNK_WIDTH + x;
                    int world_y = chunk->chunk_y * CHUNK_HEIGHT + y;
                    int world_z = chunk->chunk_z * CHUNK_DEPTH + z;

                                Vector3 world_pos = (Vector3){
                                    world_x + 0.5f - camera_offset.x,
                                    world_y + 0.5f - camera_offset.y,
                                    world_z + 0.5f - camera_offset.z
                                };
                                // distance-based LOD: use squared distance to avoid sqrt
                                Vector3 to_block = vec3_sub(world_pos, shifted_cam_pos);
                                float dist_sq = to_block.x*to_block.x + to_block.y*to_block.y + to_block.z*to_block.z;

                                // hard render distance limit
                                if (dist_sq > render_dist_sq) {
                                    continue;
                                }

                                // AGGRESSIVE LOD: Skip blocks beyond 75% of render distance
                                // This culls ~60% of blocks while maintaining visual quality
                                // Blocks at render_distance are mostly fog-shrouded anyway
                                if (dist_sq > aggressive_lod_dist_sq) {
                                    continue;
                                }

                                // OPTIMIZATION: Skip is_block_visible_fast() - chunk frustum culling is sufficient
                                // OPTIMIZATION: Skip is_block_occluded() - visible_blocks cache already filters these

                                float dist = sqrtf(dist_sq);  // Only calc sqrt if we're actually rendering
                                Color color = world_get_block_color(block);

                                // apply fog effect: fade color towards sky blue based on distance
                                float fog_factor = 0.0f;
                                float fog_start = menu->render_distance * 0.6f;  // Fog starts at 60% of render distance
                                if (dist > fog_start) {
                                    fog_factor = (dist - fog_start) / (menu->render_distance - fog_start);
                                    fog_factor = fog_factor > 1.0f ? 1.0f : fog_factor;  // Clamp to 0-1

                                    // blend color towards sky blue
                                    color.r = (unsigned char)(color.r * (1.0f - fog_factor) + SKYBLUE.r * fog_factor);
                                    color.g = (unsigned char)(color.g * (1.0f - fog_factor) + SKYBLUE.g * fog_factor);
                                    color.b = (unsigned char)(color.b * (1.0f - fog_factor) + SKYBLUE.b * fog_factor);
                                }

                                // apply fog to wireframe too
                                Color wire_color = DARKGRAY;
                                if (fog_factor > 0.0f) {
                                    wire_color.r = (unsigned char)(wire_color.r * (1.0f - fog_factor) + SKYBLUE.r * fog_factor);
                                    wire_color.g = (unsigned char)(wire_color.g * (1.0f - fog_factor) + SKYBLUE.g * fog_factor);
                                    wire_color.b = (unsigned char)(wire_color.b * (1.0f - fog_factor) + SKYBLUE.b * fog_factor);
                                    wire_color.a = (unsigned char)(255 * (1.0f - fog_factor));  // Fade out alpha too
                                }
                        // draw only visible faces
                        draw_cube_faces(world_pos, 1.0f, color, camera.position, wire_color, world, world_x, world_y, world_z, block, visible_blocks_copy[i].exposed_faces, visible_blocks_copy[i].light_level);
                        blocks_rendered++;
                    }

                // Free the temporary copy
                free(visible_blocks_copy);
            }

            // Draw highlighting box around the block being looked at
            if (has_highlighted_block) {
                Vector3 block_pos = (Vector3){
                    highlighted_block_x + 0.5f - camera_offset.x,
                    highlighted_block_y + 0.5f - camera_offset.y,
                    highlighted_block_z + 0.5f - camera_offset.z
                };
                DrawCubeWires(block_pos, 1.0f, 1.0f, 1.0f, YELLOW);
            }
            DrawGrid(30, 1.0f);

            // Draw clouds
            clouds_draw(clouds, camera.position, camera_offset);
        EndMode3D();

        // Free the chunk snapshot
        free(chunks_snapshot);

        // Restore original camera position
        camera.position = original_camera_pos;
        camera.target.x += camera_offset.x;
        camera.target.y += camera_offset.y;
        camera.target.z += camera_offset.z;

        // Draw crosshair at center of screen
        int center_x = GetScreenWidth() / 2;
        int center_y = GetScreenHeight() / 2;
        int crosshair_size = 10;
        int crosshair_thickness = 2;

        // Draw crosshair - two perpendicular lines
        DrawLineEx((Vector2){center_x - crosshair_size, center_y},
                   (Vector2){center_x + crosshair_size, center_y},
                   crosshair_thickness, BLACK);
        DrawLineEx((Vector2){center_x, center_y - crosshair_size},
                   (Vector2){center_x, center_y + crosshair_size},
                   crosshair_thickness, BLACK);

        // Draw inventory UI at bottom of screen
        {
            int slot_size = 50;
            int slot_spacing = 10;
            int inv_width = INVENTORY_SIZE * slot_size + (INVENTORY_SIZE - 1) * slot_spacing;
            int inv_x = (GetScreenWidth() - inv_width) / 2;
            int inv_y = GetScreenHeight() - slot_size - 30;

            // Draw inventory background
            DrawRectangle(inv_x - 5, inv_y - 5, inv_width + 10, slot_size + 10, (Color){0, 0, 0, 150});

            // Draw each inventory slot
            for (int i = 0; i < INVENTORY_SIZE; i++) {
                int x = inv_x + i * (slot_size + slot_spacing);
                int y = inv_y;

                // Slot background
                Color slot_bg = (Color){80, 80, 80, 200};
                if (i == player->selected_slot) {
                    slot_bg = (Color){100, 100, 100, 255};  // Highlight selected slot
                }
                DrawRectangle(x, y, slot_size, slot_size, slot_bg);

                // Draw slot border
                Color border_color = (i == player->selected_slot) ? (Color){255, 255, 255, 255} : (Color){100, 100, 100, 255};
                DrawRectangleLines(x, y, slot_size, slot_size, border_color);

                // Draw block indicator (colored square representing block type)
                InventorySlot* slot = &player->inventory[i];
                if (slot->count > 0) {
                    // Map block type to color
                    Color block_color;
                    switch (slot->type) {
                        case BLOCK_STONE: block_color = (Color){128, 128, 128, 255}; break;
                        case BLOCK_DIRT: block_color = (Color){139, 69, 19, 255}; break;
                        case BLOCK_GRASS: block_color = (Color){34, 139, 34, 255}; break;
                        case BLOCK_SAND: block_color = (Color){194, 178, 128, 255}; break;
                        case BLOCK_WOOD: block_color = (Color){139, 90, 43, 255}; break;
                        case BLOCK_BEDROCK: block_color = (Color){64, 64, 64, 255}; break;
                        case BLOCK_GLOWSTONE: block_color = (Color){255, 255, 0, 255}; break;
                        default: block_color = (Color){200, 200, 200, 255}; break;
                    }

                    // Draw colored block indicator (smaller than slot)
                    int block_indent = 10;
                    DrawRectangle(x + block_indent, y + block_indent, slot_size - 2*block_indent, slot_size - 2*block_indent, block_color);

                    // Draw item count
                    char count_str[8];
                    snprintf(count_str, sizeof(count_str), "%d", slot->count);
                    int text_x = x + slot_size - MeasureTextEx(custom_font, count_str, 20, 1).x - 5;
                    int text_y = y + slot_size - 20;
                    DrawTextEx(custom_font, count_str, (Vector2){text_x, text_y}, 20, 1, WHITE);
                }

                // Draw slot number
                char slot_num[4];
                snprintf(slot_num, sizeof(slot_num), "%d", i + 1);
                DrawTextEx(custom_font, slot_num, (Vector2){x + 3, y + 3}, 16, 1, (Color){150, 150, 150, 255});
            }
        }

        // Draw big inventory UI when open
        if (inventory_is_big_open(player)) {
            int slot_size = 40;
            int slot_spacing = 4;
            int inv_width = BIG_INVENTORY_COLS * slot_size + (BIG_INVENTORY_COLS - 1) * slot_spacing;
            int inv_height = (BIG_INVENTORY_ROWS + 1) * slot_size + (BIG_INVENTORY_ROWS) * slot_spacing;
            int inv_x = (GetScreenWidth() - inv_width) / 2;
            int inv_y = (GetScreenHeight() - inv_height) / 2;

            // Draw background panel
            DrawRectangle(inv_x - 10, inv_y - 10, inv_width + 20, inv_height + 20, (Color){50, 50, 50, 220});
            DrawRectangleLines(inv_x - 10, inv_y - 10, inv_width + 20, inv_height + 20, (Color){100, 100, 100, 255});

            // Draw title
            DrawTextEx(custom_font, "Inventory", (Vector2){inv_x, inv_y - 35}, 24, 1, WHITE);

            // Draw big inventory slots (4 rows x 9 columns)
            for (int row = 0; row < BIG_INVENTORY_ROWS; row++) {
                for (int col = 0; col < BIG_INVENTORY_COLS; col++) {
                    int idx = row * BIG_INVENTORY_COLS + col;
                    int x = inv_x + col * (slot_size + slot_spacing);
                    int y = inv_y + row * (slot_size + slot_spacing);

                    // Slot background
                    DrawRectangle(x, y, slot_size, slot_size, (Color){80, 80, 80, 200});
                    DrawRectangleLines(x, y, slot_size, slot_size, (Color){100, 100, 100, 255});

                    // Draw block indicator
                    InventorySlot* slot = &player->big_inventory[idx];
                    if (slot->count > 0) {
                        Color block_color;
                        switch (slot->type) {
                            case BLOCK_STONE: block_color = (Color){128, 128, 128, 255}; break;
                            case BLOCK_DIRT: block_color = (Color){139, 69, 19, 255}; break;
                            case BLOCK_GRASS: block_color = (Color){34, 139, 34, 255}; break;
                            case BLOCK_SAND: block_color = (Color){194, 178, 128, 255}; break;
                            case BLOCK_WOOD: block_color = (Color){139, 90, 43, 255}; break;
                            case BLOCK_BEDROCK: block_color = (Color){64, 64, 64, 255}; break;
                            case BLOCK_GLOWSTONE: block_color = (Color){255, 255, 0, 255}; break;
                            default: block_color = (Color){200, 200, 200, 255}; break;
                        }

                        int block_indent = 8;
                        DrawRectangle(x + block_indent, y + block_indent, slot_size - 2*block_indent, slot_size - 2*block_indent, block_color);

                        // Draw item count
                        char count_str[8];
                        snprintf(count_str, sizeof(count_str), "%d", slot->count);
                        int text_x = x + slot_size - MeasureTextEx(custom_font, count_str, 16, 1).x - 3;
                        int text_y = y + slot_size - 16;
                        DrawTextEx(custom_font, count_str, (Vector2){text_x, text_y}, 16, 1, WHITE);
                    }
                    // Handle mouse interaction for big inventory
                    Vector2 mouse_pos = GetMousePosition();
                    Rectangle slot_rect = (Rectangle){ (float)x, (float)y, (float)slot_size, (float)slot_size };
                    if (CheckCollisionPointRec(mouse_pos, slot_rect)) {
                        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                            // Click handling: pick up or place
                            if (!player->holding_item) {
                                // pick up entire stack
                                if (slot->count > 0) {
                                    player->held_slot = *slot;
                                    player->holding_item = true;
                                    slot->type = BLOCK_AIR;
                                    slot->count = 0;
                                }
                            } else {
                                // place or combine
                                if (slot->count == 0) {
                                    // place held into this slot
                                    *slot = player->held_slot;
                                    player->held_slot.type = BLOCK_AIR;
                                    player->held_slot.count = 0;
                                    player->holding_item = false;
                                } else if (slot->type == player->held_slot.type) {
                                    // merge as much as possible
                                    int can_add = INVENTORY_MAX_STACK - slot->count;
                                    int moved = (player->held_slot.count <= can_add) ? player->held_slot.count : can_add;
                                    slot->count += moved;
                                    player->held_slot.count -= moved;
                                    if (player->held_slot.count <= 0) {
                                        player->held_slot.type = BLOCK_AIR;
                                        player->holding_item = false;
                                    }
                                } else {
                                    // swap
                                    InventorySlot tmp = *slot;
                                    *slot = player->held_slot;
                                    player->held_slot = tmp;
                                }
                            }
                        }
                    }
                }
            }

            // Draw hotbar at bottom of big inventory
            int hotbar_y = inv_y + (BIG_INVENTORY_ROWS + 1) * (slot_size + slot_spacing) + 20;
            for (int i = 0; i < INVENTORY_SIZE; i++) {
                int x = inv_x + i * (slot_size + slot_spacing);
                int y = hotbar_y;

                // Slot background
                Color slot_bg = (Color){80, 80, 80, 200};
                if (i == player->selected_slot) {
                    slot_bg = (Color){100, 100, 100, 255};
                }
                DrawRectangle(x, y, slot_size, slot_size, slot_bg);

                // Draw slot border
                Color border_color = (i == player->selected_slot) ? (Color){255, 255, 255, 255} : (Color){100, 100, 100, 255};
                DrawRectangleLines(x, y, slot_size, slot_size, border_color);

                // Draw block indicator
                InventorySlot* slot = &player->inventory[i];
                if (slot->count > 0) {
                    Color block_color;
                    switch (slot->type) {
                        case BLOCK_STONE: block_color = (Color){128, 128, 128, 255}; break;
                        case BLOCK_DIRT: block_color = (Color){139, 69, 19, 255}; break;
                        case BLOCK_GRASS: block_color = (Color){34, 139, 34, 255}; break;
                        case BLOCK_SAND: block_color = (Color){194, 178, 128, 255}; break;
                        case BLOCK_WOOD: block_color = (Color){139, 90, 43, 255}; break;
                        case BLOCK_BEDROCK: block_color = (Color){64, 64, 64, 255}; break;
                        case BLOCK_GLOWSTONE: block_color = (Color){255, 255, 0, 255}; break;
                        default: block_color = (Color){200, 200, 200, 255}; break;
                    }

                    int block_indent = 8;
                    DrawRectangle(x + block_indent, y + block_indent, slot_size - 2*block_indent, slot_size - 2*block_indent, block_color);

                    // Draw item count
                    char count_str[8];
                    snprintf(count_str, sizeof(count_str), "%d", slot->count);
                    int text_x = x + slot_size - MeasureTextEx(custom_font, count_str, 16, 1).x - 3;
                    int text_y = y + slot_size - 16;
                    DrawTextEx(custom_font, count_str, (Vector2){text_x, text_y}, 16, 1, WHITE);
                }

                // Draw slot number
                char slot_num[4];
                snprintf(slot_num, sizeof(slot_num), "%d", i + 1);
                DrawTextEx(custom_font, slot_num, (Vector2){x + 3, y + 3}, 14, 1, (Color){150, 150, 150, 255});
            }

            // Handle hotbar mouse interaction
            {
                Vector2 mouse_pos = GetMousePosition();
                for (int i = 0; i < INVENTORY_SIZE; i++) {
                    int x = inv_x + i * (slot_size + slot_spacing);
                    int y = hotbar_y;
                    Rectangle slot_rect = (Rectangle){ (float)x, (float)y, (float)slot_size, (float)slot_size };
                    InventorySlot* slot = &player->inventory[i];
                    if (CheckCollisionPointRec(mouse_pos, slot_rect) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        if (!player->holding_item) {
                            if (slot->count > 0) {
                                player->held_slot = *slot;
                                player->holding_item = true;
                                slot->type = BLOCK_AIR;
                                slot->count = 0;
                            }
                        } else {
                            if (slot->count == 0) {
                                *slot = player->held_slot;
                                player->held_slot.type = BLOCK_AIR;
                                player->held_slot.count = 0;
                                player->holding_item = false;
                            } else if (slot->type == player->held_slot.type) {
                                int can_add = INVENTORY_MAX_STACK - slot->count;
                                int moved = (player->held_slot.count <= can_add) ? player->held_slot.count : can_add;
                                slot->count += moved;
                                player->held_slot.count -= moved;
                                if (player->held_slot.count <= 0) {
                                    player->held_slot.type = BLOCK_AIR;
                                    player->holding_item = false;
                                }
                            } else {
                                InventorySlot tmp = *slot;
                                *slot = player->held_slot;
                                player->held_slot = tmp;
                            }
                        }
                    }
                }
            }

            // Draw help text
            // localized close help
            DrawTextEx(custom_font, menu->game_text.inventory_close, (Vector2){inv_x, hotbar_y + slot_size + 10}, 16, 1, (Color){150, 150, 150, 255});
        }

        // draw HUD based on mode
        if (hud_mode == 0) {
            // default HUD
            DrawTextEx(custom_font, menu->game_text.move_controls, (Vector2){10, 10}, 32, 1, BLACK);
            DrawTextEx(custom_font, menu->game_text.metrics_help, (Vector2){10, 50}, 32, 1, BLACK);
            DrawTextEx(custom_font, menu->game_text.mouse_help, (Vector2){10, 90}, 32, 1, BLACK);
            DrawTextEx(custom_font, menu->game_text.look_help, (Vector2){10, 130}, 32, 1, BLACK);
            DrawTextEx(custom_font, menu->game_text.pause_help, (Vector2){10, 170}, 32, 1, BLACK);

            char coord_text[128];
            snprintf(coord_text, sizeof(coord_text), "%s (%.1f, %.1f, %.1f)",
                     menu->game_text.coord_label, player->position.x, player->position.y, player->position.z);
            DrawTextEx(custom_font, coord_text, (Vector2){10, 210}, 32, 1, BLACK);

            char fps_text[64];
            snprintf(fps_text, sizeof(fps_text), "%s %d", menu->game_text.fps_label, GetFPS());
            DrawTextEx(custom_font, fps_text, (Vector2){10, 250}, 32, 1, BLACK);

            DrawTextEx(custom_font, menu->game_text.version, (Vector2){10, 290}, 32, 1, DARKGRAY);
        } else if (hud_mode == 1) {
            // performance metrics HUD
            DrawTextEx(custom_font, menu->game_text.perf_metrics, (Vector2){10, 10}, 32, 1, BLACK);

            char frame_time[64];
            snprintf(frame_time, sizeof(frame_time), "Frame Time: %.2f ms", dt * 1000.0f);
            DrawTextEx(custom_font, frame_time, (Vector2){10, 50}, 32, 1, BLACK);

            char fps_text[32];
            snprintf(fps_text, sizeof(fps_text), "%s %d", menu->game_text.fps_label, GetFPS());
            DrawTextEx(custom_font, fps_text, (Vector2){10, 90}, 32, 1, BLACK);

            char blocks_text[64];
            snprintf(blocks_text, sizeof(blocks_text), "Blocks Rendered: %d", blocks_rendered);
            DrawTextEx(custom_font, blocks_text, (Vector2){10, 130}, 32, 1, BLACK);

            int memory_mb = get_process_memory_mb();
            char memory_text[64];
            snprintf(memory_text, sizeof(memory_text), "Memory Usage: %d MB", memory_mb);
            DrawTextEx(custom_font, memory_text, (Vector2){10, 170}, 32, 1, BLACK);

            char pos_text[64];
            snprintf(pos_text, sizeof(pos_text), "Pos: (%.1f, %.1f, %.1f)",
                     player->position.x, player->position.y, player->position.z);
            DrawTextEx(custom_font, pos_text, (Vector2){10, 210}, 32, 1, BLACK);

            DrawTextEx(custom_font, "b3dv 0.0.18-beta-beta", (Vector2){10, 250}, 32, 1, DARKGRAY);
        } else if (hud_mode == 2) {
            // player stats HUD
            DrawTextEx(custom_font, "=== PLAYER STATS ===", (Vector2){10, 10}, 32, 1, BLACK);

            char fps_text[32];
            snprintf(fps_text, sizeof(fps_text), "FPS: %d", GetFPS());
            DrawTextEx(custom_font, fps_text, (Vector2){10, 50}, 32, 1, BLACK);

            char pos_text[64];
            snprintf(pos_text, sizeof(pos_text), "Pos: (%.1f, %.1f, %.1f)",
                     player->position.x, player->position.y, player->position.z);
            DrawTextEx(custom_font, pos_text, (Vector2){10, 90}, 32, 1, BLACK);

            // calculate speed (magnitude of velocity)
            // Use actual movement for speedometer
            float dx = player->position.x - player->prev_position.x;
            float dy = player->position.y - player->prev_position.y;
            float dz = player->position.z - player->prev_position.z;
            float actual_speed = sqrtf(dx*dx + dy*dy + dz*dz) / dt;
            char speed_text[64];
            snprintf(speed_text, sizeof(speed_text), "Speed: %.2f m/s", actual_speed);
            DrawTextEx(custom_font, speed_text, (Vector2){10, 130}, 32, 1, BLACK);

            // momentum/velocity components
            char momentum_text[96];
            snprintf(momentum_text, sizeof(momentum_text), "Vel: (%.2f, %.2f, %.2f) m/s",
                     player->velocity.x, player->velocity.y, player->velocity.z);
            DrawTextEx(custom_font, momentum_text, (Vector2){10, 170}, 32, 1, BLACK);

            DrawTextEx(custom_font, "b3dv 0.0.18-beta-beta", (Vector2){10, 250}, 32, 1, DARKGRAY);
        } else if (hud_mode == 3) {
            // system info HUD (using cached values)
            DrawTextEx(custom_font, "=== SYSTEM INFO ===", (Vector2){10, 10}, 32, 1, BLACK);
            DrawTextEx(custom_font, cached_cpu, (Vector2){10, 50}, 32, 1, BLACK);
            DrawTextEx(custom_font, cached_gpu, (Vector2){10, 90}, 32, 1, BLACK);
            DrawTextEx(custom_font, cached_kernel, (Vector2){10, 130}, 32, 1, BLACK);
            DrawTextEx(custom_font, "b3dv 0.0.18-beta-beta", (Vector2){10, 250}, 32, 1, DARKGRAY);
        }

        // Draw chat message history (last few messages with fade-out)
        double current_time = GetTime();
        double message_lifetime = 5.0;  // Messages stay for 5 seconds
        int screen_height = GetScreenHeight();
        int message_start_y = screen_height - 200;  // Start drawing messages from bottom-left
        int messages_shown = 0;

        for (int i = 0; i < CHAT_MESSAGE_BUFFER_SIZE && messages_shown < 5; i++) {
            // Find the most recent messages (iterate backwards)
            int msg_index = (chat_message_count - 1 - i) % CHAT_MESSAGE_BUFFER_SIZE;
            if (i >= chat_message_count) break;  // Don't show messages that haven't been set yet

            double message_age = current_time - chat_message_times[msg_index];
            if (message_age < message_lifetime && strlen(chat_messages[msg_index]) > 0) {
                // Calculate fade: full opacity for first 4 seconds, fade out in last 1 second
                float fade_factor = 1.0f;
                if (message_age > 4.0) {
                    fade_factor = 1.0f - (message_age - 4.0f);  // Fade over last 1 second
                    fade_factor = fade_factor < 0 ? 0 : fade_factor;
                }

                Color msg_color = (Color){255, 255, 255, (unsigned char)(255 * fade_factor)};
                int y_offset = message_start_y - (messages_shown * 35);
                DrawTextEx(custom_font, chat_messages[msg_index], (Vector2){10, y_offset}, 28, 1, msg_color);
                messages_shown++;
            }
        }

        // display pause menu with buttons if paused
        if (paused) {
            if (pause_settings_open) {
                // Draw settings panel in pause menu
                int screen_width = GetScreenWidth();
                int screen_height = GetScreenHeight();

                // Clear background
                DrawRectangle(0, 0, screen_width, screen_height, (Color){0, 0, 0, 150});

                // Draw title
                Vector2 title_size = MeasureTextEx(custom_font, menu->game_text.settings, 64, 2);
                DrawTextEx(custom_font, menu->game_text.settings,
                           (Vector2){(screen_width - title_size.x) / 2, 40},
                           64, 2, WHITE);

                // Settings panel
                int panel_width = 600;
                int panel_height = 300;
                int panel_x = (screen_width - panel_width) / 2;
                int panel_y = 120;

                DrawRectangle(panel_x - 10, panel_y - 10, panel_width + 20, panel_height + 20, (Color){40, 40, 40, 255});
                DrawRectangleLines(panel_x - 10, panel_y - 10, panel_width + 20, panel_height + 20, WHITE);

                // Render Distance slider
                int slider_y = panel_y + 30;
                int slider_x = panel_x + 50;
                int slider_width = 500;
                int slider_height = 20;

                // Draw label
                DrawTextEx(custom_font, menu->game_text.render_dist_label, (Vector2){panel_x + 30, slider_y - 35}, 28, 1, WHITE);

                // Draw value
                char render_dist_str[32];
                snprintf(render_dist_str, sizeof(render_dist_str), "%.0f", menu->render_distance);
                DrawTextEx(custom_font, render_dist_str, (Vector2){panel_x + 500, slider_y - 35}, 28, 1, GRAY);

                // Draw slider background
                DrawRectangle(slider_x, slider_y, slider_width, slider_height, (Color){60, 60, 60, 255});
                DrawRectangleLines(slider_x, slider_y, slider_width, slider_height, WHITE);

                // Calculate slider knob position
                float render_dist_normalized = (menu->render_distance - 10.0f) / (100.0f - 10.0f);
                render_dist_normalized = render_dist_normalized < 0 ? 0 : (render_dist_normalized > 1 ? 1 : render_dist_normalized);
                int knob_x = slider_x + (int)(render_dist_normalized * slider_width);

                // Draw knob
                DrawRectangle(knob_x - 6, slider_y - 5, 12, slider_height + 10, LIGHTGRAY);
                DrawRectangleLines(knob_x - 6, slider_y - 5, 12, slider_height + 10, WHITE);

                // Handle render distance slider input
                Vector2 mouse_pos = GetMousePosition();
                Rectangle render_slider_rect = {(float)slider_x, (float)(slider_y - 10), (float)slider_width, slider_height + 20};

                if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse_pos, render_slider_rect)) {
                    float new_pos = (mouse_pos.x - slider_x) / slider_width;
                    new_pos = new_pos < 0 ? 0 : (new_pos > 1 ? 1 : new_pos);
                    menu->render_distance = 10.0f + (new_pos * (100.0f - 10.0f));
                    menu_save_settings(menu);
                }

                // Max FPS slider
                int fps_slider_y = slider_y + 100;

                // Draw label
                DrawTextEx(custom_font, menu->game_text.max_fps_label, (Vector2){panel_x + 30, fps_slider_y - 35}, 28, 1, WHITE);

                // Draw value
                char fps_str[32];
                if (menu->max_fps == 0) {
                    snprintf(fps_str, sizeof(fps_str), "%s", menu->game_text.uncapped);
                } else {
                    snprintf(fps_str, sizeof(fps_str), "%d", menu->max_fps);
                }
                DrawTextEx(custom_font, fps_str, (Vector2){panel_x + 500, fps_slider_y - 35}, 28, 1, GRAY);

                // Draw slider background
                DrawRectangle(slider_x, fps_slider_y, slider_width, slider_height, (Color){60, 60, 60, 255});
                DrawRectangleLines(slider_x, fps_slider_y, slider_width, slider_height, WHITE);

                // Calculate FPS slider knob position (30-240, or 0 for uncapped)
                float fps_normalized;
                if (menu->max_fps == 0) {
                    fps_normalized = 1.0f;  // Show at the right end when uncapped
                } else {
                    fps_normalized = (menu->max_fps - 30) / (240.0f - 30);
                    fps_normalized = fps_normalized < 0 ? 0 : (fps_normalized > 1 ? 1 : fps_normalized);
                }
                int fps_knob_x = slider_x + (int)(fps_normalized * slider_width);

                // Draw knob
                DrawRectangle(fps_knob_x - 6, fps_slider_y - 5, 12, slider_height + 10, LIGHTGRAY);
                DrawRectangleLines(fps_knob_x - 6, fps_slider_y - 5, 12, slider_height + 10, WHITE);

                // Handle FPS slider input
                Rectangle fps_slider_rect = {(float)slider_x, (float)(fps_slider_y - 10), (float)slider_width, slider_height + 20};

                if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse_pos, fps_slider_rect)) {
                    float new_pos = (mouse_pos.x - slider_x) / slider_width;
                    new_pos = new_pos < 0 ? 0 : (new_pos > 1 ? 1 : new_pos);
                    if (new_pos >= 0.95f) {
                        menu->max_fps = 0;  // 0 means uncapped
                    } else {
                        menu->max_fps = (int)(30 + (new_pos * (240 - 30)));
                        if (menu->max_fps < 30) menu->max_fps = 30;
                    }
                    menu_save_settings(menu);
                }

                // Back button to return to pause menu
                int button_width = 450;
                int button_height = 60;
                int button_y = fps_slider_y + 100;

                Rectangle back_button = {
                    (float)((screen_width - button_width) / 2),
                    (float)button_y,
                    (float)button_width,
                    (float)button_height
                };

                bool back_hover = CheckCollisionPointRec(mouse_pos, back_button);

                DrawRectangleRec(back_button, back_hover ? LIGHTGRAY : GRAY);
                DrawRectangleLinesEx(back_button, 2, WHITE);
                Vector2 back_text_size = MeasureTextEx(custom_font, menu->text_back, 32, 1);
                DrawTextEx(custom_font, menu->text_back,
                           (Vector2){(float)screen_width / 2 - back_text_size.x / 2, (float)button_y + 12},
                           32, 1, BLACK);

                // Handle back button
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && back_hover) {
                    pause_settings_open = false;
                }

                // Also allow ESC to go back
                if (IsKeyPressed(KEY_ESCAPE)) {
                    pause_settings_open = false;
                }
            } else {
                // Draw main pause menu
                // get screen dimensions dynamically
                int screen_width = GetScreenWidth();
                int screen_height = GetScreenHeight();

                // draw semi-transparent overlay
                DrawRectangle(0, 0, screen_width, screen_height, (Color){0, 0, 0, 150});

                // measure text to center it
                Vector2 paused_size = MeasureTextEx(custom_font, menu->game_text.paused, 64, 2);

                // draw title
                DrawTextEx(custom_font, menu->game_text.paused,
                           (Vector2){(screen_width - paused_size.x) / 2, screen_height / 2 - 120},
                           64, 2, /*RED*/WHITE);

                // button dimensions
                int button_width = 450;
                int button_height = 60;
                int button_spacing = 20;
                int center_x = screen_width / 2;
                int center_y = screen_height / 2 - 20;

                // resume button
                Rectangle resume_button = {
                    center_x - button_width / 2,
                    center_y,
                    button_width,
                    button_height
                };

                // settings button
                Rectangle settings_button = {
                    center_x - button_width / 2,
                    center_y + button_height + button_spacing,
                    button_width,
                    button_height
                };

                // quit button
                Rectangle quit_button = {
                    center_x - button_width / 2,
                    center_y + 2 * (button_height + button_spacing),
                    button_width,
                    button_height
                };

                // get mouse position
                Vector2 mouse_pos = GetMousePosition();
                bool resume_hover = CheckCollisionPointRec(mouse_pos, resume_button);
                bool settings_hover = CheckCollisionPointRec(mouse_pos, settings_button);
                bool quit_hover = CheckCollisionPointRec(mouse_pos, quit_button);

                // draw resume button
                DrawRectangleRec(resume_button, resume_hover ? LIGHTGRAY : GRAY);
                DrawRectangleLinesEx(resume_button, 2, WHITE);
                Vector2 resume_text_size = MeasureTextEx(custom_font, menu->game_text.resume, 32, 1);
                DrawTextEx(custom_font, menu->game_text.resume,
                           (Vector2){center_x - resume_text_size.x / 2, center_y + 12},
                           32, 1, BLACK);

                // draw settings button
                DrawRectangleRec(settings_button, settings_hover ? LIGHTGRAY : GRAY);
                DrawRectangleLinesEx(settings_button, 2, WHITE);
                Vector2 settings_text_size = MeasureTextEx(custom_font, menu->game_text.settings, 32, 1);
                DrawTextEx(custom_font, menu->game_text.settings,
                           (Vector2){center_x - settings_text_size.x / 2, center_y + button_height + button_spacing + 12},
                           32, 1, BLACK);

                // draw quit button
                DrawRectangleRec(quit_button, quit_hover ? LIGHTGRAY : GRAY);
                DrawRectangleLinesEx(quit_button, 2, WHITE);
                Vector2 quit_text_size = MeasureTextEx(custom_font, menu->game_text.back_to_menu, 32, 1);
                DrawTextEx(custom_font, menu->game_text.back_to_menu,
                           (Vector2){center_x - quit_text_size.x / 2, center_y + 2 * (button_height + button_spacing) + 12},
                           32, 1, BLACK);

                // handle button clicks
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    if (resume_hover) {
                        paused = false;
                        // Recapture mouse if it was captured before pause
                        mouse_captured = true;
                        DisableCursor();
                    } else if (settings_hover) {
                        pause_settings_open = true;
                    } else if (quit_hover) {
                        // Save world and return to main menu
                        if (player) {
                            world->last_player_position = player->position;
                        }
                        world_save(world, world->world_name);
                        paused = false;
                        should_quit = false;
                        // Reset menu state to main
                        menu->current_state = MENU_STATE_MAIN;
                        // Free world and player
                        world_unload_textures(world);
                        world_free(world);
                        player_free(player);
                        clouds_free(clouds);
                        world = NULL;
                        player = NULL;
                        clouds = NULL;
                        // Release mouse
                        mouse_captured = false;
                        EnableCursor();
                    }
                }
            }
        }

        // display chat input if active
        if (chat_active) {
            int screen_width = GetScreenWidth();
            int screen_height = GetScreenHeight();

            // chat box at bottom
            int chat_box_height = 50;
            int chat_box_y = screen_height - chat_box_height - 10;

            DrawRectangle(10, chat_box_y, screen_width - 20, chat_box_height, (Color){30, 30, 30, 200});
            DrawRectangleLinesEx((Rectangle){10, chat_box_y, screen_width - 20, chat_box_height}, 2, WHITE);

            // draw chat input text with prompt
            char chat_display[512];
            snprintf(chat_display, sizeof(chat_display), "> %s", chat_input);
            DrawTextEx(custom_font, chat_display, (Vector2){20, chat_box_y + 8}, 28, 1, WHITE);

            // draw blinking cursor at cursor position
            if ((int)(GetTime() * 2) % 2 == 0) {
                // measure text up to cursor position (accounting for "> " prefix)
                char text_before_cursor[256];
                strncpy(text_before_cursor, chat_input, chat_cursor_pos);
                text_before_cursor[chat_cursor_pos] = '\0';
                char display_before[512];
                snprintf(display_before, sizeof(display_before), "> %s", text_before_cursor);

                Vector2 cursor_pos = MeasureTextEx(custom_font, display_before, 28, 1);
                DrawLineEx((Vector2){20 + cursor_pos.x, chat_box_y + 8},
                          (Vector2){20 + cursor_pos.x, chat_box_y + 38}, 2, WHITE);
            }
        }

        EndDrawing();
    }

    // Save world before closing (if one was loaded)
    if (world && player) {
        world->last_player_position = player->position;
        world_save(world, world->world_name);
    }

    // Save settings before closing
    menu_save_settings(menu);

    // Clean up menu system
    menu_system_free(menu);

    UnloadFont(custom_font);
    if (player) player_free(player);
    if (clouds) clouds_free(clouds);
    if (world) {
        world_unload_textures(world);  // Unload textures before closing
        world_free(world);
    }
    CloseWindow();
    return 0;
}
