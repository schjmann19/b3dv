#include "menu.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

// Helper function to check if a path is a directory (cross-platform)
static int is_directory(const char* path)
{
    struct stat statbuf;
    if (stat(path, &statbuf) != 0) {
        return 0;
    }
    #ifdef _WIN32
        return (statbuf.st_mode & _S_IFDIR) != 0;
    #else
        return S_ISDIR(statbuf.st_mode);
    #endif
}

// Scan available language directories in assets/text/
static void menu_scan_languages(MenuSystem* menu)
{
    DIR* dir = opendir("./assets/text");
    if (!dir) return;

    menu->available_languages_count = 0;
    struct dirent* entry;

    while ((entry = readdir(dir)) && menu->available_languages_count < 16) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char full_path[512];
        snprintf(full_path, sizeof(full_path), "./assets/text/%s", entry->d_name);

        if (is_directory(full_path)) {
            strcpy(menu->available_languages[menu->available_languages_count], entry->d_name);
            menu->available_languages_count++;
        }
    }
    closedir(dir);

    // Set current language index (find "en" or default to 0)
    menu->current_language_index = 0;
    for (int i = 0; i < menu->available_languages_count; i++) {
        if (strcmp(menu->available_languages[i], "en") == 0) {
            menu->current_language_index = i;
            break;
        }
    }
}

// Load text from a file in assets/text/<language>/<filename>
bool menu_load_text_file(const char* language, const char* filename, char* out_buffer, int buffer_size)
{
    char path[512];
    snprintf(path, sizeof(path), "./assets/text/%s/%s", language, filename);

    FILE* file = fopen(path, "r");
    if (!file) {
        fprintf(stderr, "Failed to load: %s\n", path);
        return false;
    }

    // Read file into buffer
    size_t bytes_read = fread(out_buffer, 1, buffer_size - 1, file);
    out_buffer[bytes_read] = '\0';

    fclose(file);
    return true;
}

