#include "raylib.h"
#include "world.h"
#include "player.h"
#include "vec_math.h"
#include "rendering.h"
#include "utils.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// graphics and player constants
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define TARGET_FPS 260
#define RENDER_DISTANCE 50.0f
#define FOG_START 30.0f
#define CULLING_FOV 110.0f
#define ASPECT_RATIO ((float)WINDOW_WIDTH / (float)WINDOW_HEIGHT)

// Load chat history from file
void load_chat_history(char chat_history[50][256], int* history_count)
{
    FILE* file = fopen("./chathistory", "r");
    if (!file) return;  // File doesn't exist yet, that's fine

    *history_count = 0;
    char line[256];

    while (fgets(line, sizeof(line), file) != NULL && *history_count < 50) {
        // Remove trailing newline
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }

        // Copy line to history
        strncpy(chat_history[*history_count], line, 255);
        chat_history[*history_count][255] = '\0';
        (*history_count)++;
    }

    fclose(file);
}

// Raycast from camera to find the block being looked at
// Returns true if a block was hit, false otherwise
// out_block_x/y/z: the coordinates of the block hit
// out_adjacent_x/y/z: the coordinates where a new block would be placed (adjacent to hit block)
bool raycast_block(World* world, Camera3D camera, float max_distance,
                   int* out_block_x, int* out_block_y, int* out_block_z,
                   int* out_adjacent_x, int* out_adjacent_y, int* out_adjacent_z)
{
    Vector3 ray_origin = camera.position;
    Vector3 ray_dir = vec3_normalize(vec3_sub(camera.target, camera.position));

    float step = 0.05f;  // Smaller step size for accuracy
    float distance = 0.0f;

    Vector3 prev_pos = ray_origin;
    Vector3 prev_block_pos = (Vector3){floorf(ray_origin.x), floorf(ray_origin.y), floorf(ray_origin.z)};

    while (distance < max_distance) {
        Vector3 current_pos = vec3_add(ray_origin, vec3_scale(ray_dir, distance));

        // Use proper floor for negative coordinates
        int block_x = (int)floorf(current_pos.x);
        int block_y = (int)floorf(current_pos.y);
        int block_z = (int)floorf(current_pos.z);

        // Check if we're in a block
        BlockType block = world_get_block(world, block_x, block_y, block_z);
        if (block != BLOCK_AIR) {
            // We hit a block - the hit block is the current one
            *out_block_x = block_x;
            *out_block_y = block_y;
            *out_block_z = block_z;

            // Adjacent block is where we came from (previous block)
            int prev_block_x = (int)floorf(prev_pos.x);
            int prev_block_y = (int)floorf(prev_pos.y);
            int prev_block_z = (int)floorf(prev_pos.z);

            *out_adjacent_x = prev_block_x;
            *out_adjacent_y = prev_block_y;
            *out_adjacent_z = prev_block_z;

            return true;
        }

        prev_pos = current_pos;
        distance += step;
    }

    return false;  // No block hit
}

