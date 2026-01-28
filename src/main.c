#include "raylib.h"
#include "world.h"
#include "vec_math.h"
#include "rendering.h"
#include "utils.h"
#include <math.h>
#include <stdio.h>

// graphics and player constants
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define TARGET_FPS 165
#define RENDER_DISTANCE 50.0f
#define FOG_START 30.0f
#define CULLING_FOV 110.0f
#define ASPECT_RATIO ((float)WINDOW_WIDTH / (float)WINDOW_HEIGHT)

int main(void)
{
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "b3dv 0.0.3");

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
            custom_font = LoadFontEx(font_paths[i], 32, NULL, 0);
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
    World* world = world_create();
    world_generate_prism(world);

    // create player at spawn position (high above the blocks)
    Player* player = player_create(8.0f, 15.0f, 8.0f);

    // enable mouse capture
    bool mouse_captured = true;
    DisableCursor();

    // HUD mode (0 = default, 1 = performance metrics 2 = player)
    int hud_mode = 0;

    // Pause state
    bool paused = false;

    SetTargetFPS(TARGET_FPS);

    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();

        // HUD mode toggle with F2/F3/F4
        if (IsKeyPressed(KEY_F2)) {
            hud_mode = 0;  // default HUD
        }
        if (IsKeyPressed(KEY_F3)) {
            hud_mode = 1;  // performance metrics
        }
        if (IsKeyPressed(KEY_F4)) {
            hud_mode = 2;  // player
        }

        // toggle pause with P key
        if (IsKeyPressed(KEY_P)) {
            paused = !paused;
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

        // get camera forward and right vectors
        Vector3 forward = vec3_normalize(vec3_sub(camera.target, camera.position));
        Vector3 right = vec3_cross(forward, camera.up);
        right = vec3_normalize(right);

        // handle player movement and update physics (only if not paused)
        if (!paused) {
            player_move_input(player, forward, right, dt);
            player_update(player, world, dt);
        }

        // update camera to follow player
        camera.position = (Vector3){
            player->position.x,
            player->position.y,
            player->position.z
        };
        camera.target = (Vector3){
            player->position.x + forward.x,
            player->position.y + forward.y,
            player->position.z + forward.z
        };

        // handle mouse look
        if (mouse_captured) {
            Vector2 mouse_delta = GetMouseDelta();
            float mouse_speed = 0.005f;

            // rotate forward vector around up axis (Y-axis rotation)
            float yaw = mouse_delta.x * mouse_speed;
            forward = vec3_rotate_y(forward, yaw);

            // rotate forward around right vector (pitch) - negate to fix vertical inversion
            float pitch = -mouse_delta.y * mouse_speed;
            float cos_p = cosf(pitch);
            float sin_p = sinf(pitch);
            forward = (Vector3){
                forward.x * cos_p + camera.up.x * sin_p,
                forward.y * cos_p + camera.up.y * sin_p,
                forward.z * cos_p + camera.up.z * sin_p
            };
            forward = vec3_normalize(forward);

            // clamp pitch to prevent gimbal lock (can't look more than 89.5 degrees up/down)
            float forward_y = forward.y;
            if (forward_y > 0.995f) forward_y = 0.995f;  // ~84 degrees down
            if (forward_y < -0.995f) forward_y = -0.995f;  // ~84 degrees up

            // reconstruct forward with clamped Y
            float horiz_dist = sqrtf(forward.x * forward.x + forward.z * forward.z);
            if (horiz_dist > 0.001f) {
                float scale = sqrtf(1.0f - forward_y * forward_y) / horiz_dist;
                forward.x *= scale;
                forward.z *= scale;
                forward.y = forward_y;
            }

            // recalculate right vector
            right = vec3_cross(forward, camera.up);
            right = vec3_normalize(right);

            // update camera target
            camera.target = vec3_add(camera.position, vec3_scale(forward, 5.0f));
        }

        BeginDrawing();
        ClearBackground(SKYBLUE);

        BeginMode3D(camera);
            Vector3 cam_forward = vec3_normalize(vec3_sub(camera.target, camera.position));
            Vector3 cam_right = vec3_cross(cam_forward, camera.up);
            cam_right = vec3_normalize(cam_right);

            int blocks_rendered = 0;

            // draw all blocks in the world with frustum culling and face culling
            for (int y = 0; y < WORLD_HEIGHT; y++) {
                for (int z = 0; z < WORLD_DEPTH; z++) {
                    for (int x = 0; x < WORLD_WIDTH; x++) {
                        BlockType block = world_get_block(world, x, y, z);
                        if (block != BLOCK_AIR) {
                            if (is_block_occluded(world, x, y, z)) {
                                continue;
                            }

                            Vector3 world_pos = (Vector3){
                                x + 0.5f + WORLD_OFFSET_X,
                                y + 0.5f,
                                z + 0.5f + WORLD_OFFSET_Z
                            };

                            // skip blocks with no visible faces toward camera
                            if (!has_visible_face(world, x, y, z, world_pos, camera.position)) {
                                continue;
                            }

                            // distance-based LOD: skip blocks at distance
                            Vector3 to_block = vec3_sub(world_pos, camera.position);
                            float dist = sqrtf(to_block.x*to_block.x + to_block.y*to_block.y + to_block.z*to_block.z);

                            // hard render distance limit
                            if (dist > RENDER_DISTANCE) {
                                continue;
                            }

                            if (is_block_visible(world_pos, camera.position, cam_forward, cam_right,
                                               camera.up, RENDER_DISTANCE, CULLING_FOV, ASPECT_RATIO)) {
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
                                draw_cube_faces(world_pos, 1.0f, color, camera.position, wire_color);
                                blocks_rendered++;
                            }
                        }
                    }
                }
            }
            DrawGrid(30, 1.0f);
        EndMode3D();

        // draw HUD based on mode
        if (hud_mode == 0) {
            // default HUD
            DrawTextEx(custom_font, "WASD to move, Space to jump", (Vector2){10, 10}, 32, 1, BLACK);
            DrawTextEx(custom_font, "F3 for performance metrics, F2 for this", (Vector2){10, 50}, 32, 1, BLACK);
            DrawTextEx(custom_font, "F7 to toggle mouse capture", (Vector2){10, 90}, 32, 1, BLACK);
            DrawTextEx(custom_font, "Mouse to look around", (Vector2){10, 130}, 32, 1, BLACK);

            char coord_text[64];
            snprintf(coord_text, sizeof(coord_text), "Pos: (%.1f, %.1f, %.1f)",
                     player->position.x, player->position.y, player->position.z);
            DrawTextEx(custom_font, coord_text, (Vector2){10, 170}, 32, 1, BLACK);

            char fps_text[32];
            snprintf(fps_text, sizeof(fps_text), "FPS: %d", GetFPS());
            DrawTextEx(custom_font, fps_text, (Vector2){10, 210}, 32, 1, BLACK);

            DrawTextEx(custom_font, "b3dv 0.0.3 - Jimena Neumann", (Vector2){10, 250}, 32, 1, DARKGRAY);
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

            DrawTextEx(custom_font, "b3dv 0.0.3 - Jimena Neumann", (Vector2){10, 250}, 32, 1, DARKGRAY);
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

            DrawTextEx(custom_font, "b3dv 0.0.3 - Jimena Neumann", (Vector2){10, 250}, 32, 1, DARKGRAY);
        }

        // display pause message if paused
        if (paused) {
            // get screen dimensions dynamically
            int screen_width = GetScreenWidth();
            int screen_height = GetScreenHeight();

            // measure text to center it
            Vector2 paused_size = MeasureTextEx(custom_font, "PAUSED", 64, 2);
            Vector2 resume_size = MeasureTextEx(custom_font, "Press P to resume", 32, 1);

            // draw centered on screen
            DrawTextEx(custom_font, "PAUSED",
                       (Vector2){(screen_width - paused_size.x) / 2, (screen_height - paused_size.y) / 2 - 40},
                       64, 2, RED);
            DrawTextEx(custom_font, "Press P to resume",
                       (Vector2){(screen_width - resume_size.x) / 2, (screen_height - resume_size.y) / 2 + 40},
                       32, 1, RED);
        }

        EndDrawing();
    }

    UnloadFont(custom_font);
    player_free(player);
    world_free(world);
    CloseWindow();
    return 0;
}