// Load all text for a given language
void menu_load_language(MenuSystem* menu, const char* language)
{
    strcpy(menu->current_language, language);

    // Load menu labels from individual files or single file
    char menu_path[512];
    snprintf(menu_path, sizeof(menu_path), "./assets/text/%s/menu.txt", language);

    FILE* file = fopen(menu_path, "r");
    if (file) {
        char line[512];
        int line_count = 0;
        while (fgets(line, sizeof(line), file) && line_count < 35) {
            // Remove newline
            line[strcspn(line, "\n")] = '\0';

            char* buffers[] = {
                menu->text_select_world,
                menu->text_create_world,
                menu->text_credits_info,
                menu->text_quit,
                menu->text_back,
                menu->text_world_name_label,
                menu->text_create_btn,
                menu->text_cancel_btn,
                menu->text_error_empty_name,
                menu->text_error_exists,
                menu->text_no_worlds,
                menu->text_title_create_world,
                menu->text_title_select_world,
                menu->text_last,
                menu->game_text.move_controls,
                menu->game_text.metrics_help,
                menu->game_text.mouse_help,
                menu->game_text.look_help,
                menu->game_text.pause_help,
                menu->game_text.paused,
                menu->game_text.resume,
                menu->game_text.back_to_menu,
                menu->game_text.perf_metrics,
                menu->game_text.system_info,
                menu->game_text.player_info,
                menu->game_text.fps_label,
                menu->game_text.coord_label,
                menu->game_text.version,
                menu->game_text.settings,
                menu->game_text.render_dist_label,
                menu->game_text.max_fps_label,
                menu->game_text.font_family_label,
                menu->game_text.font_variant_label,
                menu->game_text.uncapped,
                menu->game_text.press_esc_to_return,
                menu->game_text.msg_quitting,
                menu->game_text.msg_teleported,
                menu->game_text.msg_teleport_usage,
                menu->game_text.msg_world_saved,
                menu->game_text.msg_world_save_failed,
                menu->game_text.msg_world_loaded,
                menu->game_text.msg_world_load_failed,
                menu->game_text.msg_invalid_world_name,
                menu->game_text.msg_block_selected,
                menu->game_text.msg_unknown_block,
                menu->game_text.msg_flight_enabled,
                menu->game_text.msg_flight_disabled,
                menu->game_text.msg_fly_usage,
                menu->game_text.msg_block_set,
                menu->game_text.msg_out_of_bounds,
                menu->game_text.msg_setblock_usage,
                menu->game_text.msg_unknown_command
            };
            int sizes[] = {
                sizeof(menu->text_select_world),
                sizeof(menu->text_create_world),
                sizeof(menu->text_credits_info),
                sizeof(menu->text_quit),
                sizeof(menu->text_back),
                sizeof(menu->text_world_name_label),
                sizeof(menu->text_create_btn),
                sizeof(menu->text_cancel_btn),
                sizeof(menu->text_error_empty_name),
                sizeof(menu->text_error_exists),
                sizeof(menu->text_no_worlds),
                sizeof(menu->text_title_create_world),
                sizeof(menu->text_title_select_world),
                sizeof(menu->text_last),
                sizeof(menu->game_text.move_controls),
                sizeof(menu->game_text.metrics_help),
                sizeof(menu->game_text.mouse_help),
                sizeof(menu->game_text.look_help),
                sizeof(menu->game_text.pause_help),
                sizeof(menu->game_text.paused),
                sizeof(menu->game_text.resume),
                sizeof(menu->game_text.back_to_menu),
                sizeof(menu->game_text.perf_metrics),
                sizeof(menu->game_text.system_info),
                sizeof(menu->game_text.player_info),
                sizeof(menu->game_text.fps_label),
                sizeof(menu->game_text.coord_label),
                sizeof(menu->game_text.version),
                sizeof(menu->game_text.settings),
                sizeof(menu->game_text.render_dist_label),
                sizeof(menu->game_text.max_fps_label),
                sizeof(menu->game_text.font_family_label),
                sizeof(menu->game_text.font_variant_label),
                sizeof(menu->game_text.uncapped),
                sizeof(menu->game_text.press_esc_to_return),
                sizeof(menu->game_text.msg_quitting),
                sizeof(menu->game_text.msg_teleported),
                sizeof(menu->game_text.msg_teleport_usage),
                sizeof(menu->game_text.msg_world_saved),
                sizeof(menu->game_text.msg_world_save_failed),
                sizeof(menu->game_text.msg_world_loaded),
                sizeof(menu->game_text.msg_world_load_failed),
                sizeof(menu->game_text.msg_invalid_world_name),
                sizeof(menu->game_text.msg_block_selected),
                sizeof(menu->game_text.msg_unknown_block),
                sizeof(menu->game_text.msg_flight_enabled),
                sizeof(menu->game_text.msg_flight_disabled),
                sizeof(menu->game_text.msg_fly_usage),
                sizeof(menu->game_text.msg_block_set),
                sizeof(menu->game_text.msg_out_of_bounds),
                sizeof(menu->game_text.msg_setblock_usage),
                sizeof(menu->game_text.msg_unknown_command)
            };

            strncpy(buffers[line_count], line, sizes[line_count] - 1);
            buffers[line_count][sizes[line_count] - 1] = '\0';
            line_count++;
        }
        fclose(file);
    } else {
        fprintf(stderr, "Failed to load menu text from: %s\n", menu_path);
    }

    // Load chat messages from chat.txt
    char chat_path[512];
    snprintf(chat_path, sizeof(chat_path), "./assets/text/%s/chat.txt", language);

    FILE* chat_file = fopen(chat_path, "r");
    if (chat_file) {
        char line[512];
        int chat_line_count = 0;

        char* chat_buffers[] = {
            menu->game_text.msg_quitting,
            menu->game_text.msg_teleported,
            menu->game_text.msg_teleport_usage,
            menu->game_text.msg_world_saved,
            menu->game_text.msg_world_save_failed,
            menu->game_text.msg_world_loaded,
            menu->game_text.msg_world_load_failed,
            menu->game_text.msg_invalid_world_name,
            menu->game_text.msg_block_selected,
            menu->game_text.msg_unknown_block,
            menu->game_text.msg_flight_enabled,
            menu->game_text.msg_flight_disabled,
            menu->game_text.msg_fly_usage,
            menu->game_text.msg_noclip_enabled,
            menu->game_text.msg_noclip_disabled,
            menu->game_text.msg_noclip_usage,
            menu->game_text.msg_block_set,
            menu->game_text.msg_out_of_bounds,
            menu->game_text.msg_setblock_usage,
            menu->game_text.msg_unknown_command
        };

        int chat_sizes[] = {
            sizeof(menu->game_text.msg_quitting),
            sizeof(menu->game_text.msg_teleported),
            sizeof(menu->game_text.msg_teleport_usage),
            sizeof(menu->game_text.msg_world_saved),
            sizeof(menu->game_text.msg_world_save_failed),
            sizeof(menu->game_text.msg_world_loaded),
            sizeof(menu->game_text.msg_world_load_failed),
            sizeof(menu->game_text.msg_invalid_world_name),
            sizeof(menu->game_text.msg_block_selected),
            sizeof(menu->game_text.msg_unknown_block),
            sizeof(menu->game_text.msg_flight_enabled),
            sizeof(menu->game_text.msg_flight_disabled),
            sizeof(menu->game_text.msg_fly_usage),
            sizeof(menu->game_text.msg_noclip_enabled),
            sizeof(menu->game_text.msg_noclip_disabled),
            sizeof(menu->game_text.msg_noclip_usage),
            sizeof(menu->game_text.msg_block_set),
            sizeof(menu->game_text.msg_out_of_bounds),
            sizeof(menu->game_text.msg_setblock_usage),
            sizeof(menu->game_text.msg_unknown_command)
        };

        while (fgets(line, sizeof(line), chat_file) && chat_line_count < 20) {
            // Remove newline
            line[strcspn(line, "\n")] = '\0';
            strncpy(chat_buffers[chat_line_count], line, chat_sizes[chat_line_count] - 1);
            chat_buffers[chat_line_count][chat_sizes[chat_line_count] - 1] = '\0';
            chat_line_count++;
        }
        fclose(chat_file);
    } else {
        fprintf(stderr, "Failed to load chat text from: %s\n", chat_path);
    }

    // Load credits text from credits.txt
    char credits_path[512];
    snprintf(credits_path, sizeof(credits_path), "./assets/text/%s/credits.txt", language);

    FILE* credits_file = fopen(credits_path, "r");
    if (credits_file) {
        size_t bytes_read = fread(menu->credits_text, 1, sizeof(menu->credits_text) - 1, credits_file);
        menu->credits_text[bytes_read] = '\0';
        fclose(credits_file);
    } else {
        fprintf(stderr, "Failed to load credits text from: %s\n", credits_path);
        strcpy(menu->credits_text, "Credits data not available.");
    }
}

// Scan available fonts from assets/fonts/<font-name>/ttf/
void menu_scan_fonts(MenuSystem* menu)
{
    DIR* dir = opendir("./assets/fonts");
    if (!dir) {
        // Fallback to default font if directory doesn't exist
        strcpy(menu->font_families[0], "JetBrainsMono");
        menu->font_families_count = 1;
        menu->current_font_family_index = 0;
        return;
    }

    menu->font_families_count = 0;
    struct dirent* entry;

    while ((entry = readdir(dir)) && menu->font_families_count < 16) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char full_path[512];
        snprintf(full_path, sizeof(full_path), "./assets/fonts/%s", entry->d_name);

        // Check if it's a directory and has a ttf subdirectory
        if (is_directory(full_path)) {
            char ttf_path[512];
            snprintf(ttf_path, sizeof(ttf_path), "%s/ttf", full_path);
            if (is_directory(ttf_path)) {
                strcpy(menu->font_families[menu->font_families_count], entry->d_name);
                menu->font_families_count++;
            }
        }
    }
    closedir(dir);

    // Set current family to first available (prefer JetBrainsMono)
    menu->current_font_family_index = 0;
    for (int i = 0; i < menu->font_families_count; i++) {
        if (strcmp(menu->font_families[i], "JetBrainsMono") == 0) {
            menu->current_font_family_index = i;
            break;
        }
    }

    if (menu->font_families_count == 0) {
        // Fallback if no fonts found
        strcpy(menu->font_families[0], "JetBrainsMono");
        menu->font_families_count = 1;
        menu->current_font_family_index = 0;
    }
}