int main(void)
{
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "b3dv 0.0.5");

    // Disable default ESC key behavior (we handle it manually for pause menu)
    SetExitKey(KEY_NULL);

    // load custom font at larger size for better quality (with fallback)
    Font custom_font = {0};
    const char* font_paths[] = {
        // linux paths
        "/usr/share/fonts/TTF/JetBrainsMonoNerdFontMono-Regular.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        // windows paths (common locations)
        "C:\\Windows\\Fonts\\JetBrainsMono-Regular.ttf",
        "C:\\Windows\\Fonts\\consola.ttf",
        "C:\\Windows\\Fonts\\cour.ttf",
        NULL
    };

    for (int i = 0; font_paths[i] != NULL; i++) {
        if (FileExists(font_paths[i])) {
            custom_font = LoadFontEx(font_paths[i], 64, NULL, 0);
            break;
        }
    }

    // if no font found, use default raylib font
    if (custom_font.glyphCount == 0) {
        custom_font = GetFontDefault();
    }

    Camera3D camera = {
        .position = (Vector3){ 20, 15, 20 },
        .target = (Vector3){ 8, 4, 8 },
        .up = (Vector3){ 0, 1, 0 },
        .fovy = 90,
        .projection = CAMERA_PERSPECTIVE
    };

    // create and generate world
    world_system_init();  // Initialize world save system
    World* world = world_create();

    // Try to load default world, or generate new one
    if (!world_load(world, "default")) {
        world_generate_prism(world);
        world_save(world, "default");  // Save the generated world
    }

    // create player at spawn position (high above the blocks)
    Player* player = player_create(8.0f, 15.0f, 8.0f);

    // enable mouse capture
    bool mouse_captured = true;
    DisableCursor();

    // HUD mode (0 = default, 1 = performance metrics 2 = player, 3 = system info)
    int hud_mode = 0;
    int prev_hud_mode = -1;  // Track previous mode to detect changes
    char cached_cpu[128] = {0};
    char cached_gpu[128] = {0};
    char cached_kernel[128] = {0};

    // Pause state
    bool paused = false;
    bool should_quit = false;

    // Chat system
    bool chat_active = false;
    char chat_input[256] = {0};
    int chat_input_len = 0;
    int chat_cursor_pos = 0;  // cursor position in input string

    // Command history
    #define HISTORY_SIZE 50
    char chat_history[HISTORY_SIZE][256] = {0};
    int history_count = 0;
    int history_index = -1;  // -1 means not browsing history

    // Block highlighting for raycast
    int highlighted_block_x = 0;
    int highlighted_block_y = 0;
    int highlighted_block_z = 0;
    bool has_highlighted_block = false;

    // Camera angle tracking (in radians)
    float camera_yaw = 0.0f;    // horizontal rotation around Y axis
    float camera_pitch = 0.0f;  // vertical rotation (pitch)

    // Persistent camera basis vectors (to handle gimbal lock)
    Vector3 camera_right = {1, 0, 0};
    Vector3 camera_up = {0, 1, 0};
    // Persistent camera forward (actual look direction)
    Vector3 camera_forward = vec3_normalize(vec3_sub(camera.target, camera.position));

    // Initialize camera angles from initial forward direction
    Vector3 initial_forward = vec3_normalize(vec3_sub(camera.target, camera.position));
    camera_yaw = atan2f(initial_forward.x, initial_forward.z);
    camera_pitch = asinf(initial_forward.y);

    // Load chat history from file
    load_chat_history(chat_history, &history_count);

    SetTargetFPS(TARGET_FPS);

    while (!WindowShouldClose() && !should_quit)
    {
        float dt = GetFrameTime();

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

            // Handle up arrow (previous command)
            if (IsKeyPressed(KEY_UP)) {
                if (history_index < history_count - 1) {
                    history_index++;
                    strncpy(chat_input, chat_history[history_count - 1 - history_index], sizeof(chat_input) - 1);
                    chat_input[255] = '\0';
                    chat_input_len = strlen(chat_input);
                    chat_cursor_pos = chat_input_len;
                }
            }

            // Handle down arrow (next command)
            if (IsKeyPressed(KEY_DOWN)) {
                if (history_index > 0) {
                    history_index--;
                    strncpy(chat_input, chat_history[history_count - 1 - history_index], sizeof(chat_input) - 1);
                    chat_input[255] = '\0';
                    chat_input_len = strlen(chat_input);
                    chat_cursor_pos = chat_input_len;
                } else if (history_index == 0) {
                    history_index = -1;
                    chat_input[0] = '\0';
                    chat_input_len = 0;
                    chat_cursor_pos = 0;
                }
            }

            // Handle enter (submit command)
            if (IsKeyPressed(KEY_ENTER)) {
                // Save to history (only if not empty)
                if (chat_input_len > 0) {
                    if (history_count < HISTORY_SIZE) {
                        strncpy(chat_history[history_count], chat_input, 255);
                        chat_history[history_count][255] = '\0';
                        history_count++;
                    } else {
                        // Shift history down and add new command
                        for (int i = 0; i < HISTORY_SIZE - 1; i++) {
                            strncpy(chat_history[i], chat_history[i + 1], 255);
                            chat_history[i][255] = '\0';
                        }
                        strncpy(chat_history[HISTORY_SIZE - 1], chat_input, 255);
                        chat_history[HISTORY_SIZE - 1][255] = '\0';
                    }

                    // Log to chathistory file
                    FILE* log_file = fopen("./chathistory", "a");
                    if (log_file) {
                        fprintf(log_file, "%s\n", chat_input);
                        fclose(log_file);
                    }
                }

                chat_active = false;
                history_index = -1;
                chat_cursor_pos = 0;

                // Process commands
                if (chat_input[0] == '/') {
                    // Parse command
                    if (strncmp(chat_input, "/quit", 5) == 0) {
                        should_quit = true;
                    } else if (strncmp(chat_input, "/tp ", 4) == 0) {
                        // Parse teleport coordinates
                        float x, y, z;
                        if (sscanf(chat_input, "/tp %f %f %f", &x, &y, &z) == 3) {
                            player->position = (Vector3){ x, y, z };
                            player->velocity = (Vector3){ 0, 0, 0 };
                        }
                    } else if (strncmp(chat_input, "/save ", 6) == 0) {
                        // Save world: /save worldname
                        const char* world_name = chat_input + 6;
                        if (world_save(world, world_name)) {
                            printf("World '%s' saved successfully.\n", world_name);
                        } else {
                            printf("Failed to save world '%s'.\n", world_name);
                        }
                    } else if (strncmp(chat_input, "/load ", 6) == 0) {
                        // Load world: /load worldname
                        const char* world_name = chat_input + 6;
                        if (world_load(world, world_name)) {
                            printf("World '%s' loaded successfully.\n", world_name);
                        } else {
                            printf("Failed to load world '%s'.\n", world_name);
                        }
                    } else if (strncmp(chat_input, "/createworld ", 13) == 0) {
                        // Create new world: /createworld worldname
                        const char* world_name = chat_input + 13;

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
                            printf("Invalid world name. Use only alphanumeric characters and underscore.\n");
                        } else {
                            // Free current world and create new one
                            world_free(world);
                            world = world_create();
                            // Set the world name before generating
                            strncpy(world->world_name, world_name, sizeof(world->world_name) - 1);
                            world->world_name[sizeof(world->world_name) - 1] = '\0';

                            world_generate_prism(world);
                            // Save the newly created world
                            if (world_save(world, world_name)) {
                                printf("World '%s' created and saved successfully.\n", world_name);
                                // Reset player position to origin
                                player->position = (Vector3){ 0, 20, 0 };
                                player->velocity = (Vector3){ 0, 0, 0 };
                            } else {
                                printf("Failed to create world '%s'.\n", world_name);
                            }
                        }
                    } else if (strncmp(chat_input, "/loadworld ", 11) == 0) {
                        // Load existing world: /loadworld worldname
                        const char* world_name = chat_input + 11;

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
                            printf("Invalid world name. Use only alphanumeric characters and underscore.\n");
                        } else {
                            // Free current world and player, then load the saved one
                            world_free(world);
                            world = world_create();
                            // Recreate player at default position
                            player = player_create(8.0f, 15.0f, 8.0f);

                            if (world_load(world, world_name)) {
                                // Reset player position
                                player->position = (Vector3){ 0, 20, 0 };
                                player->velocity = (Vector3){ 0, 0, 0 };
                            } else {
                                printf("Failed to load world '%s'.\n", world_name);
                                world_generate_prism(world);  // Fall back to default world
                            }
                        }
                    } else if (strncmp(chat_input, "/setblock ", 10) == 0) {
                        // Set block: /setblock x y z <block>
                        // All coordinates treated as world-space (centered at 0,0,0)
                        float fx, fy, fz;
                        char block_name[32] = {0};

                        if (sscanf(chat_input, "/setblock %f %f %f %31s", &fx, &fy, &fz, block_name) == 4) {
                            // Convert world-space to block indices (infinite world, no offset needed)
                            int ix = (int)floorf(fx);
                            int iy = (int)floorf(fy);
                            int iz = (int)floorf(fz);

                            BlockType block_type = BLOCK_AIR;
                            const char* type_str = "air";

                            if (strcmp(block_name, "stone") == 0) {
                                block_type = BLOCK_STONE;
                                type_str = "stone";
                            } else if (strcmp(block_name, "air") == 0) {
                                block_type = BLOCK_AIR;
                                type_str = "air";
                            } else {
                                printf("Unknown block type: %s. Available: air, stone\n", block_name);
                                ix = -1;  // Skip application
                            }

                            // Apply if indices valid
                            // Check if coordinates are within valid bounds and place block
                            if (ix >= 0 && ix < 256 && iy >= 0 && iy < 256 && iz >= 0 && iz < 256) {
                                world_set_block(world, ix, iy, iz, block_type);
                                printf("Set block at world (%.1f, %.1f, %.1f) → index (%d, %d, %d) to %s.\n", fx, fy, fz, ix, iy, iz, type_str);
                            } else if (ix != -1) {
                                printf("Out of bounds: world (%.1f, %.1f, %.1f) → index (%d, %d, %d)\n", fx, fy, fz, ix, iy, iz);
                                printf("Valid range: x,z in [%.1f, %.1f], y in [0, %d]\n", -1000.0f, 1000.0f, 256);
                            }
                        } else {
                            printf("Usage: /setblock x y z <block>\n");
                            printf("Coordinates are world-space (player spawn is ~0, 0, 0)\n");
                            printf("Available blocks: air, stone\n");
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
                history_index = -1;
                chat_cursor_pos = 0;
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

        // toggle pause with P key or ESC
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

        // toggle mouse capture with F7
        if (IsKeyPressed(KEY_F7)) {
            mouse_captured = !mouse_captured;
            if (mouse_captured) {
                DisableCursor();
            } else {
                EnableCursor();
            }
        }

        // toggle fullscreen with F11
        if (IsKeyPressed(KEY_F11)) {
            ToggleFullscreen();
        }

        // take screenshot with F12
        if (IsKeyPressed(KEY_F12)) {
            char screenshot_file[64];
            get_screenshot_filename(screenshot_file, sizeof(screenshot_file));
            TakeScreenshot(screenshot_file);
        }

        // teleport with R key
        if (IsKeyPressed(KEY_R)) {
            player->position = (Vector3){ 8.0f, 15.0f, 8.0f };
            player->velocity = (Vector3){ 0, 0, 0 };
        }

        // Chat system - open with T key
        if (IsKeyPressed(KEY_T) && !paused) {
            chat_active = true;
            chat_input_len = 0;
            chat_input[0] = '\0';
            history_index = -1;
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

        // handle player movement and update physics (only if not paused or in chat)
        if (!paused && !chat_active) {
            player_move_input(player, forward, right, dt);
            player_update(player, world, dt);
            world_update_chunks(world, player->position);  // Load/unload chunks based on player position

            // Handle block breaking (left click)
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                int hit_x, hit_y, hit_z;
                int adj_x, adj_y, adj_z;
                if (raycast_block(world, camera, 10.0f, &hit_x, &hit_y, &hit_z, &adj_x, &adj_y, &adj_z)) {
                    world_set_block(world, hit_x, hit_y, hit_z, BLOCK_AIR);
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
                        world_set_block(world, adj_x, adj_y, adj_z, BLOCK_STONE);
                    }
                }
            }

            // Update highlighted block for every frame (raycast continuously)
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

        // update camera to follow player (position it at eye level, slightly above center)
        float eye_height = 0.1f;  // Slightly above the head center
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

        // Camera-relative rendering: shift camera near origin for float precision
        // Use a very large grid (131072 = 2^17) to minimize boundary crossings
        Vector3 original_camera_pos = camera.position;
        Vector3 camera_offset = (Vector3){
            floorf(camera.position.x / 131072.0f) * 131072.0f,
            0,
            floorf(camera.position.z / 131072.0f) * 131072.0f
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

        BeginMode3D(camera);
            // Use the actual camera orientation for rendering/frustum culling
            Vector3 cam_forward = camera_forward;
            Vector3 cam_right = camera_right;

            int blocks_rendered = 0;

            // draw all chunks and their blocks with frustum culling and face culling
            for (int c = 0; c < world->chunk_cache.chunk_count; c++) {
                Chunk* chunk = &world->chunk_cache.chunks[c];

                // Skip unloaded chunks
                if (!chunk->loaded) continue;

                // Skip chunks that haven't been generated yet
                if (!chunk->generated) continue;

                float chunk_center_x = chunk->chunk_x * CHUNK_WIDTH + CHUNK_WIDTH / 2.0f;
                float chunk_center_y = chunk->chunk_y * CHUNK_HEIGHT + CHUNK_HEIGHT / 2.0f;
                float chunk_center_z = chunk->chunk_z * CHUNK_DEPTH + CHUNK_DEPTH / 2.0f;

                float dx = chunk_center_x - camera.position.x;
                float dy = chunk_center_y - camera.position.y;
                float dz = chunk_center_z - camera.position.z;
                float chunk_dist_sq = dx*dx + dy*dy + dz*dz;

                // Skip chunks beyond render distance (add chunk size to account for chunk extent)
                float max_dist = RENDER_DISTANCE + CHUNK_WIDTH;
                if (chunk_dist_sq > max_dist * max_dist) {
                    continue;
                }

                // Render blocks in this chunk
                for (int y = 0; y < CHUNK_HEIGHT; y++) {
                    for (int z = 0; z < CHUNK_DEPTH; z++) {
                        for (int x = 0; x < CHUNK_WIDTH; x++) {
                            BlockType block = world_chunk_get_block(chunk, x, y, z);
                            if (block != BLOCK_AIR) {
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
                                float render_dist_sq = RENDER_DISTANCE * RENDER_DISTANCE;

                                // hard render distance limit
                                if (dist_sq > render_dist_sq) {
                                    continue;
                                }

                                // Check occlusion after distance (also relatively cheap)
                                if (is_block_occluded(world, world_x, world_y, world_z)) {
                                    continue;
                                }

                                // Only do expensive visibility checks after distance/occlusion pass
                                if (!is_block_visible(world_pos, shifted_cam_pos, cam_forward, cam_right,
                                                   camera.up, RENDER_DISTANCE, CULLING_FOV, ASPECT_RATIO)) {
                                    continue;
                                }

                                float dist = sqrtf(dist_sq);  // Only calc sqrt if we're actually rendering
                                Color color = world_get_block_color(block);

                                // apply fog effect: fade color towards sky blue based on distance
                                float fog_factor = 0.0f;
                                if (dist > FOG_START) {
                                    fog_factor = (dist - FOG_START) / (RENDER_DISTANCE - FOG_START);
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
                                draw_cube_faces(world_pos, 1.0f, color, camera.position, wire_color, world, world_x, world_y, world_z);
                                blocks_rendered++;
                            }
                        }
                    }
                }
            }

            // Draw highlighting box around the block being looked at
            if (has_highlighted_block) {
                Vector3 block_pos = (Vector3){
                    highlighted_block_x + 0.5f,
                    highlighted_block_y + 0.5f,
                    highlighted_block_z + 0.5f
                };
                DrawCubeWires(block_pos, 1.02f, 1.02f, 1.02f, YELLOW);
            }
            DrawGrid(30, 1.0f);
        EndMode3D();

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

        // draw HUD based on mode
        if (hud_mode == 0) {
            // default HUD
            DrawTextEx(custom_font, "WASD to move, Space to jump", (Vector2){10, 10}, 32, 1, BLACK);
            DrawTextEx(custom_font, "F3 for performance metrics, F2 for this", (Vector2){10, 50}, 32, 1, BLACK);
            DrawTextEx(custom_font, "F7 to toggle mouse capture", (Vector2){10, 90}, 32, 1, BLACK);
            DrawTextEx(custom_font, "Mouse to look around", (Vector2){10, 130}, 32, 1, BLACK);
            DrawTextEx(custom_font, "ESC or P to pause", (Vector2){10, 170}, 32, 1, BLACK);

            char coord_text[64];
            snprintf(coord_text, sizeof(coord_text), "Pos: (%.1f, %.1f, %.1f)",
                     player->position.x, player->position.y, player->position.z);
            DrawTextEx(custom_font, coord_text, (Vector2){10, 210}, 32, 1, BLACK);

            char fps_text[32];
            snprintf(fps_text, sizeof(fps_text), "FPS: %d", GetFPS());
            DrawTextEx(custom_font, fps_text, (Vector2){10, 250}, 32, 1, BLACK);

            DrawTextEx(custom_font, "b3dv 0.0.5 - Jimena Neumann", (Vector2){10, 290}, 32, 1, DARKGRAY);
        } else if (hud_mode == 1) {
            // performance metrics HUD
            DrawTextEx(custom_font, "=== PERFORMANCE METRICS ===", (Vector2){10, 10}, 32, 1, BLACK);

            char frame_time[64];
            snprintf(frame_time, sizeof(frame_time), "Frame Time: %.2f ms", dt * 1000.0f);
            DrawTextEx(custom_font, frame_time, (Vector2){10, 50}, 32, 1, BLACK);

            char fps_text[32];
            snprintf(fps_text, sizeof(fps_text), "FPS: %d", GetFPS());
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

            DrawTextEx(custom_font, "b3dv 0.0.5 - Jimena Neumann", (Vector2){10, 250}, 32, 1, DARKGRAY);
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
            float speed = sqrtf(player->velocity.x * player->velocity.x +
                               player->velocity.y * player->velocity.y +
                               player->velocity.z * player->velocity.z);
            char speed_text[64];
            snprintf(speed_text, sizeof(speed_text), "Speed: %.2f m/s", speed);
            DrawTextEx(custom_font, speed_text, (Vector2){10, 130}, 32, 1, BLACK);

            // momentum/velocity components
            char momentum_text[96];
            snprintf(momentum_text, sizeof(momentum_text), "Vel: (%.2f, %.2f, %.2f) m/s",
                     player->velocity.x, player->velocity.y, player->velocity.z);
            DrawTextEx(custom_font, momentum_text, (Vector2){10, 170}, 32, 1, BLACK);

            DrawTextEx(custom_font, "b3dv 0.0.5 - Jimena Neumann", (Vector2){10, 250}, 32, 1, DARKGRAY);
        } else if (hud_mode == 3) {
            // system info HUD (using cached values)
            DrawTextEx(custom_font, "=== SYSTEM INFO ===", (Vector2){10, 10}, 32, 1, BLACK);
            DrawTextEx(custom_font, cached_cpu, (Vector2){10, 50}, 32, 1, BLACK);
            DrawTextEx(custom_font, cached_gpu, (Vector2){10, 90}, 32, 1, BLACK);
            DrawTextEx(custom_font, cached_kernel, (Vector2){10, 130}, 32, 1, BLACK);
            DrawTextEx(custom_font, "b3dv 0.0.5 - Jimena Neumann", (Vector2){10, 250}, 32, 1, DARKGRAY);
        }

        // display pause menu with buttons if paused
        if (paused) {
            // get screen dimensions dynamically
            int screen_width = GetScreenWidth();
            int screen_height = GetScreenHeight();

            // draw semi-transparent overlay
            DrawRectangle(0, 0, screen_width, screen_height, (Color){0, 0, 0, 150});

            // measure text to center it
            Vector2 paused_size = MeasureTextEx(custom_font, "PAUSED", 64, 2);

            // draw title
            DrawTextEx(custom_font, "PAUSED",
                       (Vector2){(screen_width - paused_size.x) / 2, screen_height / 2 - 120},
                       64, 2, RED);

            // button dimensions
            int button_width = 200;
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

            // quit button
            Rectangle quit_button = {
                center_x - button_width / 2,
                center_y + button_height + button_spacing,
                button_width,
                button_height
            };

            // get mouse position
            Vector2 mouse_pos = GetMousePosition();
            bool resume_hover = CheckCollisionPointRec(mouse_pos, resume_button);
            bool quit_hover = CheckCollisionPointRec(mouse_pos, quit_button);

            // draw resume button
            DrawRectangleRec(resume_button, resume_hover ? LIGHTGRAY : GRAY);
            DrawRectangleLinesEx(resume_button, 2, WHITE);
            Vector2 resume_text_size = MeasureTextEx(custom_font, "Resume", 32, 1);
            DrawTextEx(custom_font, "Resume",
                       (Vector2){center_x - resume_text_size.x / 2, center_y + 12},
                       32, 1, BLACK);

            // draw quit button
            DrawRectangleRec(quit_button, quit_hover ? LIGHTGRAY : GRAY);
            DrawRectangleLinesEx(quit_button, 2, WHITE);
            Vector2 quit_text_size = MeasureTextEx(custom_font, "Quit", 32, 1);
            DrawTextEx(custom_font, "Quit",
                       (Vector2){center_x - quit_text_size.x / 2, center_y + button_height + button_spacing + 12},
                       32, 1, BLACK);

            // handle button clicks
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                if (resume_hover) {
                    paused = false;
                    // Recapture mouse if it was captured before pause
                    mouse_captured = true;
                    DisableCursor();
                } else if (quit_hover) {
                    should_quit = true;
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

    // Save world before closing
    world_save(world, "default");

    UnloadFont(custom_font);
    player_free(player);
    world_free(world);
    CloseWindow();
    return 0;
}
