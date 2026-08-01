#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "../include/menu.h"
#include "../include/player.h"
#include "raylib.h"
#include "../include/rendering.h"
#include "../include/utils.h"
#include "../include/vec_math.h"
#include "../include/world.h"
// #include "../include/clouds.h"
#include "../include/aux.h"
#include "../include/console.h"
#include "../include/game_server.h"

typedef struct {
    float dist_sq;
    Vector3 world_pos;
    int world_x;
    int world_y;
    int world_z;
    uint8_t exposed_faces;
} GlassRenderEntry;

static int compare_glass_entries(const void *a, const void *b) {
    const GlassRenderEntry *ea = (const GlassRenderEntry *)a;
    const GlassRenderEntry *eb = (const GlassRenderEntry *)b;
    if (ea->dist_sq < eb->dist_sq) {
        return 1;
    }
    if (ea->dist_sq > eb->dist_sq) {
        return -1;
    }
    return 0;
}

// Returns how far the third-person camera can be pulled back from `from`
// along `dir` (normalized) before it would end up inside a solid block,
// clamped to max_distance. This is what keeps the camera from clipping
// through walls when the player backs up against something.
static float third_person_camera_clearance(World *world, Vector3 from, Vector3 dir, float max_distance) {
    const float step = 0.05f;
    const float margin = 0.15f; // keep the camera slightly off the wall surface

    float traveled = 0.0f;
    while (traveled < max_distance) {
        float next = traveled + step;
        Vector3 sample = vec3_add(from, vec3_scale(dir, next));

        int block_x = (int)floorf(sample.x);
        int block_y = (int)floorf(sample.y);
        int block_z = (int)floorf(sample.z);

        if (world_get_block(world, block_x, block_y, block_z) != BLOCK_AIR) {
            float safe = traveled - margin;
            return safe > 0.0f ? safe : 0.0f;
        }

        traveled = next;
    }

    return max_distance;
}
static void draw_player_model(Vector3 position) {
    // `position` is the vertical center of the model.
    const Color model_color = (Color){180, 180, 180, 255};
    const float model_radius = 0.30f;
    const float half_height = PLAYER_HEIGHT * 0.5f - model_radius; // center -> each hemisphere center

    Vector3 start = {position.x, position.y - half_height, position.z};
    Vector3 end   = {position.x, position.y + half_height, position.z};

    DrawCapsule(start, end, model_radius, 16, 8, model_color);
}

#if defined(PLATFORM_DESKTOP)
#define SDF_GLSL_VER 330
#else
#define SDF_GLSL_VER 100
#endif

static Shader sdf_shader = {0};

// graphics and player constants
#define WINDOW_WIDTH 1200
#define WINDOW_HEIGHT 800
#define TARGET_FPS 144
#define RENDER_DISTANCE 50.0f
#define FOG_START 30.0f
#define CULLING_FOV 110.0f

int b3dv_main(int argc, char **argv) {

    do_args(argc, *&argv);
    if (neutrino_detection) {
        return wait_for_neutrino();
    }

    // Disable HIGHDPI to avoid fractional scaling issues with Hyprland's 1.2x compositor scaling
    // Render at 1200x800 logical pixels; let window manager handle physical scaling
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "b3dv 0.0.25-beta");

    // Load SDF shader from project assets (avoid external/ references)
    sdf_shader = (Shader){0};
    int try_versions[] = {330, 120, 100};
    for (int i = 0; i < 3; i++) {
        int ver = try_versions[i];
        if (ver == 0) {
            continue;
        }
        char vsPath[256];
        char fsPath[256];
        snprintf(vsPath, sizeof(vsPath), "assets/shaders/glsl%d/sdf.vs", ver);
        snprintf(fsPath, sizeof(fsPath), "assets/shaders/glsl%d/sdf.fs", ver);
        if (FileExists(vsPath) && FileExists(fsPath)) {
            sdf_shader = LoadShader(vsPath, fsPath);
            if (sdf_shader.id != 0) {
                TraceLog(LOG_INFO, "Loaded SDF shader from %s and %s", vsPath, fsPath);
                break;
            } else {
                TraceLog(LOG_WARNING, "Failed to load shader files %s / %s", vsPath, fsPath);
            }
        }
    }

    // Disable default ESC key behavior (we handle it manually for pause menu)
    SetExitKey(KEY_NULL);

    // Enable info-level logging for diagnosis
    SetTraceLogLevel(LOG_INFO);

    // Debug: print window and DPI info
    Vector2 dpi = GetWindowScaleDPI();
    TraceLog(LOG_INFO, "Window size: %dx%d, DPI scale: (%f, %f)", GetScreenWidth(), GetScreenHeight(), dpi.x, dpi.y);

    // Initialize world save system (needed for menu world scanning)
    world_system_init();

    // Create menu system
    MenuSystem *menu = menu_system_create();

    // Initialize console input system
    console_init();

    // Set target FPS based on menu settings (will be updated dynamically)
    SetTargetFPS(menu->max_fps);
    int last_target_fps = menu->max_fps;

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
        .position = (Vector3){20, 15, 20},
        .target = (Vector3){8, 4, 8},
        .up = (Vector3){0, 1, 0},
        .fovy = 90,
        .projection = CAMERA_PERSPECTIVE};

    // World and player will be created after menu selection
    World *world = NULL;
    Player *player = NULL;
    GameServer game_server = {0};
    float server_accumulator = 0.0f;
    const float SERVER_FIXED_DT = 1.0f / 60.0f;
    // CloudSystem* clouds = NULL;

    // enable mouse capture (will be disabled in menu)
    bool mouse_captured = false;

    // HUD mode (0 = default, 1 = performance metrics 2 = player, 3 = system info)
    int hud_mode = 0;
    int prev_hud_mode = -1;  // Track previous mode to detect changes
    bool hud_visible = true; // Toggle HUD visibility with F1
    char cached_cpu[128] = {0};
    char cached_gpu[128] = {0};
    char cached_kernel[128] = {0};

    // Wireframe rendering mode (toggled with F7)
    bool show_wireframe = false;
    bool show_chunk_borders = false;

    // Flight system
    bool flight_enabled = false;
    bool third_person_camera = false;

    // Pause state
    bool paused = false;
    bool pause_settings_open = false;
    bool should_quit = false;

    // Chat system
    bool chat_active = false;
    char chat_input[256] = {0};
    int chat_input_len = 0;
    int chat_cursor_pos = 0; // cursor position in input string
    int history_index = 0;   // 0 means no history entry selected, 1+ means Nth-from-last

// Chat message display (feedback messages)
#define CHAT_MESSAGE_BUFFER_SIZE 16
#define CHAT_MESSAGE_MAX_LEN 512
    char chat_messages[CHAT_MESSAGE_BUFFER_SIZE][CHAT_MESSAGE_MAX_LEN] = {0};
    double chat_message_times[CHAT_MESSAGE_BUFFER_SIZE] = {0};
    int chat_message_count = 0;