// Scan font variants (individual .ttf files) for a given font family
void menu_scan_font_variants(MenuSystem* menu, const char* font_family)
{
    char ttf_dir[512];
    snprintf(ttf_dir, sizeof(ttf_dir), "./assets/fonts/%s/ttf", font_family);

    DIR* dir = opendir(ttf_dir);
    menu->font_variants_count = 0;
    menu->current_font_variant_index = 0;

    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) && menu->font_variants_count < 32) {
            // Look for .ttf files
            char* dot = strrchr(entry->d_name, '.');
            if (dot && strcmp(dot, ".ttf") == 0) {
                strcpy(menu->font_variants[menu->font_variants_count], entry->d_name);
                // Prefer Regular variant if available
                if (strstr(entry->d_name, "Regular") != NULL) {
                    menu->current_font_variant_index = menu->font_variants_count;
                }
                menu->font_variants_count++;
            }
        }
        closedir(dir);
    }

    // If no variants found, add a placeholder
    if (menu->font_variants_count == 0) {
        strcpy(menu->font_variants[0], "Regular");
        menu->font_variants_count = 1;
        menu->current_font_variant_index = 0;
    }
}

void menu_load_settings(MenuSystem* menu)
{
    FILE* file = fopen("./options.txt", "r");
    if (!file) {
        // File doesn't exist, use defaults (already set in menu_system_create)
        return;
    }

    char language[32] = {0};
    char font_family[256] = {0};
    char font_variant[256] = {0};

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        // Remove newline
        line[strcspn(line, "\n")] = '\0';

        // Skip empty lines and comments
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }

        // Parse key=value format
        char* equals = strchr(line, '=');
        if (!equals) {
            continue;
        }

        *equals = '\0';
        char* key = line;
        char* value = equals + 1;

        if (strcmp(key, "render_distance") == 0) {
            menu->render_distance = atof(value);
            // Clamp to valid range
            if (menu->render_distance < 10.0f) menu->render_distance = 10.0f;
            if (menu->render_distance > 100.0f) menu->render_distance = 100.0f;
        } else if (strcmp(key, "max_fps") == 0) {
            menu->max_fps = atoi(value);
            // Clamp to valid range (0 = uncapped, 30-240 = capped)
            if (menu->max_fps != 0 && menu->max_fps < 30) menu->max_fps = 30;
            if (menu->max_fps > 240) menu->max_fps = 240;
        } else if (strcmp(key, "language") == 0) {
            strncpy(language, value, sizeof(language) - 1);
            language[sizeof(language) - 1] = '\0';
        } else if (strcmp(key, "font_family") == 0) {
            strncpy(font_family, value, sizeof(font_family) - 1);
            font_family[sizeof(font_family) - 1] = '\0';
        } else if (strcmp(key, "font_variant") == 0) {
            strncpy(font_variant, value, sizeof(font_variant) - 1);
            font_variant[sizeof(font_variant) - 1] = '\0';
        }
    }

    fclose(file);

    // Now apply the loaded settings
    // Load language if saved
    if (language[0] != '\0') {
        // Find language index
        for (int i = 0; i < menu->available_languages_count; i++) {
            if (strcmp(menu->available_languages[i], language) == 0) {
                menu->current_language_index = i;
                menu_load_language(menu, language);
                break;
            }
        }
    }

    // Load font family if saved
    if (font_family[0] != '\0') {
        // Find font family index
        for (int i = 0; i < menu->font_families_count; i++) {
            if (strcmp(menu->font_families[i], font_family) == 0) {
                menu->current_font_family_index = i;
                // Scan variants for this family
                menu_scan_font_variants(menu, font_family);
                break;
            }
        }

        // Load font variant if saved
        if (font_variant[0] != '\0') {
            for (int i = 0; i < menu->font_variants_count; i++) {
                if (strcmp(menu->font_variants[i], font_variant) == 0) {
                    menu->current_font_variant_index = i;
                    break;
                }
            }
        }
    }
}

void menu_save_settings(MenuSystem* menu)
{
    FILE* file = fopen("./options.txt", "w");
    if (!file) {
        fprintf(stderr, "Failed to save options.txt\n");
        return;
    }

    fprintf(file, "# B3DV Game Settings\n");
    fprintf(file, "render_distance=%.1f\n", menu->render_distance);
    fprintf(file, "max_fps=%d\n", menu->max_fps);
    fprintf(file, "language=%s\n", menu->current_language);
    fprintf(file, "font_family=%s\n", menu->font_families[menu->current_font_family_index]);
    fprintf(file, "font_variant=%s\n", menu->font_variants[menu->current_font_variant_index]);

    fclose(file);
}

MenuSystem* menu_system_create(void)
{
    MenuSystem* menu = (MenuSystem*)malloc(sizeof(MenuSystem));
    if (!menu) return NULL;

    menu->current_state = MENU_STATE_MAIN;
    menu->previous_state = MENU_STATE_MAIN;
    menu->available_worlds = NULL;
    menu->world_count = 0;
    menu->selected_world_index = 0;
    menu->should_start_game = false;
    strcpy(menu->selected_world_name, "");
    strcpy(menu->new_world_name, "");
    menu->new_world_name_len = 0;
    menu->create_world_error = false;
    strcpy(menu->create_world_error_msg, "");

    // Initialize settings with defaults
    menu->render_distance = 50.0f;
    menu->max_fps = 144;

    // Load background image
    menu->background_loaded = false;
    if (FileExists("./assets/MainMenuBackground.png")) {
        menu->background_texture = LoadTexture("./assets/MainMenuBackground.png");
        menu->background_loaded = true;
    }

    // Scan for available languages
    menu_scan_languages(menu);

    // Load default language (English)
    if (menu->available_languages_count > 0) {
        menu_load_language(menu, menu->available_languages[menu->current_language_index]);
    }

    // Scan for available fonts
    menu_scan_fonts(menu);

    // Scan variants for the current font family
    menu_scan_font_variants(menu, menu->font_families[menu->current_font_family_index]);

    // Load persisted settings from options.txt (if it exists)
    // This must be done after languages and fonts are scanned
    menu_load_settings(menu);

    // Scan for available worlds
    menu_scan_worlds(menu);

    // Create/save options.txt file if it doesn't exist
    menu_save_settings(menu);

    return menu;
}

