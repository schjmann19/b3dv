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

    // Scan for available worlds
    menu_scan_worlds(menu);

    return menu;
}

void menu_system_free(MenuSystem* menu)
{
    if (!menu) return;
    if (menu->available_worlds) {
        free(menu->available_worlds);
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

    // Clear background
    ClearBackground((Color){20, 20, 20, 255});

    // Draw title
    const char* title = "B3DV";
    Vector2 title_size = MeasureTextEx(font, title, 80, 2);
    DrawTextEx(font, title,
               (Vector2){(screen_width - title_size.x) / 2, 60},
               80, 2, WHITE);

    // Draw version
    const char* version = "Basic 3D Visualizer - v0.0.8d";
    Vector2 version_size = MeasureTextEx(font, version, 24, 1);
    DrawTextEx(font, version,
               (Vector2){(screen_width - version_size.x) / 2, 150},
               24, 1, GRAY);

    // Button dimensions
    int button_width = 250;
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

    // Quit button
    Rectangle quit_button = {
        center_x - button_width / 2,
        center_y + 2 * (button_height + button_spacing),
        button_width,
        button_height
    };

    // Get mouse position
    Vector2 mouse_pos = GetMousePosition();
    bool world_hover = CheckCollisionPointRec(mouse_pos, world_button);
    bool create_hover = CheckCollisionPointRec(mouse_pos, create_button);
    bool quit_hover = CheckCollisionPointRec(mouse_pos, quit_button);

    // Draw Select World button
    DrawRectangleRec(world_button, world_hover ? LIGHTGRAY : (Color){60, 60, 60, 255});
    DrawRectangleLinesEx(world_button, 2, WHITE);
    Vector2 world_text_size = MeasureTextEx(font, "Select World", 32, 1);
    DrawTextEx(font, "Select World",
               (Vector2){center_x - world_text_size.x / 2, center_y + 14},
               32, 1, BLACK);

    // Draw Create World button
    DrawRectangleRec(create_button, create_hover ? LIGHTGRAY : (Color){60, 60, 60, 255});
    DrawRectangleLinesEx(create_button, 2, WHITE);
    Vector2 create_text_size = MeasureTextEx(font, "Create World", 32, 1);
    DrawTextEx(font, "Create World",
               (Vector2){center_x - create_text_size.x / 2, center_y + button_height + button_spacing + 14},
               32, 1, BLACK);

    // Draw Quit button
    DrawRectangleRec(quit_button, quit_hover ? LIGHTGRAY : (Color){60, 60, 60, 255});
    DrawRectangleLinesEx(quit_button, 2, WHITE);
    Vector2 quit_text_size = MeasureTextEx(font, "Quit", 32, 1);
    DrawTextEx(font, "Quit",
               (Vector2){center_x - quit_text_size.x / 2, center_y + 2 * (button_height + button_spacing) + 14},
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
        } else if (quit_hover) {
            exit(0);
        }
    }
}

void menu_draw_world_select(MenuSystem* menu, Font font)
{
    int screen_width = GetScreenWidth();
    int screen_height = GetScreenHeight();

    // Clear background
    ClearBackground((Color){20, 20, 20, 255});

    // Draw title
    const char* title = "Select World";
    Vector2 title_size = MeasureTextEx(font, title, 64, 2);
    DrawTextEx(font, title,
               (Vector2){(screen_width - title_size.x) / 2, 40},
               64, 2, WHITE);

    // World list parameters
    int item_height = 50;
    int item_padding = 10;
    int list_start_y = 120;
    int list_width = 400;
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
        snprintf(metadata, sizeof(metadata), "Last: %s | Chunks: %d",
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
    Vector2 back_text_size = MeasureTextEx(font, "Back", 28, 1);
    DrawTextEx(font, "Back",
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
        const char* no_worlds = "No worlds found";
        Vector2 no_worlds_size = MeasureTextEx(font, no_worlds, 32, 1);
        DrawTextEx(font, no_worlds,
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
}

void menu_draw_create_world(MenuSystem* menu, Font font)
{
    int screen_width = GetScreenWidth();
    int screen_height = GetScreenHeight();

    // Clear background
    ClearBackground((Color){20, 20, 20, 255});

    // Draw title
    const char* title = "Create New World";
    Vector2 title_size = MeasureTextEx(font, title, 64, 2);
    DrawTextEx(font, title,
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
    const char* label = "World Name (alphanumeric + underscore):";
    DrawTextEx(font, label,
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
    Vector2 create_text_size = MeasureTextEx(font, "Create", 28, 1);
    DrawTextEx(font, "Create",
               (Vector2){create_btn.x + (button_width - create_text_size.x) / 2, create_btn.y + 10},
               28, 1, BLACK);

    // Draw Cancel button
    DrawRectangleRec(cancel_btn, cancel_hover ? LIGHTGRAY : (Color){60, 60, 60, 255});
    DrawRectangleLinesEx(cancel_btn, 2, WHITE);
    Vector2 cancel_text_size = MeasureTextEx(font, "Cancel", 28, 1);
    DrawTextEx(font, "Cancel",
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
            strcpy(menu->create_world_error_msg, "World name cannot be empty");
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
                strcpy(menu->create_world_error_msg, "World already exists");
            } else {
                // Create the world
                strcpy(menu->selected_world_name, menu->new_world_name);
                menu->should_start_game = true;
                menu->current_state = MENU_STATE_GAME;
            }
        }
    }
}