// Macro to add a chat message to the buffer
#define add_chat_message(message)                                              \
    do {                                                                       \
        int _msg_idx = chat_message_count % CHAT_MESSAGE_BUFFER_SIZE;          \
        strncpy(chat_messages[_msg_idx], (message), CHAT_MESSAGE_MAX_LEN - 1); \
        chat_messages[_msg_idx][CHAT_MESSAGE_MAX_LEN - 1] = '\0';              \
        chat_message_times[_msg_idx] = GetTime();                              \
        chat_message_count++;                                                  \
    } while (0)

    // Camera angle tracking (in radians)
    float camera_yaw = 0.0f;   // horizontal rotation around Y axis
    float camera_pitch = 0.0f; // vertical rotation (pitch)

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

    B3DV_MAIN_LOOP
    while (!WindowShouldClose() && !should_quit) {
        float dt = GetFrameTime();

        // Check if font family or variant changed in settings and reload if needed
        if (strcmp(menu->font_families[menu->current_font_family_index], last_loaded_family) != 0 ||
            strcmp(menu->font_variants[menu->current_font_variant_index], last_loaded_variant) != 0) {
            // Unload old font if it's not the default
            if (custom_font.glyphCount > 0 && custom_font.glyphCount != GetFontDefault().glyphCount) {
                UnloadFont(custom_font);
            }
            // Reset SDF flag when font changes
            sdf_font_is_sdf = false;
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
        } else if (menu->current_state == MENU_STATE_WORLD_SELECT) {
            BeginDrawing();
            menu_draw_world_select(menu, custom_font);
            EndDrawing();
            menu_update_input(menu);
            continue;
        } else if (menu->current_state == MENU_STATE_CREATE_WORLD) {
            BeginDrawing();
            menu_draw_create_world(menu, custom_font);
            EndDrawing();
            menu_update_input(menu);
            continue;
        } else if (menu->current_state == MENU_STATE_CREDITS) {
            BeginDrawing();
            menu_draw_credits(menu, custom_font);
            EndDrawing();
            menu_update_input(menu);
            continue;
        } else if (menu->current_state == MENU_STATE_SETTINGS) {
            BeginDrawing();
            menu_draw_settings(menu, custom_font);
            EndDrawing();
            menu_update_input(menu);
            continue;
        } else if (menu->current_state == MENU_STATE_GAME && menu->should_start_game) {
            // Initialize game with selected world
            if (!world) {
                world = world_create();
                world->compress_chunk_files = menu->create_world_compress;
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
                // Register player with world and apply saved inventory if present
                world->current_player = player;
                world_apply_players_to(world, player);
                game_server_reset(&game_server, world, player);
                if (menu->nickname[0] != '\0') {
                    strncpy(world->player_nickname, menu->nickname, sizeof(world->player_nickname) - 1);
                    world->player_nickname[sizeof(world->player_nickname) - 1] = '\0';
                    strncpy(player->nickname, menu->nickname, sizeof(player->nickname) - 1);
                    player->nickname[sizeof(player->nickname) - 1] = '\0';
                }
#if 0
                // Create cloud system
                clouds = clouds_create("./assets/textures/clouds.png");
                // Apply cloud settings from menu
                if (clouds) {
                    clouds->enabled = menu->clouds_enabled;
                    clouds->render_distance = menu->clouds_render_distance;
                }
#endif
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

        // Update target FPS from settings only when it changes to avoid repeated INFO messages
        if (menu->max_fps != last_target_fps) {
            SetTargetFPS(menu->max_fps);
            last_target_fps = menu->max_fps;
        }

        // Chat system - handle first to consume all input when active
        if (chat_active) {
            // Text input
            int key = GetCharPressed();
            while (key > 0) {
                if ((key >= 32 && key <= 125) && chat_input_len < 255) {
                    for (int i = chat_input_len; i > chat_cursor_pos; i--) {
                        chat_input[i] = chat_input[i - 1];
                    }
                    chat_input[chat_cursor_pos++] = (char)key;
                    chat_input_len++;
                    chat_input[chat_input_len] = '\0';
                }
                key = GetCharPressed();
            }

            if (IsKeyPressed(KEY_BACKSPACE) && chat_cursor_pos > 0) {
                for (int i = chat_cursor_pos - 1; i < chat_input_len; i++) {
                    chat_input[i] = chat_input[i + 1];
                }
                chat_cursor_pos--;
                chat_input_len--;
                chat_input[chat_input_len] = '\0';
            }

            if (IsKeyPressed(KEY_LEFT) && chat_cursor_pos > 0) {
                chat_cursor_pos--;
            }
            if (IsKeyPressed(KEY_RIGHT) && chat_cursor_pos < chat_input_len) {
                chat_cursor_pos++;
            }

            if (IsKeyPressed(KEY_UP)) {
                history_index++;
                char history_line[256] = {0};
                if (get_chat_history_line(history_index, history_line, sizeof(history_line))) {
                    strncpy(chat_input, history_line, sizeof(chat_input) - 1);
                    chat_input[sizeof(chat_input) - 1] = '\0';
                    chat_input_len = strlen(chat_input);
                    chat_cursor_pos = chat_input_len;
                } else {
                    history_index--;
                }
            }

            if (IsKeyPressed(KEY_DOWN)) {
                history_index--;
                if (history_index <= 0) {
                    history_index = 0;
                    chat_input[0] = '\0';
                    chat_input_len = 0;
                    chat_cursor_pos = 0;
                } else {
                    char history_line[256] = {0};
                    if (get_chat_history_line(history_index, history_line, sizeof(history_line))) {
                        strncpy(chat_input, history_line, sizeof(chat_input) - 1);
                        chat_input[sizeof(chat_input) - 1] = '\0';
                        chat_input_len = strlen(chat_input);
                        chat_cursor_pos = chat_input_len;
                    }
                }
            }

            if (IsKeyPressed(KEY_ENTER)) {
                if (chat_input_len > 0) {
                    FILE *log_file = fopen("./chathistory", "a");
                    if (log_file) {
                        fprintf(log_file, "%s\n", chat_input);
                        fclose(log_file);
                    }
                }

                // Process command or chat
                if (chat_input[0] == '/') {
                    ConsoleCommand parsed_cmd = console_parse_command(chat_input);
                    bool should_quit_cmd = false;
                    bool flight_enabled_cmd = flight_enabled;
                    bool show_chunk_borders_cmd = show_chunk_borders;
                    char command_msg[512] = {0};
                    if (game_server_submit_command(&game_server, player->uid, &parsed_cmd, chat_input,
                                                   &should_quit_cmd, &flight_enabled_cmd,
                                                   &show_chunk_borders_cmd, &world, &player,
                                                   command_msg, sizeof(command_msg))) {
                        if (command_msg[0] != '\0') {
                            add_chat_message(command_msg);
                        }
                        if (should_quit_cmd) {
                            should_quit = true;
                        }
                        flight_enabled = flight_enabled_cmd;
                        show_chunk_borders = show_chunk_borders_cmd;
                    } else {
                        char msg[512];
                        snprintf(msg, sizeof(msg), menu->game_text.msg_unknown_command, chat_input);
                        add_chat_message(msg);
                    }
                } else {
                    char out_msg[1024];
                    const char *nick = (world->player_nickname[0] != '\0') ? world->player_nickname : "Player";
                    snprintf(out_msg, sizeof(out_msg), "<%s> %s", nick, chat_input);
                    add_chat_message(out_msg);
                }

                // finalize chat state
                chat_active = false;
                chat_cursor_pos = 0;
                history_index = 0;
                // Recapture mouse if it was captured before chat
                mouse_captured = true;
                DisableCursor();
            }

            if (IsKeyPressed(KEY_ESCAPE)) {
                chat_active = false;
                chat_cursor_pos = 0;
                history_index = 0;
                mouse_captured = true;
                DisableCursor();
            }
        } else {
            // Only process game keys when chat is not active
            // HUD visibility toggle with F1
            if (IsKeyPressed(KEY_F1)) {
                hud_visible = !hud_visible; // Toggle HUD on/off
            }
            // HUD mode toggle with F2/F3/F4/F5
            if (IsKeyPressed(KEY_F2)) {
                hud_mode = 0; // default HUD
            }
            if (IsKeyPressed(KEY_F3)) {
                hud_mode = 1; // performance metrics
            }
            if (IsKeyPressed(KEY_F4)) {
                hud_mode = 2; // player
            }
            if (IsKeyPressed(KEY_F5)) {
                third_person_camera = !third_person_camera;
                if (third_person_camera) {
                    add_chat_message(menu->game_text.msg_third_person_camera);
                } else {
                    add_chat_message(menu->game_text.msg_first_person_camera);
                }
            }
            if (IsKeyPressed(KEY_F7)) {
                show_wireframe = !show_wireframe; // Toggle wireframe rendering
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
                    if (!paused && mouse_captured) {
                        DisableCursor();
                    }
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
                player->position = (Vector3){8.0f, 105.0f, 8.0f};
                player->velocity = (Vector3){0, 0, 0};
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
        } // End of else block for non-chat input

        // Console system - check for commands from terminal
        ConsoleCommand console_cmd;
        while (console_get_next_command(&console_cmd)) {
            bool should_quit_cmd = false;
            bool flight_enabled_cmd = flight_enabled;
            bool show_chunk_borders_cmd = show_chunk_borders;
            char command_msg[512] = {0};
            World *world_after = world;
            Player *player_after = player;

            if (game_server_submit_command(&game_server, player->uid, &console_cmd, console_cmd.raw_input,
                                           &should_quit_cmd, &flight_enabled_cmd,
                                           &show_chunk_borders_cmd, &world_after, &player_after,
                                           command_msg, sizeof(command_msg))) {
                if (command_msg[0] != '\0') {
                    add_chat_message(command_msg);
                }
                if (should_quit_cmd) {
                    should_quit = true;
                }
                world = world_after;
                player = player_after;
                flight_enabled = flight_enabled_cmd;
                show_chunk_borders = show_chunk_borders_cmd;
            } else {
                add_chat_message(menu->game_text.msg_console_command_failed);
            }
        }

        // handle mouse look (must happen before getting forward/right)
        if (mouse_captured) {
            Vector2 mouse_delta = GetMouseDelta();
            float mouse_speed = 0.005f;

            // Update pitch (vertical rotation) with clamping
            float new_pitch = camera_pitch - mouse_delta.y * mouse_speed;

            // Clamp pitch to very close to ±90 degrees (e.g., 89.9°)
            float pitch_limit = 1.56905f; // ~89.9 degrees in radians
            if (new_pitch > pitch_limit) {
                new_pitch = pitch_limit;
            }
            if (new_pitch < -pitch_limit) {
                new_pitch = -pitch_limit;
            }
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
                cos_yaw * cos_pitch};
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
                camera_up = (Vector3){0, 1, 0}; // Keep world up
            }
        }

        // Movement should follow the camera's horizontal basis. Project the camera
        // basis vectors onto the XZ plane so movement stays consistent even when
        // looking up or down.
        Vector3 right = camera_right;
        right.y = 0.0f;
        float right_len = sqrtf(right.x * right.x + right.z * right.z);
        if (right_len < 1e-6f) {
            right = (Vector3){-cosf(camera_yaw), 0.0f, sinf(camera_yaw)};
        } else {
            right.x /= right_len;
            right.z /= right_len;
        }

        Vector3 forward = camera_forward;
        forward.y = 0.0f;
        float forward_len = sqrtf(forward.x * forward.x + forward.z * forward.z);
        if (forward_len < 1e-6f) {
            forward = (Vector3){-sinf(camera_yaw), 0.0f, -cosf(camera_yaw)};
        } else {
            forward.x /= forward_len;
            forward.z /= forward_len;
        }

        // Handle player input first so the server tick receives the latest movement command.
        if (!paused && !chat_active && !inventory_is_big_open(player)) {
            PlayerInputCommand input_cmd = {0};
            input_cmd.selected_slot = player->selected_slot;
            input_cmd.shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
            input_cmd.sprint = IsKeyDown(KEY_LEFT_CONTROL);
            input_cmd.jump = IsKeyDown(KEY_SPACE);
            input_cmd.fly_toggle = IsKeyPressed(KEY_SPACE);

            float move_speed = PLAYER_SPEED;
            if (input_cmd.shift) {
                move_speed *= 0.5f;
            } else if (input_cmd.sprint) {
                move_speed *= 1.5f;
            }

            Vector3 move = {0, 0, 0};
            if (IsKeyDown(KEY_W)) {
                move = vec3_add(move, vec3_scale(forward, move_speed));
            }
            if (IsKeyDown(KEY_S)) {
                move = vec3_add(move, vec3_scale(forward, -move_speed));
            }
            if (IsKeyDown(KEY_D)) {
                move = vec3_add(move, vec3_scale(right, move_speed));
            }
            if (IsKeyDown(KEY_A)) {
                move = vec3_add(move, vec3_scale(right, -move_speed));
            }

            float move_len = sqrtf(move.x * move.x + move.z * move.z);
            if (move_len > move_speed) {
                float scale = move_speed / move_len;
                move.x *= scale;
                move.z *= scale;
            }

            input_cmd.move_x = move.x;
            input_cmd.move_z = move.z;
            game_server_submit_input(&game_server, player->uid, &input_cmd);

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
                            adj_z + 0.5f};

                        // Simple AABB collision check: block (1x1x1) vs player (radius 0.35 cylinder with height 1.9)
                        // Check if block center is within player's horizontal radius
                        float dx = player->position.x - block_center.x;
                        float dz = player->position.z - block_center.z;
                        float horizontal_dist = sqrtf(dx * dx + dz * dz);

                        // Check vertical overlap (player head at position.y, feet at position.y - PLAYER_HEIGHT)
                        float player_bottom = player->position.y - PLAYER_HEIGHT;
                        float player_top = player->position.y;
                        float block_bottom = adj_y;
                        float block_top = adj_y + 1.0f;

                        bool vertical_overlap = (block_bottom < player_top && block_top > player_bottom);
                        bool horizontal_overlap = (horizontal_dist < PLAYER_RADIUS + 0.5f); // 0.5 is half block width

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
            if (IsKeyPressed(KEY_ONE)) {
                player->selected_slot = 0;
            }
            if (IsKeyPressed(KEY_TWO)) {
                player->selected_slot = 1;
            }
            if (IsKeyPressed(KEY_THREE)) {
                player->selected_slot = 2;
            }
            if (IsKeyPressed(KEY_FOUR)) {
                player->selected_slot = 3;
            }
            if (IsKeyPressed(KEY_FIVE)) {
                player->selected_slot = 4;
            }
            if (IsKeyPressed(KEY_SIX)) {
                player->selected_slot = 5;
            }
            if (IsKeyPressed(KEY_SEVEN)) {
                player->selected_slot = 6;
            }
            if (IsKeyPressed(KEY_EIGHT)) {
                player->selected_slot = 7;
            }
            if (IsKeyPressed(KEY_NINE)) {
                player->selected_slot = 8;
            }

            // Handle big inventory toggle (I key)
            if (IsKeyPressed(KEY_I)) {
                inventory_toggle_big(player);
                if (inventory_is_big_open(player)) {
                    // Opened inventory: show cursor and stop mouse-look
                    mouse_captured = false;
                    EnableCursor();
                } else {
                    // Closed inventory: restore capture if gameplay expects it
                    if (!paused && mouse_captured) {
                        DisableCursor();
                    }
                }
            }
        }

        // Update physics and world always (unless game is paused), even if chat is active
        if (!paused) {
            game_server_set_interest(&game_server, player->position, camera_forward, menu->render_distance);
            server_accumulator += dt;
            while (server_accumulator >= SERVER_FIXED_DT) {
                game_server_tick(&game_server, SERVER_FIXED_DT);
                server_accumulator -= SERVER_FIXED_DT;
            }
            // clouds_update(clouds, player->position);  // Update cloud positions
        }

        // update camera to follow player (position it at eye level, slightly above center)
        float eye_height = 0.7f;
        Vector3 player_eye = (Vector3){
            player->position.x,
            player->position.y + eye_height,
            player->position.z};

        if (third_person_camera) {
            const float tp_distance = 4.0f; // desired distance behind the head

            // Anchor to the player's head and translate backward along the SAME
            // direction the camera is actually looking (yaw + pitch), then pull
            // it forward if a block would be in the way.
            Vector3 back_dir = vec3_scale(camera_forward, -1.0f);
            float clearance = third_person_camera_clearance(world, player_eye, back_dir, tp_distance);

            camera.position = vec3_add(player_eye, vec3_scale(back_dir, clearance));
            camera.target = vec3_add(player_eye, camera_forward);
        } else {
            camera.position = player_eye;
            // Use the actual look direction for camera target (includes pitch)
            camera.target = (Vector3){
                player->position.x + camera_forward.x,
                player->position.y + eye_height + camera_forward.y,
                player->position.z + camera_forward.z};
        }

        // (mouse look handled earlier in the loop)

        BeginDrawing();
        ClearBackground(SKYBLUE);

        // Camera-relative rendering: shift camera to block-aligned grid for float precision
        // Align to the nearest block boundary to avoid jittering with negative coordinates
        Vector3 original_camera_pos = camera.position;
        Vector3 camera_offset = (Vector3){
            floorf(camera.position.x),
            0,
            floorf(camera.position.z)};

        // Shift camera and target for rendering (only for graphics)
        camera.position.x -= camera_offset.x;
        camera.position.y -= camera_offset.y;
        camera.position.z -= camera_offset.z;
        camera.target.x -= camera_offset.x;
        camera.target.y -= camera_offset.y;
        camera.target.z -= camera_offset.z;

        // Use shifted camera position for visibility checks
        Vector3 shifted_cam_pos = camera.position;

        int blocks_rendered = 0; // Declared here so it's accessible after the if block

        BeginMode3D(camera);
        // Use the actual camera orientation for rendering/frustum culling
        Vector3 cam_forward = camera_forward;
        Vector3 cam_right = camera_right;

        // CRITICAL: Take a snapshot of chunk pointers while holding cache_mutex
        // This prevents chunks from being unloaded during rendering
        pthread_mutex_lock(&world->cache_mutex);
        int chunk_count_snapshot = world->chunk_cache.chunk_count;
        // Allocate temporary array for chunk pointers
        Chunk **chunks_snapshot = (Chunk **)malloc(chunk_count_snapshot * sizeof(Chunk *));
        for (int i = 0; i < chunk_count_snapshot; i++) {
            chunks_snapshot[i] = &world->chunk_cache.chunks[i];
        }
        pthread_mutex_unlock(&world->cache_mutex);

        // draw all chunks and their blocks with frustum culling and face culling
        GlassRenderEntry *glass_entries = NULL;
        int glass_count = 0;
        int glass_capacity = 0;

        for (int c = 0; c < chunk_count_snapshot; c++) {
            Chunk *chunk = chunks_snapshot[c];

            // Skip unloaded chunks
            if (!chunk->loaded) {
                continue;
            }

            // Skip chunks that haven't been generated yet
            if (!chunk->generated) {
                continue;
            }

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
            CachedVisibleBlock *visible_blocks_copy = (CachedVisibleBlock *)malloc(visible_count * sizeof(CachedVisibleBlock));
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
                    world_z + 0.5f - camera_offset.z};
                // distance-based LOD: use squared distance to avoid sqrt
                Vector3 to_block = vec3_sub(world_pos, shifted_cam_pos);
                float dist_sq = to_block.x * to_block.x + to_block.y * to_block.y + to_block.z * to_block.z;

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

                float dist = sqrtf(dist_sq); // Only calc sqrt if we're actually rendering
                Color color = world_get_block_color(block);

                // apply fog effect: fade color towards sky blue based on distance
                float fog_factor = 0.0f;
                float fog_start = menu->render_distance * 0.6f; // Fog starts at 60% of render distance
                if (dist > fog_start) {
                    fog_factor = (dist - fog_start) / (menu->render_distance - fog_start);
                    fog_factor = fog_factor > 1.0f ? 1.0f : fog_factor; // Clamp to 0-1

                    // blend color towards sky blue
                    color.r = (unsigned char)(color.r * (1.0f - fog_factor) + SKYBLUE.r * fog_factor);
                    color.g = (unsigned char)(color.g * (1.0f - fog_factor) + SKYBLUE.g * fog_factor);
                    color.b = (unsigned char)(color.b * (1.0f - fog_factor) + SKYBLUE.b * fog_factor);
                }

                // apply fog to wireframe too
                Color wire_color = MAGENTA;
                if (fog_factor > 0.0f) {
                    wire_color.r = (unsigned char)(wire_color.r * (1.0f - fog_factor) + SKYBLUE.r * fog_factor);
                    wire_color.g = (unsigned char)(wire_color.g * (1.0f - fog_factor) + SKYBLUE.g * fog_factor);
                    wire_color.b = (unsigned char)(wire_color.b * (1.0f - fog_factor) + SKYBLUE.b * fog_factor);
                    wire_color.a = (unsigned char)(255 * (1.0f - fog_factor)); // Fade out alpha too
                }
                if (block != BLOCK_GLASS) {
                    // draw opaque blocks first
                    draw_cube_faces(world_pos, 1.0f, color, camera.position, wire_color, world, world_x, world_y, world_z, block, visible_blocks_copy[i].exposed_faces, show_wireframe);
                    blocks_rendered++;
                }
            }

            for (int i = 0; i < visible_count; i++) {
                int x = visible_blocks_copy[i].x;
                int y = visible_blocks_copy[i].y;
                int z = visible_blocks_copy[i].z;
                BlockType block = world_chunk_get_block(chunk, x, y, z);
                if (block != BLOCK_GLASS) {
                    continue;
                }

                int world_x = chunk->chunk_x * CHUNK_WIDTH + x;
                int world_y = chunk->chunk_y * CHUNK_HEIGHT + y;
                int world_z = chunk->chunk_z * CHUNK_DEPTH + z;

                Vector3 world_pos = (Vector3){
                    world_x + 0.5f - camera_offset.x,
                    world_y + 0.5f - camera_offset.y,
                    world_z + 0.5f - camera_offset.z};

                Vector3 to_block = vec3_sub(world_pos, shifted_cam_pos);
                float dist_sq = to_block.x * to_block.x + to_block.y * to_block.y + to_block.z * to_block.z;

                if (glass_count >= glass_capacity) {
                    glass_capacity = glass_capacity > 0 ? glass_capacity * 2 : 64;
                    GlassRenderEntry *new_entries = (GlassRenderEntry *)realloc(glass_entries, sizeof(GlassRenderEntry) * glass_capacity);
                    if (new_entries == NULL) {
                        // OOM: keep existing entries and skip adding this glass block
                        break;
                    }
                    glass_entries = new_entries;
                }

                glass_entries[glass_count].dist_sq = dist_sq;
                glass_entries[glass_count].world_pos = world_pos;
                glass_entries[glass_count].world_x = world_x;
                glass_entries[glass_count].world_y = world_y;
                glass_entries[glass_count].world_z = world_z;
                glass_entries[glass_count].exposed_faces = visible_blocks_copy[i].exposed_faces;
                glass_count++;
            }

            // Free the temporary copy
            free(visible_blocks_copy);
        }

        // Draw transparent glass blocks after opaque blocks so blending is correct
        if (glass_count > 0) {
            qsort(glass_entries, glass_count, sizeof(GlassRenderEntry), compare_glass_entries);
            for (int i = 0; i < glass_count; i++) {
                GlassRenderEntry *entry = &glass_entries[i];
                Color color = world_get_block_color(BLOCK_GLASS);
                Color wire_color = MAGENTA;
                draw_cube_faces(entry->world_pos, 1.0f, color, camera.position, wire_color,
                                world, entry->world_x, entry->world_y, entry->world_z,
                                BLOCK_GLASS, entry->exposed_faces, show_wireframe);
                blocks_rendered++;
            }
            free(glass_entries);
            glass_entries = NULL;
            glass_count = 0;
            glass_capacity = 0;
        }

        // Draw highlighting box around the block being looked at
        if (has_highlighted_block) {
            Vector3 block_pos = (Vector3){
                highlighted_block_x + 0.5f - camera_offset.x,
                highlighted_block_y + 0.5f - camera_offset.y,
                highlighted_block_z + 0.5f - camera_offset.z};
            DrawCubeWires(block_pos, 1.0f, 1.0f, 1.0f, YELLOW);
        }
        DrawGrid(30, 1.0f);

        if (third_person_camera && player) {
            Vector3 player_model_pos = (Vector3){
                player->position.x - camera_offset.x,
                player->position.y - camera_offset.y,
                player->position.z - camera_offset.z};
            draw_player_model(player_model_pos);
        }

        if (show_chunk_borders) {
            Color border_color = (Color){255, 255, 0, 150};
            Color fill_color = (Color){255, 255, 0, 45};

            int player_chunk_x = (int)floorf(original_camera_pos.x / CHUNK_WIDTH);
            int player_chunk_z = (int)floorf(original_camera_pos.z / CHUNK_DEPTH);

            for (int c = 0; c < chunk_count_snapshot; c++) {
                Chunk *chunk = chunks_snapshot[c];
                if (!chunk->loaded || !chunk->generated) {
                    continue;
                }
                if (chunk->chunk_x < player_chunk_x - 1 || chunk->chunk_x > player_chunk_x + 1) {
                    continue;
                }
                if (chunk->chunk_z < player_chunk_z - 1 || chunk->chunk_z > player_chunk_z + 1) {
                    continue;
                }

                Vector3 chunk_min = (Vector3){
                    chunk->chunk_x * CHUNK_WIDTH - camera_offset.x,
                    chunk->chunk_y * CHUNK_HEIGHT - camera_offset.y,
                    chunk->chunk_z * CHUNK_DEPTH - camera_offset.z};
                Vector3 chunk_center = (Vector3){
                    chunk_min.x + CHUNK_WIDTH * 0.5f,
                    chunk_min.y + CHUNK_HEIGHT * 0.5f,
                    chunk_min.z + CHUNK_DEPTH * 0.5f};

                // Draw translucent faces to show full chunk bounds
                DrawCube(chunk_center, (float)CHUNK_WIDTH, (float)CHUNK_HEIGHT, (float)CHUNK_DEPTH, fill_color);
                DrawCubeWires(chunk_center, (float)CHUNK_WIDTH, (float)CHUNK_HEIGHT, (float)CHUNK_DEPTH, border_color);
            }
        }