void menu_system_free(MenuSystem* menu)
{
    if (!menu) return;
    if (menu->available_worlds) {
        free(menu->available_worlds);
    }
    if (menu->background_loaded) {
        UnloadTexture(menu->background_texture);
    }
    free(menu);
}

void menu_scan_worlds(MenuSystem* menu)
{
    // Free previous list
    if (menu->available_worlds) {
        free(menu->available_worlds);
        menu->available_worlds = NULL;
    }
    menu->world_count = 0;

    // Scan worlds directory
    DIR* dir = opendir("./worlds");
    if (!dir) {
        return;
    }

    // Count directories first
    int count = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] != '.') {
            char full_path[512];
            snprintf(full_path, sizeof(full_path), "./worlds/%s", entry->d_name);
            if (is_directory(full_path)) {
                count++;
            }
        }
    }

    if (count == 0) {
        closedir(dir);
        return;
    }

    // Allocate memory for worlds
    menu->available_worlds = (WorldInfo*)malloc(sizeof(WorldInfo) * count);
    if (!menu->available_worlds) {
        closedir(dir);
        return;
    }

    // Populate world list
    rewinddir(dir);
    int index = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] != '.') {
            char full_path[512];
            snprintf(full_path, sizeof(full_path), "./worlds/%s", entry->d_name);
            if (is_directory(full_path)) {
                strncpy(menu->available_worlds[index].name, entry->d_name, 255);
                menu->available_worlds[index].name[255] = '\0';

                // Read metadata
                char metadata_path[512];
                snprintf(metadata_path, sizeof(metadata_path), "./worlds/%s/world.txt", entry->d_name);

                strcpy(menu->available_worlds[index].created, "Unknown");
                menu->available_worlds[index].chunk_count = 0;

                FILE* metadata_file = fopen(metadata_path, "r");
                if (metadata_file) {
                    char line[256];
                    while (fgets(line, sizeof(line), metadata_file)) {
                        if (strncmp(line, "last_saved=", 11) == 0) {
                            char* value = line + 11;
                            int len = strlen(value);
                            if (value[len-1] == '\n') len--;
                            strncpy(menu->available_worlds[index].created, value, len);
                            menu->available_worlds[index].created[len] = '\0';
                        } else if (strncmp(line, "chunk_count=", 12) == 0) {
                            menu->available_worlds[index].chunk_count = atoi(line + 12);
                        }
                    }
                    fclose(metadata_file);
                }

                index++;
            }
        }
    }
    menu->world_count = count;

    closedir(dir);

    // Reset selection
    menu->selected_world_index = 0;
}

void menu_draw_main(MenuSystem* menu, Font font)
{
    int screen_width = GetScreenWidth();
    int screen_height = GetScreenHeight();

    // Draw background
    if (menu->background_loaded) {
        // Draw the background image scaled to fit the screen
        DrawTexturePro(menu->background_texture,
                       (Rectangle){0, 0, menu->background_texture.width, menu->background_texture.height},
                       (Rectangle){0, 0, screen_width, screen_height},
                       (Vector2){0, 0},
                       0,
                       WHITE);
    } else {
        // Fallback to solid color if image not loaded
        ClearBackground((Color){20, 20, 20, 255});
    }

    // Draw title
    const char* title = "B3DV";
    Vector2 title_size = MeasureTextEx(font, title, 80, 2);
    DrawTextEx(font, title,
               (Vector2){(screen_width - title_size.x) / 2, 60},
               80, 2, WHITE);

    // Draw version
    const char* version = "Basic 3D Visualizer - v0.0.10";
    Vector2 version_size = MeasureTextEx(font, version, 24, 1);
    DrawTextEx(font, version,
               (Vector2){(screen_width - version_size.x) / 2, 150},
               24, 1, GRAY);

    // Button dimensions
    int button_width = 400;
    int button_height = 60;
    int button_spacing = 20;
    int center_x = screen_width / 2;
    int center_y = screen_height / 2;

    // Select World button
    Rectangle world_button = {
        center_x - button_width / 2,
        center_y,
        button_width,
        button_height
    };

    // Create World button
    Rectangle create_button = {
        center_x - button_width / 2,
        center_y + button_height + button_spacing,
        button_width,
        button_height
    };

    // Credits & Info button
    Rectangle credits_button = {
        center_x - button_width / 2,
        center_y + 2 * (button_height + button_spacing),
        button_width,
        button_height
    };

    // Settings button
    Rectangle settings_button = {
        center_x - button_width / 2,
        center_y + 3 * (button_height + button_spacing),
        button_width,
        button_height
    };

    // Quit button
    Rectangle quit_button = {
        center_x - button_width / 2,
        center_y + 4 * (button_height + button_spacing),
        button_width,
        button_height
    };

    // Get mouse position
    Vector2 mouse_pos = GetMousePosition();
    bool world_hover = CheckCollisionPointRec(mouse_pos, world_button);
    bool create_hover = CheckCollisionPointRec(mouse_pos, create_button);
    bool credits_hover = CheckCollisionPointRec(mouse_pos, credits_button);
    bool settings_hover = CheckCollisionPointRec(mouse_pos, settings_button);
    bool quit_hover = CheckCollisionPointRec(mouse_pos, quit_button);

    // Draw Select World button
    DrawRectangleRec(world_button, world_hover ? LIGHTGRAY : (Color){60, 60, 60, 255});
    DrawRectangleLinesEx(world_button, 2, WHITE);
    Vector2 world_text_size = MeasureTextEx(font, menu->text_select_world, 32, 1);
    DrawTextEx(font, menu->text_select_world,
               (Vector2){center_x - world_text_size.x / 2, center_y + 14},
               32, 1, BLACK);

    // Draw Create World button
    DrawRectangleRec(create_button, create_hover ? LIGHTGRAY : (Color){60, 60, 60, 255});
    DrawRectangleLinesEx(create_button, 2, WHITE);
    Vector2 create_text_size = MeasureTextEx(font, menu->text_create_world, 32, 1);
    DrawTextEx(font, menu->text_create_world,
               (Vector2){center_x - create_text_size.x / 2, center_y + button_height + button_spacing + 14},
               32, 1, BLACK);

    // Draw Credits & Info button
    DrawRectangleRec(credits_button, credits_hover ? LIGHTGRAY : (Color){60, 60, 60, 255});
    DrawRectangleLinesEx(credits_button, 2, WHITE);
    Vector2 credits_text_size = MeasureTextEx(font, menu->text_credits_info, 32, 1);
    DrawTextEx(font, menu->text_credits_info,
               (Vector2){center_x - credits_text_size.x / 2, center_y + 2 * (button_height + button_spacing) + 14},
               32, 1, BLACK);

    // Draw Settings button
    DrawRectangleRec(settings_button, settings_hover ? LIGHTGRAY : (Color){60, 60, 60, 255});
    DrawRectangleLinesEx(settings_button, 2, WHITE);
    Vector2 settings_text_size = MeasureTextEx(font, menu->game_text.settings, 32, 1);
    DrawTextEx(font, menu->game_text.settings,
               (Vector2){center_x - settings_text_size.x / 2, center_y + 3 * (button_height + button_spacing) + 14},
               32, 1, BLACK);

    // Draw Quit button
    DrawRectangleRec(quit_button, quit_hover ? LIGHTGRAY : (Color){60, 60, 60, 255});
    DrawRectangleLinesEx(quit_button, 2, WHITE);
    Vector2 quit_text_size = MeasureTextEx(font, menu->text_quit, 32, 1);
    DrawTextEx(font, menu->text_quit,
               (Vector2){center_x - quit_text_size.x / 2, center_y + 4 * (button_height + button_spacing) + 14},
               32, 1, BLACK);

    // Handle button clicks
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (world_hover) {
            menu_scan_worlds(menu);  // Refresh world list
            menu->current_state = MENU_STATE_WORLD_SELECT;
        } else if (create_hover) {
            menu->current_state = MENU_STATE_CREATE_WORLD;
            strcpy(menu->new_world_name, "");
            menu->new_world_name_len = 0;
            menu->create_world_error = false;
        } else if (credits_hover) {
            menu->current_state = MENU_STATE_CREDITS;
        } else if (settings_hover) {
            menu->current_state = MENU_STATE_SETTINGS;
        } else if (quit_hover) {
            exit(0);
        }
    }

    // Draw language toggle button in bottom left
    int lang_button_width = 80;
    int lang_button_height = 40;
    int lang_button_x = 10;
    int lang_button_y = screen_height - lang_button_height - 10;

    Rectangle lang_button = {
        (float)lang_button_x,
        (float)lang_button_y,
        (float)lang_button_width,
        (float)lang_button_height
    };

    Vector2 mouse_pos_lang = GetMousePosition();
    bool lang_hover = CheckCollisionPointRec(mouse_pos_lang, lang_button);

    // Draw language button
    DrawRectangleRec(lang_button, lang_hover ? LIGHTGRAY : (Color){60, 60, 60, 255});
    DrawRectangleLinesEx(lang_button, 2, WHITE);

    Vector2 lang_text_size = MeasureTextEx(font, menu->current_language, 24, 1);
    DrawTextEx(font, menu->current_language,
               (Vector2){lang_button_x + (lang_button_width - (int)lang_text_size.x) / 2,
                        lang_button_y + (lang_button_height - (int)lang_text_size.y) / 2},
               24, 1, BLACK);

    // Handle language button click
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && lang_hover) {
        // Cycle to next language
        menu->current_language_index = (menu->current_language_index + 1) % menu->available_languages_count;
        menu_load_language(menu, menu->available_languages[menu->current_language_index]);
        menu_save_settings(menu);
    }
}

void menu_draw_world_select(MenuSystem* menu, Font font)
{
    int screen_width = GetScreenWidth();
    int screen_height = GetScreenHeight();

    // Clear background
    ClearBackground((Color){20, 20, 20, 255});

    // Draw title
    Vector2 title_size = MeasureTextEx(font, menu->text_title_select_world, 64, 2);
    DrawTextEx(font, menu->text_title_select_world,
               (Vector2){(screen_width - title_size.x) / 2, 40},
               64, 2, WHITE);

    // World list parameters
    int item_height = 50;
    int item_padding = 10;
    int list_start_y = 120;
    int list_width = 600;
    int list_x = (screen_width - list_width) / 2;
    int visible_items = 8;
    int list_height = visible_items * (item_height + item_padding);

    // Get mouse position
    Vector2 mouse_pos = GetMousePosition();

    // Draw world items
    for (int i = 0; i < menu->world_count && i < visible_items; i++) {
        Rectangle item_rect = {
            (float)list_x,
            (float)(list_start_y + i * (item_height + item_padding)),
            (float)list_width,
            (float)item_height
        };

        bool is_hovered = CheckCollisionPointRec(mouse_pos, item_rect);
        bool is_selected = (i == menu->selected_world_index);

        // Draw item background
        Color bg_color = is_selected ? (Color){80, 120, 200, 255}
                         : is_hovered ? (Color){100, 100, 100, 255}
                         : (Color){60, 60, 60, 255};
        DrawRectangleRec(item_rect, bg_color);
        DrawRectangleLinesEx(item_rect, 2, WHITE);

        // Draw world name and metadata
        DrawTextEx(font, menu->available_worlds[i].name,
                   (Vector2){item_rect.x + 10, item_rect.y + 5},
                   24, 1, WHITE);

        // Draw metadata on second line
        char metadata[256];
        snprintf(metadata, sizeof(metadata), menu->text_last,
                 menu->available_worlds[i].created, menu->available_worlds[i].chunk_count);
        DrawTextEx(font, metadata,
                   (Vector2){item_rect.x + 10, item_rect.y + 28},
                   16, 1, GRAY);

        // Handle click
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && is_hovered) {
            menu->selected_world_index = i;
            strcpy(menu->selected_world_name, menu->available_worlds[i].name);
            menu->should_start_game = true;
            menu->current_state = MENU_STATE_GAME;
        }
    }

    // Back button
    int button_width = 150;
    int button_height = 50;
    int button_y = list_start_y + list_height + 30;
    Rectangle back_button = {
        (float)(list_x + list_width - button_width),
        (float)button_y,
        (float)button_width,
        (float)button_height
    };

    bool back_hover = CheckCollisionPointRec(mouse_pos, back_button);
    DrawRectangleRec(back_button, back_hover ? LIGHTGRAY : (Color){60, 60, 60, 255});
    DrawRectangleLinesEx(back_button, 2, WHITE);
    Vector2 back_text_size = MeasureTextEx(font, menu->text_back, 28, 1);
    DrawTextEx(font, menu->text_back,
               (Vector2){back_button.x + (button_width - back_text_size.x) / 2, back_button.y + 10},
               28, 1, BLACK);

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && back_hover) {
        menu->current_state = MENU_STATE_MAIN;
    }

    // Keyboard navigation
    if (IsKeyPressed(KEY_UP) && menu->selected_world_index > 0) {
        menu->selected_world_index--;
    }
    if (IsKeyPressed(KEY_DOWN) && menu->selected_world_index < menu->world_count - 1) {
        menu->selected_world_index++;
    }
    if (IsKeyPressed(KEY_ENTER) && menu->world_count > 0) {
        strcpy(menu->selected_world_name, menu->available_worlds[menu->selected_world_index].name);
        menu->should_start_game = true;
        menu->current_state = MENU_STATE_GAME;
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        menu->current_state = MENU_STATE_MAIN;
    }

    // Display info if no worlds found
    if (menu->world_count == 0) {
        Vector2 no_worlds_size = MeasureTextEx(font, menu->text_no_worlds, 32, 1);
        DrawTextEx(font, menu->text_no_worlds,
                   (Vector2){(screen_width - no_worlds_size.x) / 2, screen_height / 2},
                   32, 1, GRAY);
    }
}