#if 0
            // Draw clouds (sync with menu settings)
            if (clouds) {
                clouds->enabled = menu->clouds_enabled;
                clouds->render_distance = menu->clouds_render_distance;
            }
            clouds_draw(clouds, camera.position, camera_offset);
#endif
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
                    slot_bg = (Color){100, 100, 100, 255}; // Highlight selected slot
                }
                DrawRectangle(x, y, slot_size, slot_size, slot_bg);

                // Draw slot border
                Color border_color = (i == player->selected_slot) ? (Color){255, 255, 255, 255} : (Color){100, 100, 100, 255};
                DrawRectangleLines(x, y, slot_size, slot_size, border_color);

                // Draw block indicator (colored square representing block type)
                InventorySlot *slot = &player->inventory[i];
                if (slot->count > 0) {
                    // Map block type to color
                    Color block_color;
                    switch (slot->type) {
                    case BLOCK_STONE:
                        block_color = (Color){128, 128, 128, 255};
                        break;
                    case BLOCK_DIRT:
                        block_color = (Color){139, 69, 19, 255};
                        break;
                    case BLOCK_GRASS:
                        block_color = (Color){34, 139, 34, 255};
                        break;
                    case BLOCK_SAND:
                        block_color = (Color){194, 178, 128, 255};
                        break;
                    case BLOCK_WOOD:
                        block_color = (Color){139, 90, 43, 255};
                        break;
                    case BLOCK_BEDROCK:
                        block_color = (Color){64, 64, 64, 255};
                        break;
                    case BLOCK_GLOWSTONE:
                        block_color = (Color){255, 255, 0, 255};
                        break;
                    case BLOCK_GLASS:
                        block_color = (Color){200, 230, 255, 255};
                        break;
                    case BLOCK_COBBLESTONE:
                        block_color = (Color){128, 128, 128, 255};
                        break;
                    default:
                        block_color = (Color){200, 200, 200, 255};
                        break;
                    }

                    // Draw colored block indicator (smaller than slot)
                    int block_indent = 10;
                    DrawRectangle(x + block_indent, y + block_indent, slot_size - 2 * block_indent, slot_size - 2 * block_indent, block_color);

                    // Draw item count
                    char count_str[8];
                    snprintf(count_str, sizeof(count_str), "%d", slot->count);
                    int text_x = x + slot_size - MeasureTextEx(custom_font, count_str, 20, 1).x - 5;
                    int text_y = y + slot_size - 20;
                    DrawTextExCustom(custom_font, count_str, (Vector2){text_x, text_y}, 20, 1, WHITE);
                }

                // Draw slot number
                char slot_num[4];
                snprintf(slot_num, sizeof(slot_num), "%d", i + 1);
                DrawTextExCustom(custom_font, slot_num, (Vector2){x + 3, y + 3}, 16, 1, (Color){150, 150, 150, 255});
            }
        }

        // Draw big inventory UI when open
        if (inventory_is_big_open(player)) {
            int slot_size = 40;
            int slot_spacing = 4;
            int inv_width = BIG_INVENTORY_COLS * slot_size + (BIG_INVENTORY_COLS - 1) * slot_spacing;
            int inv_height = (BIG_INVENTORY_ROWS + 1) * slot_size + (BIG_INVENTORY_ROWS)*slot_spacing;
            int inv_x = (GetScreenWidth() - inv_width) / 2;
            int inv_y = (GetScreenHeight() - inv_height) / 2;

            // Draw background panel
            DrawRectangle(inv_x - 10, inv_y - 10, inv_width + 20, inv_height + 20, (Color){50, 50, 50, 220});
            DrawRectangleLines(inv_x - 10, inv_y - 10, inv_width + 20, inv_height + 20, (Color){100, 100, 100, 255});

            // Draw title
            DrawTextExCustom(custom_font, "Inventory", (Vector2){inv_x, inv_y - 35}, 24, 1, WHITE);

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
                    InventorySlot *slot = &player->big_inventory[idx];
                    if (slot->count > 0) {
                        Color block_color;
                        switch (slot->type) {
                        case BLOCK_STONE:
                            block_color = (Color){128, 128, 128, 255};
                            break;
                        case BLOCK_DIRT:
                            block_color = (Color){139, 69, 19, 255};
                            break;
                        case BLOCK_GRASS:
                            block_color = (Color){34, 139, 34, 255};
                            break;
                        case BLOCK_SAND:
                            block_color = (Color){194, 178, 128, 255};
                            break;
                        case BLOCK_WOOD:
                            block_color = (Color){139, 90, 43, 255};
                            break;
                        case BLOCK_BEDROCK:
                            block_color = (Color){64, 64, 64, 255};
                            break;
                        case BLOCK_GLOWSTONE:
                            block_color = (Color){255, 255, 0, 255};
                            break;
                        case BLOCK_GLASS:
                            block_color = (Color){200, 230, 255, 255};
                            break;
                        case BLOCK_COBBLESTONE:
                            block_color = (Color){128, 128, 128, 255};
                            break;
                        default:
                            block_color = (Color){200, 200, 200, 255};
                            break;
                        }

                        int block_indent = 8;
                        DrawRectangle(x + block_indent, y + block_indent, slot_size - 2 * block_indent, slot_size - 2 * block_indent, block_color);

                        // Draw item count
                        char count_str[8];
                        snprintf(count_str, sizeof(count_str), "%d", slot->count);
                        int text_x = x + slot_size - MeasureTextEx(custom_font, count_str, 16, 1).x - 3;
                        int text_y = y + slot_size - 16;
                        DrawTextExCustom(custom_font, count_str, (Vector2){text_x, text_y}, 16, 1, WHITE);
                    }
                    // Handle mouse interaction for big inventory
                    Vector2 mouse_pos = GetMousePosition();
                    Rectangle slot_rect = (Rectangle){(float)x, (float)y, (float)slot_size, (float)slot_size};
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
                InventorySlot *slot = &player->inventory[i];
                if (slot->count > 0) {
                    Color block_color;
                    switch (slot->type) {
                    case BLOCK_STONE:
                        block_color = (Color){128, 128, 128, 255};
                        break;
                    case BLOCK_DIRT:
                        block_color = (Color){139, 69, 19, 255};
                        break;
                    case BLOCK_GRASS:
                        block_color = (Color){34, 139, 34, 255};
                        break;
                    case BLOCK_SAND:
                        block_color = (Color){194, 178, 128, 255};
                        break;
                    case BLOCK_WOOD:
                        block_color = (Color){139, 90, 43, 255};
                        break;
                    case BLOCK_BEDROCK:
                        block_color = (Color){64, 64, 64, 255};
                        break;
                    case BLOCK_GLOWSTONE:
                        block_color = (Color){255, 255, 0, 255};
                        break;
                    case BLOCK_COBBLESTONE:
                        block_color = (Color){128, 128, 128, 255};
                        break;
                    default:
                        block_color = (Color){200, 200, 200, 255};
                        break;
                    }

                    int block_indent = 8;
                    DrawRectangle(x + block_indent, y + block_indent, slot_size - 2 * block_indent, slot_size - 2 * block_indent, block_color);

                    // Draw item count
                    char count_str[8];
                    snprintf(count_str, sizeof(count_str), "%d", slot->count);
                    int text_x = x + slot_size - MeasureTextEx(custom_font, count_str, 16, 1).x - 3;
                    int text_y = y + slot_size - 16;
                    DrawTextExCustom(custom_font, count_str, (Vector2){text_x, text_y}, 16, 1, WHITE);
                }

                // Draw slot number
                char slot_num[4];
                snprintf(slot_num, sizeof(slot_num), "%d", i + 1);
                DrawTextExCustom(custom_font, slot_num, (Vector2){x + 3, y + 3}, 14, 1, (Color){150, 150, 150, 255});
            }

            // Handle hotbar mouse interaction
            {
                Vector2 mouse_pos = GetMousePosition();
                for (int i = 0; i < INVENTORY_SIZE; i++) {
                    int x = inv_x + i * (slot_size + slot_spacing);
                    int y = hotbar_y;
                    Rectangle slot_rect = (Rectangle){(float)x, (float)y, (float)slot_size, (float)slot_size};
                    InventorySlot *slot = &player->inventory[i];
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
            DrawTextExCustom(custom_font, menu->game_text.inventory_close, (Vector2){inv_x, hotbar_y + slot_size + 10}, 16, 1, (Color){150, 150, 150, 255});
        }

        // draw HUD based on mode
        if (hud_visible && hud_mode == 0) {
            // default HUD
            DrawTextExCustom(custom_font, menu->game_text.move_controls, (Vector2){10, 10}, 32, 1, BLACK);
            DrawTextExCustom(custom_font, menu->game_text.metrics_help, (Vector2){10, 50}, 32, 1, BLACK);
            DrawTextExCustom(custom_font, menu->game_text.mouse_help, (Vector2){10, 90}, 32, 1, BLACK);
            DrawTextExCustom(custom_font, menu->game_text.look_help, (Vector2){10, 130}, 32, 1, BLACK);
            DrawTextExCustom(custom_font, menu->game_text.pause_help, (Vector2){10, 170}, 32, 1, BLACK);

            char coord_text[128];
            snprintf(coord_text, sizeof(coord_text), "%s (%.1f, %.1f, %.1f)",
                     menu->game_text.coord_label, player->position.x, player->position.y, player->position.z);
            DrawTextExCustom(custom_font, coord_text, (Vector2){10, 210}, 32, 1, BLACK);

            char fps_text[64];
            snprintf(fps_text, sizeof(fps_text), "%s %d", menu->game_text.fps_label, GetFPS());
            DrawTextExCustom(custom_font, fps_text, (Vector2){10, 250}, 32, 1, BLACK);

            DrawTextExCustom(custom_font, menu->game_text.version, (Vector2){10, 290}, 32, 1, DARKGRAY);
        } else if (hud_visible && hud_mode == 1) {
            // performance metrics HUD
            DrawTextExCustom(custom_font, menu->game_text.perf_metrics, (Vector2){10, 10}, 32, 1, BLACK);

            char frame_time[64];
            snprintf(frame_time, sizeof(frame_time), "Frame Time: %.2f ms", dt * 1000.0f);
            DrawTextExCustom(custom_font, frame_time, (Vector2){10, 50}, 32, 1, BLACK);

            char fps_text[32];
            snprintf(fps_text, sizeof(fps_text), "%s %d", menu->game_text.fps_label, GetFPS());
            DrawTextExCustom(custom_font, fps_text, (Vector2){10, 90}, 32, 1, BLACK);

            char blocks_text[64];
            snprintf(blocks_text, sizeof(blocks_text), "Blocks Rendered: %d", blocks_rendered);
            DrawTextExCustom(custom_font, blocks_text, (Vector2){10, 130}, 32, 1, BLACK);

            int memory_mb = get_process_memory_mb();
            char memory_text[64];
            snprintf(memory_text, sizeof(memory_text), "Memory Usage: %d MB", memory_mb);
            DrawTextExCustom(custom_font, memory_text, (Vector2){10, 170}, 32, 1, BLACK);

            char pos_text[64];
            snprintf(pos_text, sizeof(pos_text), "Pos: (%.1f, %.1f, %.1f)",
                     player->position.x, player->position.y, player->position.z);
            DrawTextExCustom(custom_font, pos_text, (Vector2){10, 210}, 32, 1, BLACK);

            DrawTextExCustom(custom_font, "b3dv 0.0.25-beta", (Vector2){10, 250}, 32, 1, DARKGRAY);
        } else if (hud_visible && hud_mode == 2) {
            // player stats HUD
            DrawTextExCustom(custom_font, "=== PLAYER STATS ===", (Vector2){10, 10}, 32, 1, BLACK);

            char fps_text[32];
            snprintf(fps_text, sizeof(fps_text), "FPS: %d", GetFPS());
            DrawTextExCustom(custom_font, fps_text, (Vector2){10, 50}, 32, 1, BLACK);

            char pos_text[64];
            snprintf(pos_text, sizeof(pos_text), "Pos: (%.1f, %.1f, %.1f)",
                     player->position.x, player->position.y, player->position.z);
            DrawTextExCustom(custom_font, pos_text, (Vector2){10, 90}, 32, 1, BLACK);

            // calculate speed (magnitude of velocity)
            // Use actual movement for speedometer
            float dx = player->position.x - player->prev_position.x;
            float dy = player->position.y - player->prev_position.y;
            float dz = player->position.z - player->prev_position.z;
            float actual_speed = sqrtf(dx * dx + dy * dy + dz * dz) / dt;
            char speed_text[64];
            snprintf(speed_text, sizeof(speed_text), "Speed: %.2f m/s", actual_speed);
            DrawTextExCustom(custom_font, speed_text, (Vector2){10, 130}, 32, 1, BLACK);

            // momentum/velocity components
            char momentum_text[96];
            snprintf(momentum_text, sizeof(momentum_text), "Vel: (%.2f, %.2f, %.2f) m/s",
                     player->velocity.x, player->velocity.y, player->velocity.z);
            DrawTextExCustom(custom_font, momentum_text, (Vector2){10, 170}, 32, 1, BLACK);

            DrawTextExCustom(custom_font, "b3dv 0.0.25-beta", (Vector2){10, 250}, 32, 1, DARKGRAY);
        } else if (hud_visible && hud_mode == 3) {
            // system info HUD (using cached values)
            DrawTextExCustom(custom_font, "=== SYSTEM INFO ===", (Vector2){10, 10}, 32, 1, BLACK);
            DrawTextExCustom(custom_font, cached_cpu, (Vector2){10, 50}, 32, 1, BLACK);
            DrawTextExCustom(custom_font, cached_gpu, (Vector2){10, 90}, 32, 1, BLACK);
            DrawTextExCustom(custom_font, cached_kernel, (Vector2){10, 130}, 32, 1, BLACK);
            DrawTextExCustom(custom_font, "b3dv 0.0.25-beta", (Vector2){10, 250}, 32, 1, DARKGRAY);
        }

        if (hud_visible && menu->compass_enabled) {
            draw_compass_hud(custom_font, camera_yaw);
        }

        // Draw chat message history (last few messages with fade-out)
        double current_time = GetTime();
        double message_lifetime = 5.0; // Messages stay for 5 seconds
        int screen_height = GetScreenHeight();
        int message_start_y = screen_height - 200; // Start drawing messages from bottom-left
        int messages_shown = 0;

        for (int i = 0; i < CHAT_MESSAGE_BUFFER_SIZE && messages_shown < 5; i++) {
            // Find the most recent messages (iterate backwards)
            int msg_index = (chat_message_count - 1 - i) % CHAT_MESSAGE_BUFFER_SIZE;
            if (i >= chat_message_count) {
                break; // Don't show messages that haven't been set yet
            }

            double message_age = current_time - chat_message_times[msg_index];
            if (message_age < message_lifetime && strlen(chat_messages[msg_index]) > 0) {
                // Calculate fade: full opacity for first 4 seconds, fade out in last 1 second
                float fade_factor = 1.0f;
                if (message_age > 4.0) {
                    fade_factor = 1.0f - (message_age - 4.0f); // Fade over last 1 second
                    fade_factor = fade_factor < 0 ? 0 : fade_factor;
                }

                Color msg_color = (Color){255, 255, 255, (unsigned char)(255 * fade_factor)};
                int y_offset = message_start_y - (messages_shown * 35);
                DrawTextExCustom(custom_font, chat_messages[msg_index], (Vector2){10, y_offset}, 28, 1, msg_color);
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
                DrawTextExCustom(custom_font, menu->game_text.settings,
                                 (Vector2){(screen_width - title_size.x) / 2, 40},
                                 64, 2, WHITE);

                // Settings panel
                int panel_width = 700;
                int panel_height = 700;
                int panel_x = (screen_width - panel_width) / 2;
                int panel_y = 30;

                DrawRectangle(panel_x - 10, panel_y - 10, panel_width + 20, panel_height + 20, (Color){40, 40, 40, 255});
                DrawRectangleLines(panel_x - 10, panel_y - 10, panel_width + 20, panel_height + 20, WHITE);

                // Render Distance slider
                int slider_y = panel_y + 30;
                int slider_x = panel_x + 50;
                int slider_width = 500;
                int slider_height = 20;

                // Draw label
                DrawTextExCustom(custom_font, menu->game_text.render_dist_label, (Vector2){panel_x + 30, slider_y - 35}, 28, 1, WHITE);

                // Draw value
                char render_dist_str[32];
                snprintf(render_dist_str, sizeof(render_dist_str), "%.0f", menu->render_distance);
                DrawTextExCustom(custom_font, render_dist_str, (Vector2){panel_x + 500, slider_y - 35}, 28, 1, GRAY);

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
                DrawTextExCustom(custom_font, menu->game_text.max_fps_label, (Vector2){panel_x + 30, fps_slider_y - 35}, 28, 1, WHITE);

                // Draw value
                char fps_str[32];
                if (menu->max_fps == 0) {
                    snprintf(fps_str, sizeof(fps_str), "%s", menu->game_text.uncapped);
                } else {
                    snprintf(fps_str, sizeof(fps_str), "%d", menu->max_fps);
                }
                DrawTextExCustom(custom_font, fps_str, (Vector2){panel_x + 500, fps_slider_y - 35}, 28, 1, GRAY);

                // Draw slider background
                DrawRectangle(slider_x, fps_slider_y, slider_width, slider_height, (Color){60, 60, 60, 255});
                DrawRectangleLines(slider_x, fps_slider_y, slider_width, slider_height, WHITE);

                // Calculate FPS slider knob position (30-240, or 0 for uncapped)
                float fps_normalized;
                if (menu->max_fps == 0) {
                    fps_normalized = 1.0f; // Show at the right end when uncapped
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
                        menu->max_fps = 0; // 0 means uncapped
                    } else {
                        menu->max_fps = (int)(30 + (new_pos * (240 - 30)));
                        if (menu->max_fps < 30) {
                            menu->max_fps = 30;
                        }
                    }
                    menu_save_settings(menu);
                }

                // Nickname input
                int nickname_y = fps_slider_y + 90;
                int nickname_box_x = panel_x + 50;
                int nickname_box_width = 500;
                int nickname_box_height = 40;
                Rectangle nickname_box = {
                    (float)nickname_box_x,
                    (float)nickname_y,
                    (float)nickname_box_width,
                    (float)nickname_box_height};

                DrawTextExCustom(custom_font, "Nickname:", (Vector2){panel_x + 30, nickname_y - 35}, 24, 1, WHITE);
                DrawRectangleRec(nickname_box, (Color){60, 60, 60, 255});
                DrawRectangleLinesEx(nickname_box, 2, menu->nickname_edit_active ? YELLOW : WHITE);
                DrawTextExCustom(custom_font, menu->nickname,
                                 (Vector2){nickname_box.x + 10, nickname_box.y + 8},
                                 24, 1, WHITE);

                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    if (CheckCollisionPointRec(mouse_pos, nickname_box)) {
                        menu->nickname_edit_active = true;
                    } else {
                        menu->nickname_edit_active = false;
                    }
                }

                if (menu->nickname_edit_active) {
                    int key = GetCharPressed();
                    bool changed = false;
                    while (key > 0) {
                        if ((key >= 32 && key <= 126) && menu->nickname_len < (int)sizeof(menu->nickname) - 1) {
                            menu->nickname[menu->nickname_len++] = (char)key;
                            menu->nickname[menu->nickname_len] = '\0';
                            changed = true;
                        }
                        key = GetCharPressed();
                    }

                    if (IsKeyPressed(KEY_BACKSPACE) && menu->nickname_len > 0) {
                        menu->nickname_len--;
                        menu->nickname[menu->nickname_len] = '\0';
                        changed = true;
                    }

                    if (changed) {
                        menu_save_settings(menu);
                    }
                }

                if (menu->nickname_edit_active && (int)(GetTime() * 2) % 2 == 0) {
                    Vector2 cursor_pos = MeasureTextEx(custom_font, menu->nickname, 24, 1);
                    DrawLineEx((Vector2){nickname_box.x + 10 + cursor_pos.x, nickname_box.y + 8},
                               (Vector2){nickname_box.x + 10 + cursor_pos.x, nickname_box.y + nickname_box_height - 8},
                               2, WHITE);
                }

                /*// Cloud render distance slider
                int cloud_dist_y = nickname_y + 90;

                // Draw label
                DrawTextExCustom(custom_font, "Cloud Distance:", (Vector2){panel_x + 30, cloud_dist_y - 35}, 24, 1, WHITE);

                // Draw value
                char cloud_dist_str[32];
                snprintf(cloud_dist_str, sizeof(cloud_dist_str), "%.0f", menu->clouds_render_distance);
                DrawTextExCustom(custom_font, cloud_dist_str, (Vector2){panel_x + 500, cloud_dist_y - 35}, 24, 1, GRAY);

                // Draw slider background
                DrawRectangle(slider_x, cloud_dist_y, slider_width, slider_height, (Color){60, 60, 60, 255});
                DrawRectangleLines(slider_x, cloud_dist_y, slider_width, slider_height, WHITE);

                // Calculate cloud distance slider knob position
                float cloud_dist_normalized = (menu->clouds_render_distance - 32.0f) / (512.0f - 32.0f);
                cloud_dist_normalized = cloud_dist_normalized < 0 ? 0 : (cloud_dist_normalized > 1 ? 1 : cloud_dist_normalized);
                int cloud_knob_x = slider_x + (int)(cloud_dist_normalized * slider_width);

                // Draw knob
                DrawRectangle(cloud_knob_x - 6, cloud_dist_y - 5, 12, slider_height + 10, LIGHTGRAY);
                DrawRectangleLines(cloud_knob_x - 6, cloud_dist_y - 5, 12, slider_height + 10, WHITE);

                // Handle cloud distance slider input
                Rectangle cloud_slider_rect = {(float)slider_x, (float)(cloud_dist_y - 10), (float)slider_width, slider_height + 20};

                if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse_pos, cloud_slider_rect)) {
                    float new_pos = (mouse_pos.x - slider_x) / slider_width;
                    new_pos = new_pos < 0 ? 0 : (new_pos > 1 ? 1 : new_pos);
                    menu->clouds_render_distance = 32.0f + (new_pos * (512.0f - 32.0f));
                    menu_save_settings(menu);
                }

                // Cloud toggle checkbox
                int cloud_toggle_y = cloud_dist_y + 60;
                int checkbox_size = 30;
                int checkbox_x = panel_x + 50;
                Rectangle cloud_checkbox = {(float)checkbox_x, (float)cloud_toggle_y, (float)checkbox_size, (float)checkbox_size};

                DrawRectangleRec(cloud_checkbox, (Color){60, 60, 60, 255});
                DrawRectangleLinesEx(cloud_checkbox, 2, WHITE);
                if (menu->clouds_enabled) {
                    DrawRectangle(checkbox_x + 5, cloud_toggle_y + 5, checkbox_size - 10, checkbox_size - 10, (Color){100, 200, 100, 255});
                }

                DrawTextExCustom(custom_font, "Clouds Enabled", (Vector2){checkbox_x + checkbox_size + 15, cloud_toggle_y + 3}, 24, 1, WHITE);

                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse_pos, cloud_checkbox)) {
                    menu->clouds_enabled = !menu->clouds_enabled;
                    menu_save_settings(menu);
                }*/

                // Back button to return to pause menu
                int button_width = 450;
                int button_height = 60;
                int button_y = nickname_y + 150; // Adjusted position since cloud UI is now disabled

                Rectangle back_button = {
                    ((float)screen_width - (float)button_width) / 2.0f,
                    (float)button_y,
                    (float)button_width,
                    (float)button_height};

                bool back_hover = CheckCollisionPointRec(mouse_pos, back_button);

                DrawRectangleRec(back_button, back_hover ? LIGHTGRAY : GRAY);
                DrawRectangleLinesEx(back_button, 2, WHITE);
                Vector2 back_text_size = MeasureTextEx(custom_font, menu->text_back, 32, 1);
                DrawTextExCustom(custom_font, menu->text_back,
                                 (Vector2){((float)screen_width / 2.0f) - (back_text_size.x / 2.0f), (float)button_y + 12.0f},
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
                DrawTextExCustom(custom_font, menu->game_text.paused,
                                 (Vector2){((float)screen_width - paused_size.x) / 2.0f, (float)screen_height / 2.0f - 120.0f},
                                 64, 2, /*RED*/ WHITE);

                // button dimensions
                int button_width = 450;
                int button_height = 60;
                int button_spacing = 20;
                int center_x = screen_width / 2;
                int center_y = screen_height / 2 - 20;

                // resume button
                Rectangle resume_button = {
                    ((float)center_x - (float)button_width / 2.0f),
                    (float)center_y,
                    (float)button_width,
                    (float)button_height};

                // settings button
                Rectangle settings_button = {
                    ((float)center_x - (float)button_width / 2.0f),
                    (float)(center_y + button_height + button_spacing),
                    (float)button_width,
                    (float)button_height};

                // quit button
                Rectangle quit_button = {
                    ((float)center_x - (float)button_width / 2.0f),
                    (float)(center_y + 2 * (button_height + button_spacing)),
                    (float)button_width,
                    (float)button_height};

                // get mouse position
                Vector2 mouse_pos = GetMousePosition();
                bool resume_hover = CheckCollisionPointRec(mouse_pos, resume_button);
                bool settings_hover = CheckCollisionPointRec(mouse_pos, settings_button);
                bool quit_hover = CheckCollisionPointRec(mouse_pos, quit_button);

                // draw resume button
                DrawRectangleRec(resume_button, resume_hover ? LIGHTGRAY : GRAY);
                DrawRectangleLinesEx(resume_button, 2, WHITE);
                Vector2 resume_text_size = MeasureTextEx(custom_font, menu->game_text.resume, 32, 1);
                DrawTextExCustom(custom_font, menu->game_text.resume,
                                 (Vector2){center_x - resume_text_size.x / 2, center_y + 12},
                                 32, 1, BLACK);

                // draw settings button
                DrawRectangleRec(settings_button, settings_hover ? LIGHTGRAY : GRAY);
                DrawRectangleLinesEx(settings_button, 2, WHITE);
                Vector2 settings_text_size = MeasureTextEx(custom_font, menu->game_text.settings, 32, 1);
                DrawTextExCustom(custom_font, menu->game_text.settings,
                                 (Vector2){center_x - settings_text_size.x / 2, center_y + button_height + button_spacing + 12},
                                 32, 1, BLACK);

                // draw quit button
                DrawRectangleRec(quit_button, quit_hover ? LIGHTGRAY : GRAY);
                DrawRectangleLinesEx(quit_button, 2, WHITE);
                Vector2 quit_text_size = MeasureTextEx(custom_font, menu->game_text.back_to_menu, 32, 1);
                DrawTextExCustom(custom_font, menu->game_text.back_to_menu,
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
                        // clouds_free(clouds);
                        world = NULL;
                        player = NULL;
                        // clouds = NULL;
                        //  Release mouse
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
            DrawTextExCustom(custom_font, chat_display, (Vector2){20, chat_box_y + 8}, 28, 1, WHITE);

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
    B3DV_MAIN_LOOP

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
    if (player) {
        player_free(player);
    }
    // if (clouds) clouds_free(clouds);
    if (world) {
        world_unload_textures(world); // Unload textures before closing
        world_free(world);
    }
    if (sdf_shader.id != 0) {
        UnloadShader(sdf_shader);
    }

    // Shutdown console input system
    console_shutdown();

    CloseWindow();
    return 0;
}