void menu_update_input(MenuSystem* menu)
{
    // ESC key returns to main menu from world select
    if (menu->current_state == MENU_STATE_WORLD_SELECT && IsKeyPressed(KEY_ESCAPE)) {
        menu->current_state = MENU_STATE_MAIN;
    }
    // ESC key returns to main menu from create world
    if (menu->current_state == MENU_STATE_CREATE_WORLD && IsKeyPressed(KEY_ESCAPE)) {
        menu->current_state = MENU_STATE_MAIN;
        menu->create_world_error = false;
    }
    // ESC key returns to main menu from credits
    if (menu->current_state == MENU_STATE_CREDITS && IsKeyPressed(KEY_ESCAPE)) {
        menu->current_state = MENU_STATE_MAIN;
    }
    // ESC key returns to main menu from settings
    if (menu->current_state == MENU_STATE_SETTINGS && IsKeyPressed(KEY_ESCAPE)) {
        menu->current_state = MENU_STATE_MAIN;
    }
}

void menu_draw_create_world(MenuSystem* menu, Font font)
{
    int screen_width = GetScreenWidth();
    int screen_height = GetScreenHeight();

    // Clear background
    ClearBackground((Color){20, 20, 20, 255});

    // Draw title
    Vector2 title_size = MeasureTextEx(font, menu->text_title_create_world, 64, 2);
    DrawTextEx(font, menu->text_title_create_world,
               (Vector2){(screen_width - title_size.x) / 2, 40},
               64, 2, WHITE);

    // Input box
    int input_width = 400;
    int input_height = 50;
    int input_x = (screen_width - input_width) / 2;
    int input_y = screen_height / 2 - 50;

    Rectangle input_box = {
        (float)input_x,
        (float)input_y,
        (float)input_width,
        (float)input_height
    };

    // Draw input box
    DrawRectangleRec(input_box, (Color){60, 60, 60, 255});
    DrawRectangleLinesEx(input_box, 2, WHITE);

    // Handle text input
    int key = GetCharPressed();
    while (key > 0) {
        if ((key >= 32 && key <= 125) && menu->new_world_name_len < 255) {
            // Only allow alphanumeric, underscore, and space
            if ((key >= '0' && key <= '9') || (key >= 'a' && key <= 'z') ||
                (key >= 'A' && key <= 'Z') || key == '_' || key == ' ') {
                menu->new_world_name[menu->new_world_name_len++] = (char)key;
                menu->new_world_name[menu->new_world_name_len] = '\0';
                menu->create_world_error = false;
            }
        }
        key = GetCharPressed();
    }

    // Handle backspace
    if (IsKeyPressed(KEY_BACKSPACE) && menu->new_world_name_len > 0) {
        menu->new_world_name_len--;
        menu->new_world_name[menu->new_world_name_len] = '\0';
    }

    // Draw input text
    DrawTextEx(font, menu->new_world_name,
               (Vector2){input_x + 10, input_y + 10},
               32, 1, WHITE);

    // Draw blinking cursor
    if ((int)(GetTime() * 2) % 2 == 0) {
        Vector2 cursor_pos = MeasureTextEx(font, menu->new_world_name, 32, 1);
        DrawLineEx((Vector2){input_x + 10 + cursor_pos.x, input_y + 10},
                  (Vector2){input_x + 10 + cursor_pos.x, input_y + 40}, 2, WHITE);
    }

    // Draw label
    DrawTextEx(font, menu->text_world_name_label,
               (Vector2){input_x, input_y - 40},
               20, 1, GRAY);

    // Buttons
    int button_width = 150;
    int button_height = 50;
    int button_spacing = 20;
    int buttons_y = screen_height / 2 + 100;

    Rectangle create_btn = {
        (float)(input_x + input_width / 2 - button_width - button_spacing / 2),
        (float)buttons_y,
        (float)button_width,
        (float)button_height
    };

    Rectangle cancel_btn = {
        (float)(input_x + input_width / 2 + button_spacing / 2),
        (float)buttons_y,
        (float)button_width,
        (float)button_height
    };

    Vector2 mouse_pos = GetMousePosition();
    bool create_hover = CheckCollisionPointRec(mouse_pos, create_btn);
    bool cancel_hover = CheckCollisionPointRec(mouse_pos, cancel_btn);

    // Draw Create button
    DrawRectangleRec(create_btn, create_hover ? LIGHTGRAY : (Color){60, 60, 60, 255});
    DrawRectangleLinesEx(create_btn, 2, WHITE);
    Vector2 create_text_size = MeasureTextEx(font, menu->text_create_btn, 28, 1);
    DrawTextEx(font, menu->text_create_btn,
               (Vector2){create_btn.x + (button_width - create_text_size.x) / 2, create_btn.y + 10},
               28, 1, BLACK);

    // Draw Cancel button
    DrawRectangleRec(cancel_btn, cancel_hover ? LIGHTGRAY : (Color){60, 60, 60, 255});
    DrawRectangleLinesEx(cancel_btn, 2, WHITE);
    Vector2 cancel_text_size = MeasureTextEx(font, menu->text_cancel_btn, 28, 1);
    DrawTextEx(font, menu->text_cancel_btn,
               (Vector2){cancel_btn.x + (button_width - cancel_text_size.x) / 2, cancel_btn.y + 10},
               28, 1, BLACK);

    // Draw error message if any
    if (menu->create_world_error) {
        Vector2 error_size = MeasureTextEx(font, menu->create_world_error_msg, 24, 1);
        DrawTextEx(font, menu->create_world_error_msg,
                   (Vector2){(screen_width - error_size.x) / 2, buttons_y + button_height + 20},
                   24, 1, RED);
    }

    // Handle button clicks and Enter key
    bool should_create = false;
    if (IsKeyPressed(KEY_ENTER)) {
        should_create = true;
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (create_hover) {
            should_create = true;
        } else if (cancel_hover) {
            menu->current_state = MENU_STATE_MAIN;
            menu->create_world_error = false;
        }
    }

    if (should_create) {
        // Validate world name
        if (menu->new_world_name_len == 0) {
            menu->create_world_error = true;
            strcpy(menu->create_world_error_msg, menu->text_error_empty_name);
        } else {
            // Check if world already exists
            bool already_exists = false;
            for (int i = 0; i < menu->world_count; i++) {
                if (strcmp(menu->available_worlds[i].name, menu->new_world_name) == 0) {
                    already_exists = true;
                    break;
                }
            }
            if (already_exists) {
                menu->create_world_error = true;
                strcpy(menu->create_world_error_msg, menu->text_error_exists);
            } else {
                // Create the world
                strcpy(menu->selected_world_name, menu->new_world_name);
                menu->should_start_game = true;
                menu->current_state = MENU_STATE_GAME;
            }
        }
    }
}

void menu_draw_settings(MenuSystem* menu, Font font)
{
    int screen_width = GetScreenWidth();

    // Clear background
    ClearBackground((Color){20, 20, 20, 255});

    // Draw title
    Vector2 title_size = MeasureTextEx(font, menu->game_text.settings, 64, 2);
    DrawTextEx(font, menu->game_text.settings,
               (Vector2){(screen_width - title_size.x) / 2, 40},
               64, 2, WHITE);

    // Settings panel - increased height for font selection
    int panel_width = 600;
    int panel_height = 420;
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
    DrawTextEx(font, menu->game_text.render_dist_label, (Vector2){panel_x + 30, slider_y - 35}, 28, 1, WHITE);

    // Draw value
    char render_dist_str[32];
    snprintf(render_dist_str, sizeof(render_dist_str), "%.0f", menu->render_distance);
    DrawTextEx(font, render_dist_str, (Vector2){panel_x + 500, slider_y - 35}, 28, 1, GRAY);

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
    DrawTextEx(font, menu->game_text.max_fps_label, (Vector2){panel_x + 30, fps_slider_y - 35}, 28, 1, WHITE);

    // Draw value
    char fps_str[32];
    if (menu->max_fps == 0) {
        snprintf(fps_str, sizeof(fps_str), "%s", menu->game_text.uncapped);
    } else {
        snprintf(fps_str, sizeof(fps_str), "%d", menu->max_fps);
    }
    DrawTextEx(font, fps_str, (Vector2){panel_x + 500, fps_slider_y - 35}, 28, 1, GRAY);

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

    // Font selection
    int font_y = fps_slider_y + 90;

    // Draw label
    DrawTextEx(font, menu->game_text.font_family_label, (Vector2){panel_x + 30, font_y - 35}, 24, 1, WHITE);

    // Previous/Next buttons for font family selection
    int button_small_width = 35;
    int button_small_height = 35;
    int prev_button_x = panel_x + 50;
    int next_button_x = panel_x + 500;
    int font_display_y = font_y - 15;

    Rectangle prev_button = {
        (float)prev_button_x,
        (float)font_display_y,
        (float)button_small_width,
        (float)button_small_height
    };

    Rectangle next_button = {
        (float)next_button_x,
        (float)font_display_y,
        (float)button_small_width,
        (float)button_small_height
    };

    bool prev_hover = CheckCollisionPointRec(mouse_pos, prev_button);
    bool next_hover = CheckCollisionPointRec(mouse_pos, next_button);

    // Draw previous button
    DrawRectangleRec(prev_button, prev_hover ? LIGHTGRAY : (Color){60, 60, 60, 255});
    DrawRectangleLinesEx(prev_button, 2, WHITE);
    DrawTextEx(font, "<", (Vector2){prev_button_x + 7, font_display_y + 3}, 24, 1, BLACK);

    // Draw next button
    DrawRectangleRec(next_button, next_hover ? LIGHTGRAY : (Color){60, 60, 60, 255});
    DrawRectangleLinesEx(next_button, 2, WHITE);
    DrawTextEx(font, ">", (Vector2){next_button_x + 9, font_display_y + 3}, 24, 1, BLACK);

    // Draw current font family name in the middle
    Vector2 family_name_size = MeasureTextEx(font, menu->font_families[menu->current_font_family_index], 22, 1);
    DrawTextEx(font, menu->font_families[menu->current_font_family_index],
               (Vector2){panel_x + (panel_width - (int)family_name_size.x) / 2, font_display_y + 6},
               22, 1, WHITE);

    // Handle font family selection buttons
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (prev_hover) {
            menu->current_font_family_index = (menu->current_font_family_index - 1 + menu->font_families_count) % menu->font_families_count;
            // Scan variants for the new family
            menu_scan_font_variants(menu, menu->font_families[menu->current_font_family_index]);
            menu_save_settings(menu);
        } else if (next_hover) {
            menu->current_font_family_index = (menu->current_font_family_index + 1) % menu->font_families_count;
            // Scan variants for the new family
            menu_scan_font_variants(menu, menu->font_families[menu->current_font_family_index]);
            menu_save_settings(menu);
        }
    }

    // Font variant dropdown
    int variant_y = font_display_y + 50;
    DrawTextEx(font, menu->game_text.font_variant_label, (Vector2){panel_x + 30, variant_y - 25}, 24, 1, WHITE);

    // Draw variant dropdown background
    int dropdown_x = panel_x + 140;
    int dropdown_y = variant_y - 20;
    int dropdown_width = 330;
    int dropdown_height = 30;

    DrawRectangle(dropdown_x, dropdown_y, dropdown_width, dropdown_height, (Color){60, 60, 60, 255});
    DrawRectangleLines(dropdown_x, dropdown_y, dropdown_width, dropdown_height, WHITE);

    // Display current variant name (without .ttf extension)
    char variant_display[256];
    strcpy(variant_display, menu->font_variants[menu->current_font_variant_index]);
    // Remove .ttf extension
    char* ext = strrchr(variant_display, '.');
    if (ext) *ext = '\0';

    DrawTextEx(font, variant_display,
               (Vector2){dropdown_x + 10, dropdown_y + 5},
               20, 1, WHITE);

    // Up/Down buttons for variant selection
    int variant_button_width = 30;
    int variant_button_height = 30;
    int variant_up_x = dropdown_x + dropdown_width + 5;
    int variant_down_x = dropdown_x + dropdown_width + 5;
    int variant_up_y = variant_y - 20;
    int variant_down_y = variant_y - 20 + variant_button_height;

    Rectangle variant_up_button = {
        (float)variant_up_x,
        (float)variant_up_y,
        (float)variant_button_width,
        (float)variant_button_height
    };

    Rectangle variant_down_button = {
        (float)variant_down_x,
        (float)variant_down_y,
        (float)variant_button_width,
        (float)variant_button_height
    };

    bool variant_up_hover = CheckCollisionPointRec(mouse_pos, variant_up_button);
    bool variant_down_hover = CheckCollisionPointRec(mouse_pos, variant_down_button);

    // Draw up button
    DrawRectangleRec(variant_up_button, variant_up_hover ? LIGHTGRAY : (Color){60, 60, 60, 255});
    DrawRectangleLinesEx(variant_up_button, 2, WHITE);
    DrawTextEx(font, "^", (Vector2){variant_up_x + 6, variant_up_y + 2}, 20, 1, BLACK);

    // Draw down button
    DrawRectangleRec(variant_down_button, variant_down_hover ? LIGHTGRAY : (Color){60, 60, 60, 255});
    DrawRectangleLinesEx(variant_down_button, 2, WHITE);
    DrawTextEx(font, "v", (Vector2){variant_down_x + 6, variant_down_y + 2}, 20, 1, BLACK);

    // Handle variant selection
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (variant_up_hover && menu->current_font_variant_index > 0) {
            menu->current_font_variant_index--;
            menu_save_settings(menu);
        } else if (variant_down_hover && menu->current_font_variant_index < menu->font_variants_count - 1) {
            menu->current_font_variant_index++;
            menu_save_settings(menu);
        }
    }

    // Back button
    int button_width = 150;
    int button_height = 50;
    int button_y = panel_y + panel_height + 40;
    Rectangle back_button = {
        (float)(screen_width / 2 - button_width / 2),
        (float)button_y,
        (float)button_width,
        (float)button_height
    };

    bool back_hover = CheckCollisionPointRec(mouse_pos, back_button);
    DrawRectangleRec(back_button, back_hover ? LIGHTGRAY : (Color){60, 60, 60, 255});
    DrawRectangleLinesEx(back_button, 2, WHITE);
    Vector2 back_text_size = MeasureTextEx(font, menu->text_back, 28, 1);
    DrawTextEx(font, menu->text_back,
               (Vector2){back_button.x + (button_width - back_text_size.x) / 2, back_button.y + 10},
               28, 1, BLACK);

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && back_hover) {
        menu->current_state = MENU_STATE_MAIN;
    }
}

void menu_draw_credits(MenuSystem* menu, Font font)
{
    int screen_width = GetScreenWidth();
    int screen_height = GetScreenHeight();

    // Draw background
    if (menu->background_loaded) {
        DrawTexturePro(menu->background_texture,
                       (Rectangle){0, 0, menu->background_texture.width, menu->background_texture.height},
                       (Rectangle){0, 0, screen_width, screen_height},
                       (Vector2){0, 0},
                       0,
                       WHITE);
    } else {
        ClearBackground((Color){20, 20, 20, 255});
    }

    // Draw semi-transparent overlay for readability
    DrawRectangle(0, 0, screen_width, screen_height, (Color){0, 0, 0, 150});

    int padding = 40;
    int text_x = padding;
    int text_y = padding;
    int font_size = 20;
    int spacing = 2;

    // Draw background box
    int box_width = screen_width - (padding * 2);
    int box_height = screen_height - (padding * 2) - 60;
    DrawRectangle(text_x - padding / 2, text_y - padding / 2,
                  box_width + padding, box_height + padding,
                  (Color){40, 40, 40, 200});
    DrawRectangleLines(text_x - padding / 2, text_y - padding / 2,
                       box_width + padding, box_height + padding,
                       WHITE);

// Draw the credits text directly - simple single line test
    DrawTextEx(font, menu->credits_text,
               (Vector2){text_x, text_y},
               font_size, spacing, WHITE);

    // Draw instructions
    Vector2 instr_size = MeasureTextEx(font, menu->game_text.press_esc_to_return, 18, 1);
    DrawTextEx(font, menu->game_text.press_esc_to_return,
               (Vector2){(screen_width - instr_size.x) / 2, screen_height - 40},
               18, 1, GRAY);
}