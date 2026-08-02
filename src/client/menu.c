#include "../../include/menu.h"
#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

// Helper function to check if a path is a directory (cross-platform)
static int is_directory(const char *path) {
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

// Helper function to count and load a random background image from any .png in mainmenubackground/
static void menu_load_random_background(MenuSystem *menu) {
    menu->background_loaded = false;

    // Open the backgrounds directory
    DIR *dir = opendir("./assets/mainmenubackground");
    if (!dir) {
        return; // Directory doesn't exist
    }

    // Collect all .png filenames
    char filenames[256][256] = {0}; // Up to 256 images
    int png_count = 0;
    struct dirent *entry;

    while ((entry = readdir(dir)) && png_count < 256) {
        // Check if filename ends with .png
        int len = strlen(entry->d_name);
        if (len > 4 && strcmp(entry->d_name + len - 4, ".png") == 0) {
            strcpy(filenames[png_count], entry->d_name);
            png_count++;
        }
    }
    closedir(dir);

    if (png_count > 0) {
        // Seed random number generator
        srand((unsigned int)time(NULL));
        // Select a random background index
        int random_index = rand() % png_count;

        char path[512];
        snprintf(path, sizeof(path), "./assets/mainmenubackground/%s", filenames[random_index]);

        menu->background_texture = LoadTexture(path);
        menu->background_loaded = true;
    }
}

// Load splash texts from file and select a random one
static void menu_load_splash_text(MenuSystem *menu) {
    menu->splash_texts_count = 0;
    strcpy(menu->current_splash_text, "");

    FILE *file = fopen("./assets/splashtexts/splashs.txt", "r");
    if (!file) {
        return; // File doesn't exist
    }

    char line[512];
    while (fgets(line, sizeof(line), file) && menu->splash_texts_count < 256) {
        // Remove newline
        line[strcspn(line, "\n")] = '\0';

        // Skip empty lines
        if (strlen(line) == 0) {
            continue;
        }

        strcpy(menu->splash_texts[menu->splash_texts_count], line);
        menu->splash_texts_count++;
    }
    fclose(file);

    // Select a random splash text if any were loaded
    if (menu->splash_texts_count > 0) {
        int random_index = rand() % menu->splash_texts_count;
        strcpy(menu->current_splash_text, menu->splash_texts[random_index]);
    }
}

static char *trim_whitespace(char *str) {
    char *end;

    while (*str && isspace((unsigned char)*str)) {
        str++;
    }

    if (*str == '\0') {
        return str;
    }

    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        end--;
    }
    end[1] = '\0';
    return str;
}

static bool is_comment_or_empty(const char *line) {
    while (*line && isspace((unsigned char)*line)) {
        line++;
    }
    if (*line == '\0' || *line == '#') {
        return true;
    }
    if (line[0] == '/' && line[1] == '/') {
        return true;
    }
    return false;
}

#define OFFSET_IN(type, field) (offsetof(type, field))

typedef struct {
    const char *key;
    size_t offset;
    size_t size;
} LocalizationField;

static const LocalizationField localization_fields[] = {
    {"menu.select_world", OFFSET_IN(MenuSystem, text_select_world), sizeof(((MenuSystem *)0)->text_select_world)},
    {"menu.create_world", OFFSET_IN(MenuSystem, text_create_world), sizeof(((MenuSystem *)0)->text_create_world)},
    {"menu.credits_info", OFFSET_IN(MenuSystem, text_credits_info), sizeof(((MenuSystem *)0)->text_credits_info)},
    {"menu.quit", OFFSET_IN(MenuSystem, text_quit), sizeof(((MenuSystem *)0)->text_quit)},
    {"menu.back", OFFSET_IN(MenuSystem, text_back), sizeof(((MenuSystem *)0)->text_back)},
    {"menu.world_name_label", OFFSET_IN(MenuSystem, text_world_name_label), sizeof(((MenuSystem *)0)->text_world_name_label)},
    {"menu.compress_world_files", OFFSET_IN(MenuSystem, text_compress_world_files), sizeof(((MenuSystem *)0)->text_compress_world_files)},
    {"menu.create_btn", OFFSET_IN(MenuSystem, text_create_btn), sizeof(((MenuSystem *)0)->text_create_btn)},
    {"menu.cancel_btn", OFFSET_IN(MenuSystem, text_cancel_btn), sizeof(((MenuSystem *)0)->text_cancel_btn)},
    {"menu.error_empty_name", OFFSET_IN(MenuSystem, text_error_empty_name), sizeof(((MenuSystem *)0)->text_error_empty_name)},
    {"menu.error_exists", OFFSET_IN(MenuSystem, text_error_exists), sizeof(((MenuSystem *)0)->text_error_exists)},
    {"menu.no_worlds", OFFSET_IN(MenuSystem, text_no_worlds), sizeof(((MenuSystem *)0)->text_no_worlds)},
    {"menu.title_create_world", OFFSET_IN(MenuSystem, text_title_create_world), sizeof(((MenuSystem *)0)->text_title_create_world)},
    {"menu.title_select_world", OFFSET_IN(MenuSystem, text_title_select_world), sizeof(((MenuSystem *)0)->text_title_select_world)},
    {"menu.last", OFFSET_IN(MenuSystem, text_last), sizeof(((MenuSystem *)0)->text_last)},
    {"hud.move_controls", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, move_controls), sizeof(((GameText *)0)->move_controls)},
    {"hud.metrics_help", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, metrics_help), sizeof(((GameText *)0)->metrics_help)},
    {"hud.mouse_help", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, mouse_help), sizeof(((GameText *)0)->mouse_help)},
    {"hud.look_help", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, look_help), sizeof(((GameText *)0)->look_help)},
    {"hud.pause_help", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, pause_help), sizeof(((GameText *)0)->pause_help)},
    {"hud.paused", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, paused), sizeof(((GameText *)0)->paused)},
    {"hud.resume", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, resume), sizeof(((GameText *)0)->resume)},
    {"hud.back_to_menu", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, back_to_menu), sizeof(((GameText *)0)->back_to_menu)},
    {"hud.perf_metrics", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, perf_metrics), sizeof(((GameText *)0)->perf_metrics)},
    {"hud.system_info", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, system_info), sizeof(((GameText *)0)->system_info)},
    {"hud.player_info", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, player_info), sizeof(((GameText *)0)->player_info)},
    {"hud.fps_label", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, fps_label), sizeof(((GameText *)0)->fps_label)},
    {"hud.coord_label", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, coord_label), sizeof(((GameText *)0)->coord_label)},
    {"hud.version", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, version), sizeof(((GameText *)0)->version)},
    {"settings.title", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, settings), sizeof(((GameText *)0)->settings)},
    {"settings.render_dist_label", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, render_dist_label), sizeof(((GameText *)0)->render_dist_label)},
    {"settings.max_fps_label", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, max_fps_label), sizeof(((GameText *)0)->max_fps_label)},
    {"settings.font_family_label", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, font_family_label), sizeof(((GameText *)0)->font_family_label)},
    {"settings.font_variant_label", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, font_variant_label), sizeof(((GameText *)0)->font_variant_label)},
    {"settings.uncapped", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, uncapped), sizeof(((GameText *)0)->uncapped)},
    {"settings.nickname_label", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, nickname_label), sizeof(((GameText *)0)->nickname_label)},
    {"settings.compass_hud_label", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, compass_hud_label), sizeof(((GameText *)0)->compass_hud_label)},
    {"settings.press_esc_to_return", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, press_esc_to_return), sizeof(((GameText *)0)->press_esc_to_return)},
    {"settings.see_full_info", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, see_full_info), sizeof(((GameText *)0)->see_full_info)},
    {"inventory.title", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, inventory_title), sizeof(((GameText *)0)->inventory_title)},
    {"inventory.close", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, inventory_close), sizeof(((GameText *)0)->inventory_close)},
    {"msg.quitting", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, msg_quitting), sizeof(((GameText *)0)->msg_quitting)},
    {"msg.teleported", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, msg_teleported), sizeof(((GameText *)0)->msg_teleported)},
    {"msg.teleport_usage", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, msg_teleport_usage), sizeof(((GameText *)0)->msg_teleport_usage)},
    {"msg.world_saved", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, msg_world_saved), sizeof(((GameText *)0)->msg_world_saved)},
    {"msg.world_save_failed", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, msg_world_save_failed), sizeof(((GameText *)0)->msg_world_save_failed)},
    {"msg.world_loaded", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, msg_world_loaded), sizeof(((GameText *)0)->msg_world_loaded)},
    {"msg.world_load_failed", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, msg_world_load_failed), sizeof(((GameText *)0)->msg_world_load_failed)},
    {"msg.invalid_world_name", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, msg_invalid_world_name), sizeof(((GameText *)0)->msg_invalid_world_name)},
    {"msg.block_selected", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, msg_block_selected), sizeof(((GameText *)0)->msg_block_selected)},
    {"msg.unknown_block", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, msg_unknown_block), sizeof(((GameText *)0)->msg_unknown_block)},
    {"msg.flight_enabled", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, msg_flight_enabled), sizeof(((GameText *)0)->msg_flight_enabled)},
    {"msg.flight_disabled", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, msg_flight_disabled), sizeof(((GameText *)0)->msg_flight_disabled)},
    {"msg.fly_usage", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, msg_fly_usage), sizeof(((GameText *)0)->msg_fly_usage)},
    {"msg.noclip_enabled", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, msg_noclip_enabled), sizeof(((GameText *)0)->msg_noclip_enabled)},
    {"msg.noclip_disabled", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, msg_noclip_disabled), sizeof(((GameText *)0)->msg_noclip_disabled)},
    {"msg.noclip_usage", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, msg_noclip_usage), sizeof(((GameText *)0)->msg_noclip_usage)},
    {"msg.console_command_failed", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, msg_console_command_failed), sizeof(((GameText *)0)->msg_console_command_failed)},
    {"msg.third_person_camera", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, msg_third_person_camera), sizeof(((GameText *)0)->msg_third_person_camera)},
    {"msg.first_person_camera", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, msg_first_person_camera), sizeof(((GameText *)0)->msg_first_person_camera)},
    {"msg.block_set", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, msg_block_set), sizeof(((GameText *)0)->msg_block_set)},
    {"msg.out_of_bounds", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, msg_out_of_bounds), sizeof(((GameText *)0)->msg_out_of_bounds)},
    {"msg.setblock_usage", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, msg_setblock_usage), sizeof(((GameText *)0)->msg_setblock_usage)},
    {"msg.unknown_command", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, msg_unknown_command), sizeof(((GameText *)0)->msg_unknown_command)},
    {"msg.chunk_borders_enabled", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, msg_chunk_borders_enabled), sizeof(((GameText *)0)->msg_chunk_borders_enabled)},
    {"msg.chunk_borders_disabled", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, msg_chunk_borders_disabled), sizeof(((GameText *)0)->msg_chunk_borders_disabled)},
    {"msg.chunk_borders_usage", OFFSET_IN(MenuSystem, game_text) + offsetof(GameText, msg_chunk_borders_usage), sizeof(((GameText *)0)->msg_chunk_borders_usage)},
};

static void clear_localization_strings(MenuSystem *menu) {
    for (size_t i = 0; i < sizeof(localization_fields) / sizeof(localization_fields[0]); i++) {
        memset((char *)menu + localization_fields[i].offset, 0, localization_fields[i].size);
    }
}

static bool menu_set_localized_text(MenuSystem *menu, const char *key, const char *value) {
    for (size_t i = 0; i < sizeof(localization_fields) / sizeof(localization_fields[0]); i++) {
        if (strcmp(localization_fields[i].key, key) == 0) {
            char *dest = (char *)menu + localization_fields[i].offset;
            strncpy(dest, value, localization_fields[i].size - 1);
            dest[localization_fields[i].size - 1] = '\0';
            return true;
        }
    }
    return false;
}

const char *lang_get(MenuSystem *menu, const char *key) {
    for (size_t i = 0; i < sizeof(localization_fields) / sizeof(localization_fields[0]); i++) {
        if (strcmp(localization_fields[i].key, key) == 0) {
            return (const char *)menu + localization_fields[i].offset;
        }
    }
    return "";
}

static void load_localization_file(MenuSystem *menu, const char *path, int positional_count) {
    FILE *file = fopen(path, "r");
    if (!file) {
        fprintf(stderr, "Failed to load localization file: %s\n", path);
        return;
    }

    char line[512];
    bool use_key_value = false;

    while (fgets(line, sizeof(line), file)) {
        char *trimmed = trim_whitespace(line);
        if (is_comment_or_empty(trimmed)) {
            continue;
        }
        if (strchr(trimmed, '=') != NULL) {
            use_key_value = true;
        }
        break;
    }

    rewind(file);

    int positional_index = 0;
    while (fgets(line, sizeof(line), file)) {
        char *trimmed = trim_whitespace(line);
        if (is_comment_or_empty(trimmed)) {
            continue;
        }

        if (use_key_value) {
            char *equals = strchr(trimmed, '=');
            if (!equals) {
                continue;
            }
            *equals = '\0';
            char *key = trim_whitespace(trimmed);
            char *value = trim_whitespace(equals + 1);
            if (!menu_set_localized_text(menu, key, value)) {
                fprintf(stderr, "Warning: unknown localization key '%s' in %s\n", key, path);
            }
        } else {
            if (positional_index >= positional_count) {
                break;
            }
            char *dest = (char *)menu + localization_fields[positional_index].offset;
            size_t size = localization_fields[positional_index].size;
            strncpy(dest, trimmed, size - 1);
            dest[size - 1] = '\0';
            positional_index++;
        }
    }

    fclose(file);
}

// Scan available language directories in assets/text/
static void menu_scan_languages(MenuSystem *menu) {
    DIR *dir = opendir("./assets/text");
    if (!dir) {
        return;
    }

    menu->available_languages_count = 0;
    struct dirent *entry;

    while ((entry = readdir(dir)) && menu->available_languages_count < 16) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

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
bool menu_load_text_file(const char *language, const char *filename, char *out_buffer, int buffer_size) {
    char path[512];
    snprintf(path, sizeof(path), "./assets/text/%s/%s", language, filename);

    FILE *file = fopen(path, "r");
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
void menu_load_language(MenuSystem *menu, const char *language) {
    strcpy(menu->current_language, language);
    clear_localization_strings(menu);

    char menu_path[512];
    snprintf(menu_path, sizeof(menu_path), "./assets/text/%s/menu.txt", language);
    load_localization_file(menu, menu_path, 41);

    char chat_path[512];
    snprintf(chat_path, sizeof(chat_path), "./assets/text/%s/chat.txt", language);
    load_localization_file(menu, chat_path, 27);

    // Load credits text from credits.txt
    char credits_path[512];
    snprintf(credits_path, sizeof(credits_path), "./assets/text/%s/credits.txt", language);

    FILE *credits_file = fopen(credits_path, "r");
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
void menu_scan_fonts(MenuSystem *menu) {
    DIR *dir = opendir("./assets/fonts");
    if (!dir) {
        // Fallback to default font if directory doesn't exist
        strcpy(menu->font_families[0], "JetBrainsMono");
        menu->font_families_count = 1;
        menu->current_font_family_index = 0;
        return;
    }

    menu->font_families_count = 0;
    struct dirent *entry;

    while ((entry = readdir(dir)) && menu->font_families_count < 16) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

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
void menu_scan_font_variants(MenuSystem *menu, const char *font_family) {
    char ttf_dir[512];
    snprintf(ttf_dir, sizeof(ttf_dir), "./assets/fonts/%s/ttf", font_family);

    DIR *dir = opendir(ttf_dir);
    menu->font_variants_count = 0;
    menu->current_font_variant_index = 0;

    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) && menu->font_variants_count < 32) {
            // Look for .ttf files
            char *dot = strrchr(entry->d_name, '.');
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

void menu_load_settings(MenuSystem *menu) {
    FILE *file = fopen("./options.conf", "r");
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

        // Skip empty lines and pure comment lines
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }

        // Strip inline comments (anything after #)
        char *hash = strchr(line, '#');
        if (hash) {
            *hash = '\0';
        }

        // Trim trailing whitespace from the value part
        int len = strlen(line);
        while (len > 0 && isspace(line[len - 1])) {
            line[len - 1] = '\0';
            len--;
        }

        // Parse key=value format
        char *equals = strchr(line, '=');
        if (!equals) {
            continue;
        }

        *equals = '\0';
        char *key = line;
        char *value = equals + 1;

        // Trim leading whitespace from value
        while (*value && isspace(*value)) {
            value++;
        }

        if (strcmp(key, "render_distance") == 0) {
            menu->render_distance = atof(value);
            // Clamp to valid range
            if (menu->render_distance < 10.0f) {
                menu->render_distance = 10.0f;
            }
            if (menu->render_distance > 100.0f) {
                menu->render_distance = 100.0f;
            }
        } else if (strcmp(key, "max_fps") == 0) {
            menu->max_fps = atoi(value);
            // Clamp to valid range (0 = uncapped, 30-240 = capped)
            if (menu->max_fps != 0 && menu->max_fps < 30) {
                menu->max_fps = 30;
            }
            if (menu->max_fps > 240) {
                menu->max_fps = 240;
            }
        } else if (strcmp(key, "language") == 0) {
            strncpy(language, value, sizeof(language) - 1);
            language[sizeof(language) - 1] = '\0';
        } else if (strcmp(key, "font_family") == 0) {
            strncpy(font_family, value, sizeof(font_family) - 1);
            font_family[sizeof(font_family) - 1] = '\0';
        } else if (strcmp(key, "font_variant") == 0) {
            strncpy(font_variant, value, sizeof(font_variant) - 1);
            font_variant[sizeof(font_variant) - 1] = '\0';
        } else if (strcmp(key, "nickname") == 0) {
            strncpy(menu->nickname, value, sizeof(menu->nickname) - 1);
            menu->nickname[sizeof(menu->nickname) - 1] = '\0';
            menu->nickname_len = (int)strlen(menu->nickname);
        } /* else if (strcmp(key, "clouds_enabled") == 0) {
            menu->clouds_enabled = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
        } */
        else if (strcmp(key, "compass_enabled") == 0) {
            menu->compass_enabled = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
        } /* else if (strcmp(key, "clouds_render_distance") == 0) {
            menu->clouds_render_distance = atof(value);
            if (menu->clouds_render_distance < 32.0f) menu->clouds_render_distance = 32.0f;
            if (menu->clouds_render_distance > 512.0f) menu->clouds_render_distance = 512.0f;
        } */
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

void menu_save_settings(MenuSystem *menu) {
    FILE *file = fopen("./options.conf", "w");
    if (!file) {
        fprintf(stderr, "Failed to save options.conf\n");
        return;
    }

    fprintf(file, "# B3DV Game Settings\n");
    fprintf(file, "render_distance=%.1f\n", menu->render_distance);
    fprintf(file, "max_fps=%d # 0 means unlimited\n", menu->max_fps);
    fprintf(file, "nickname=%s\n", menu->nickname);
    // fprintf(file, "clouds_enabled=%s\n", menu->clouds_enabled ? "true" : "false");
    fprintf(file, "compass_enabled=%s\n", menu->compass_enabled ? "true" : "false");
    // fprintf(file, "clouds_render_distance=%.1f\n", menu->clouds_render_distance);
    fprintf(file, "language=%s\n", menu->current_language);
    fprintf(file, "\n"); // fprintf(file, "# do not change fonts manually i made a nice little interface for that :c\n");
    fprintf(file, "font_family=%s\n", menu->font_families[menu->current_font_family_index]);
    fprintf(file, "font_variant=%s\n", menu->font_variants[menu->current_font_variant_index]);

    fclose(file);
}

MenuSystem *menu_system_create(void) {
    MenuSystem *menu = (MenuSystem *)malloc(sizeof(MenuSystem));
    if (!menu) {
        return NULL;
    }

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
    menu->create_world_compress = true;
    strcpy(menu->create_world_error_msg, "");
    strcpy(menu->server_address, "");
    menu->server_address_len = 0;
    strcpy(menu->server_port, "42069");
    menu->server_port_len = (int)strlen(menu->server_port);
    menu->server_socket = -1;
    menu->multiplayer_client = false;
    menu->multiplayer_connecting = false;
    menu->multiplayer_connected = false;
    menu->multiplayer_player_uid = 0x00000002;
    menu->multiplayer_connect_thread_active = false;
    menu->multiplayer_handshake_buffer[0] = '\0';
    menu->multiplayer_handshake_used = 0;
    strcpy(menu->server_world_name, "");
    menu->multiplayer_active_field = 0;
    menu->multiplayer_error = false;
    strcpy(menu->multiplayer_error_msg, "");

    // Initialize settings with defaults
    menu->render_distance = 50.0f;
    menu->max_fps = 144;
    strcpy(menu->nickname, "Player");
    menu->nickname_len = (int)strlen(menu->nickname);
    menu->nickname_edit_active = false;
    // menu->clouds_enabled = true;
    menu->compass_enabled = true;
    // menu->clouds_render_distance = 128.0f; //

    // Initialize multiplayer connect mutex
    pthread_mutex_init(&menu->multiplayer_connect_mutex, NULL);

    // Load random background image from mainmenubackground folder
    menu_load_random_background(menu);
    menu_load_splash_text(menu);

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

    // Load persisted settings from options.conf (if it exists)
    // This must be done after languages and fonts are scanned
    menu_load_settings(menu);

    // Scan for available worlds
    menu_scan_worlds(menu);

    // Create/save options.conf file if it doesn't exist
    menu_save_settings(menu);

    return menu;
}

void menu_system_free(MenuSystem *menu) {
    if (!menu) {
        return;
    }
    if (menu->server_socket >= 0) {
        close(menu->server_socket);
        menu->server_socket = -1;
    }
    pthread_mutex_destroy(&menu->multiplayer_connect_mutex);
    if (menu->available_worlds) {
        free(menu->available_worlds);
    }
    if (menu->background_loaded) {
        UnloadTexture(menu->background_texture);
    }
    free(menu);
}

void menu_scan_worlds(MenuSystem *menu) {
    // Free previous list
    if (menu->server_socket >= 0) {
        close(menu->server_socket);
        menu->server_socket = -1;
    }
    if (menu->available_worlds) {
        free(menu->available_worlds);
        menu->available_worlds = NULL;
    }
    menu->world_count = 0;

    // Scan worlds directory
    DIR *dir = opendir("./worlds");
    if (!dir) {
        return;
    }

    // Count directories first
    int count = 0;
    struct dirent *entry;
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
    menu->available_worlds = (WorldInfo *)malloc(sizeof(WorldInfo) * count);
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

                FILE *metadata_file = fopen(metadata_path, "r");
                if (metadata_file) {
                    char line[256];
                    while (fgets(line, sizeof(line), metadata_file)) {
                        if (strncmp(line, "last_saved=", 11) == 0) {
                            char *value = line + 11;
                            int len = strlen(value);
                            if (value[len - 1] == '\n') {
                                len--;
                            }
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

void menu_draw_main(MenuSystem *menu, Font font) {
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
        // Fallback to solid diagnostic color if image not loaded (magenta for visibility)
        ClearBackground((Color){255, 0, 255, 255});
    }

    // Draw title
    const char *title = "B3DV";
    Vector2 title_size = MeasureTextEx(font, title, 80, 2);
    DrawTextExCustom(font, title,
                     (Vector2){(screen_width - title_size.x) / 2, 60},
                     80, 2, WHITE);

    // Draw version
    const char *version = "Basic 3D Visualizer - v0.0.25-beta";
    Vector2 version_size = MeasureTextEx(font, version, 24, 1);
    DrawTextExCustom(font, version,
                     (Vector2){(screen_width - version_size.x) / 2, 150},
                     24, 1, GRAY);
    if (menu->multiplayer_connected) {
        char status_text[512];
        snprintf(status_text, sizeof(status_text), "Connected to %s:%s", menu->server_address, menu->server_port);
        Vector2 status_size = MeasureTextEx(font, status_text, 20, 1);
        DrawTextExCustom(font, status_text,
                         (Vector2){(screen_width - status_size.x) / 2, 180},
                         20, 1, GREEN);
    }

    // Draw splash text (semi-diagonal, like Minecraft)
    if (strlen(menu->current_splash_text) > 0) {
        Vector2 splash_size = MeasureTextEx(font, menu->current_splash_text, 36, 1);
        float splash_x = (screen_width - title_size.x) / 2 + title_size.x + 30; // Right of title
        float splash_y = 210;
        float rotation = -20.0f; // 20 degrees rotation (negative for clockwise)

        // Draw with rotation
        DrawTextPro(font, menu->current_splash_text,
                    (Vector2){splash_x, splash_y},
                    (Vector2){0, splash_size.y / 2}, // Rotate around center height
                    rotation,
                    36, 1,
                    (Color){255, 255, 0, 200}); // Yellow with slight transparency
    }

    // Button dimensions
    int button_width = 400;
    int button_height = 60;
    int button_spacing = 20;
    int center_x = screen_width / 2;
    int center_y = screen_height / 2;

    // Play button (formerly "Select World")
    Rectangle world_button = {
        center_x - button_width / 2,
        center_y,
        button_width,
        button_height};

    // Multiplayer button
    Rectangle multiplayer_button = {
        center_x - button_width / 2,
        center_y + button_height + button_spacing,
        button_width,
        button_height};

    // Credits & Info button
    Rectangle credits_button = {
        center_x - button_width / 2,
        center_y + 2 * (button_height + button_spacing),
        button_width,
        button_height};

    // Settings button
    Rectangle settings_button = {
        center_x - button_width / 2,
        center_y + 3 * (button_height + button_spacing),
        button_width,
        button_height};

    // Quit button
    Rectangle quit_button = {
        center_x - button_width / 2,
        center_y + 4 * (button_height + button_spacing),
        button_width,
        button_height};

    // Get mouse position
    Vector2 mouse_pos = GetMousePosition();
    bool world_hover = CheckCollisionPointRec(mouse_pos, world_button);
    bool multiplayer_hover = CheckCollisionPointRec(mouse_pos, multiplayer_button);
    bool credits_hover = CheckCollisionPointRec(mouse_pos, credits_button);
    bool settings_hover = CheckCollisionPointRec(mouse_pos, settings_button);
    bool quit_hover = CheckCollisionPointRec(mouse_pos, quit_button);

    // Draw Play button
    DrawRectangleRec(world_button, world_hover ? LIGHTGRAY : (Color){60, 60, 60, 255});
    DrawRectangleLinesEx(world_button, 2, WHITE);
    Vector2 world_text_size = MeasureTextEx(font, menu->text_select_world, 32, 1);
    DrawTextExCustom(font, menu->text_select_world,
                     (Vector2){center_x - world_text_size.x / 2, center_y + 14},
                     32, 1, WHITE);

    // Draw Multiplayer button
    DrawRectangleRec(multiplayer_button, multiplayer_hover ? LIGHTGRAY : (Color){60, 60, 60, 255});
    DrawRectangleLinesEx(multiplayer_button, 2, WHITE);
    Vector2 multiplayer_text_size = MeasureTextEx(font, "Multiplayer", 32, 1);
    DrawTextExCustom(font, "Multiplayer",
                     (Vector2){center_x - multiplayer_text_size.x / 2, center_y + button_height + button_spacing + 14},
                     32, 1, WHITE);

    // Draw Credits & Info button
    DrawRectangleRec(credits_button, credits_hover ? LIGHTGRAY : (Color){60, 60, 60, 255});
    DrawRectangleLinesEx(credits_button, 2, WHITE);
    Vector2 credits_text_size = MeasureTextEx(font, menu->text_credits_info, 32, 1);
    DrawTextExCustom(font, menu->text_credits_info,
                     (Vector2){center_x - credits_text_size.x / 2, center_y + 2 * (button_height + button_spacing) + 14},
                     32, 1, BLACK);

    // Draw Settings button
    DrawRectangleRec(settings_button, settings_hover ? LIGHTGRAY : (Color){60, 60, 60, 255});
    DrawRectangleLinesEx(settings_button, 2, WHITE);
    Vector2 settings_text_size = MeasureTextEx(font, menu->game_text.settings, 32, 1);
    DrawTextExCustom(font, menu->game_text.settings,
                     (Vector2){center_x - settings_text_size.x / 2, center_y + 3 * (button_height + button_spacing) + 14},
                     32, 1, BLACK);

    // Draw Quit button
    DrawRectangleRec(quit_button, quit_hover ? LIGHTGRAY : (Color){60, 60, 60, 255});
    DrawRectangleLinesEx(quit_button, 2, WHITE);
    Vector2 quit_text_size = MeasureTextEx(font, menu->text_quit, 32, 1);
    DrawTextExCustom(font, menu->text_quit,
                     (Vector2){center_x - quit_text_size.x / 2, center_y + 4 * (button_height + button_spacing) + 14},
                     32, 1, BLACK);

    // Handle button clicks
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (world_hover) {
            menu_scan_worlds(menu); // Refresh world list
            menu->current_state = MENU_STATE_WORLD_SELECT;
        } else if (multiplayer_hover) {
            menu->current_state = MENU_STATE_MULTIPLAYER;
            strcpy(menu->server_address, "localhost");
            menu->server_address_len = (int)strlen(menu->server_address);
            strcpy(menu->server_port, "42069");
            menu->server_port_len = (int)strlen(menu->server_port);
            menu->multiplayer_active_field = 0;
            menu->multiplayer_error = false;
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
        (float)lang_button_height};

    Vector2 mouse_pos_lang = GetMousePosition();
    bool lang_hover = CheckCollisionPointRec(mouse_pos_lang, lang_button);

    // Draw language button
    DrawRectangleRec(lang_button, lang_hover ? LIGHTGRAY : (Color){60, 60, 60, 255});
    DrawRectangleLinesEx(lang_button, 2, WHITE);

    Vector2 lang_text_size = MeasureTextEx(font, menu->current_language, 24, 1);
    DrawTextExCustom(font, menu->current_language,
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

static bool set_socket_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        return false;
    }
    return true;
}

static int menu_try_connect_server(const char *host, const char *port, char *error_msg, size_t error_msg_size);

typedef struct {
    MenuSystem *menu;
    char host[256];
    char port[16];
} MenuConnectRequest;

static void *menu_connect_thread(void *arg) {
    MenuConnectRequest *request = (MenuConnectRequest *)arg;
    MenuSystem *menu = request->menu;
    char error_msg[256] = {0};
    int sock = menu_try_connect_server(request->host, request->port, error_msg, sizeof(error_msg));

    pthread_mutex_lock(&menu->multiplayer_connect_mutex);
    if (!menu->multiplayer_connecting) {
        if (sock >= 0) {
            close(sock);
        }
    } else if (sock < 0) {
        menu->multiplayer_error = true;
        strncpy(menu->multiplayer_error_msg, error_msg, sizeof(menu->multiplayer_error_msg) - 1);
        menu->multiplayer_error_msg[sizeof(menu->multiplayer_error_msg) - 1] = '\0';
        menu->server_socket = -1;
        menu->multiplayer_connecting = false;
    } else {
        menu->server_socket = sock;
        menu->multiplayer_error_msg[0] = '\0';
        menu->multiplayer_handshake_buffer[0] = '\0';
        menu->multiplayer_handshake_used = 0;
    }
    menu->multiplayer_connect_thread_active = false;
    pthread_mutex_unlock(&menu->multiplayer_connect_mutex);
    free(request);
    return NULL;
}

static int menu_try_connect_server(const char *host, const char *port, char *error_msg, size_t error_msg_size) {
    struct addrinfo hints = {0};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *result = NULL;
    int gai_err = getaddrinfo(host, port, &hints, &result);
    if (gai_err != 0) {
        snprintf(error_msg, error_msg_size, "Resolve failed: %s", gai_strerror(gai_err));
        return -1;
    }

    int sock = -1;
    int last_error = 0;
    for (struct addrinfo *ai = result; ai != NULL; ai = ai->ai_next) {
        int candidate = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (candidate < 0) {
            last_error = errno;
            continue;
        }

        if (!set_socket_nonblocking(candidate)) {
            last_error = errno;
            close(candidate);
            continue;
        }

        int connect_rc = connect(candidate, ai->ai_addr, ai->ai_addrlen);
        if (connect_rc < 0 && errno != EINPROGRESS && errno != EWOULDBLOCK) {
            last_error = errno;
            close(candidate);
            continue;
        }

        sock = candidate;
        break;
    }

    freeaddrinfo(result);

    if (sock < 0) {
        if (last_error != 0) {
            snprintf(error_msg, error_msg_size, "%s", strerror(last_error));
        } else {
            snprintf(error_msg, error_msg_size, "Connection failed");
        }
        return -1;
    }

    return sock;
}

static int menu_poll_server_connect(int sock, char *error_msg, size_t error_msg_size) {
    if (sock < 0) {
        snprintf(error_msg, error_msg_size, "No socket available");
        return -1;
    }

    fd_set write_fds;
    FD_ZERO(&write_fds);
    FD_SET(sock, &write_fds);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    int select_rc = select(sock + 1, NULL, &write_fds, NULL, &timeout);
    if (select_rc < 0) {
        snprintf(error_msg, error_msg_size, "%s", strerror(errno));
        return -1;
    }
    if (select_rc == 0) {
        return 0;
    }

    int socket_error = 0;
    socklen_t len = sizeof(socket_error);
    if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &socket_error, &len) < 0) {
        snprintf(error_msg, error_msg_size, "%s", strerror(errno));
        return -1;
    }
    if (socket_error != 0) {
        snprintf(error_msg, error_msg_size, "%s", strerror(socket_error));
        return -1;
    }

    return 1;
}

static int menu_receive_server_welcome(int sock, char *world_name, size_t world_name_size, char *buffer, size_t buffer_size, size_t *buffer_used, char *error_msg, size_t error_msg_size) {
    if (*buffer_used >= buffer_size - 1) {
        snprintf(error_msg, error_msg_size, "Handshake buffer full");
        return -1;
    }

    ssize_t bytes = recv(sock, buffer + *buffer_used, buffer_size - *buffer_used - 1, MSG_DONTWAIT);
    if (bytes > 0) {
        *buffer_used += (size_t)bytes;
    } else if (bytes == 0) {
        snprintf(error_msg, error_msg_size, "Handshake failed");
        return -1;
    } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
        snprintf(error_msg, error_msg_size, "%s", strerror(errno));
        return -1;
    } else {
        return 0;
    }

    buffer[*buffer_used] = '\0';
    char *line_end = strchr(buffer, '\n');
    if (!line_end) {
        return 0;
    }

    size_t line_len = (size_t)(line_end - buffer);
    if (line_len >= 512) {
        line_len = 511;
    }

    char line[512] = {0};
    memcpy(line, buffer, line_len);
    line[line_len] = '\0';
    if (line_len > 0 && line[line_len - 1] == '\r') {
        line[line_len - 1] = '\0';
    }

    if (strncmp(line, "WELCOME ", 8) != 0) {
        snprintf(error_msg, error_msg_size, "Unexpected server response");
        return -1;
    }

    const char *world = line + 8;
    if (world[0] == '\0') {
        snprintf(error_msg, error_msg_size, "Server did not provide a world name");
        return -1;
    }

    strncpy(world_name, world, world_name_size - 1);
    world_name[world_name_size - 1] = '\0';
    *buffer_used = 0;
    buffer[0] = '\0';
    return 1;
}

void menu_draw_multiplayer(MenuSystem *menu, Font font) {
    int screen_width = GetScreenWidth();

    ClearBackground((Color){20, 20, 20, 255});

    const char *title = "Join Multiplayer";
    Vector2 title_size = MeasureTextEx(font, title, 64, 2);
    DrawTextExCustom(font, title,
                     (Vector2){(screen_width - title_size.x) / 2, 40},
                     64, 2, WHITE);

    const char *subtitle = "Enter server address and port, then press Connect.";
    Vector2 subtitle_size = MeasureTextEx(font, subtitle, 24, 1);
    DrawTextExCustom(font, subtitle,
                     (Vector2){(screen_width - subtitle_size.x) / 2, 120},
                     24, 1, GRAY);

    int box_width = 600;
    int box_height = 50;
    int start_x = (screen_width - box_width) / 2;
    int address_y = 160;
    int port_y = address_y + box_height + 100;

    Rectangle address_box = {(float)start_x, (float)address_y, (float)box_width, (float)box_height};
    Rectangle port_box = {(float)start_x, (float)port_y, 220, (float)box_height};

    Vector2 mouse_pos = GetMousePosition();
    bool address_hover = CheckCollisionPointRec(mouse_pos, address_box);
    bool port_hover = CheckCollisionPointRec(mouse_pos, port_box);

    // Draw address label and box
    DrawTextExCustom(font, "Domain or IP:", (Vector2){(float)start_x, address_y - 35}, 24, 1, WHITE);
    DrawRectangleRec(address_box, menu->multiplayer_active_field == 0 ? (Color){80, 80, 120, 255} : (Color){60, 60, 60, 255});
    DrawRectangleLinesEx(address_box, 2, address_hover ? YELLOW : WHITE);
    DrawTextExCustom(font,
                     menu->server_address_len > 0 ? menu->server_address : "Enter domain or IP",
                     (Vector2){address_box.x + 10, address_box.y + 12},
                     24, 1, menu->server_address_len > 0 ? WHITE : GRAY);

    // Draw port label and box
    DrawTextExCustom(font, "Port:", (Vector2){(float)start_x, port_y - 35}, 24, 1, WHITE);
    DrawRectangleRec(port_box, menu->multiplayer_active_field == 1 ? (Color){80, 80, 120, 255} : (Color){60, 60, 60, 255});
    DrawRectangleLinesEx(port_box, 2, port_hover ? YELLOW : WHITE);
    DrawTextExCustom(font,
                     menu->server_port_len > 0 ? menu->server_port : "42069",
                     (Vector2){port_box.x + 10, port_box.y + 12},
                     24, 1, menu->server_port_len > 0 ? WHITE : GRAY);

    if (menu->multiplayer_active_field == 0) {
        DrawRectangleLinesEx(address_box, 3, YELLOW);
    } else if (menu->multiplayer_active_field == 1) {
        DrawRectangleLinesEx(port_box, 3, YELLOW);
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (address_hover) {
            menu->multiplayer_active_field = 0;
        } else if (port_hover) {
            menu->multiplayer_active_field = 1;
        }
    }

    if (IsKeyPressed(KEY_TAB)) {
        menu->multiplayer_active_field = 1 - menu->multiplayer_active_field;
    }

    int key = GetCharPressed();
    while (key > 0) {
        if (key >= 32 && key <= 126) {
            if (menu->multiplayer_active_field == 0) {
                if (menu->server_address_len < (int)sizeof(menu->server_address) - 1 && key != ' ') {
                    menu->server_address[menu->server_address_len++] = (char)key;
                    menu->server_address[menu->server_address_len] = '\0';
                }
            } else {
                if (menu->server_port_len < (int)sizeof(menu->server_port) - 1 && key >= '0' && key <= '9') {
                    menu->server_port[menu->server_port_len++] = (char)key;
                    menu->server_port[menu->server_port_len] = '\0';
                }
            }
        }
        key = GetCharPressed();
    }

    if (IsKeyPressed(KEY_BACKSPACE)) {
        if (menu->multiplayer_active_field == 0 && menu->server_address_len > 0) {
            menu->server_address_len--;
            menu->server_address[menu->server_address_len] = '\0';
        } else if (menu->multiplayer_active_field == 1 && menu->server_port_len > 0) {
            menu->server_port_len--;
            menu->server_port[menu->server_port_len] = '\0';
        }
    }

    // Buttons
    int button_width = 180;
    int button_height = 50;
    int button_y = port_y + box_height + 80;
    Rectangle connect_button = {(float)start_x, (float)button_y, (float)button_width, (float)button_height};
    Rectangle back_button = {(float)(start_x + box_width - button_width), (float)button_y, (float)button_width, (float)button_height};
    bool connect_hover = CheckCollisionPointRec(mouse_pos, connect_button);
    bool back_hover = CheckCollisionPointRec(mouse_pos, back_button);

    DrawRectangleRec(connect_button, connect_hover ? LIGHTGRAY : (Color){60, 60, 60, 255});
    DrawRectangleLinesEx(connect_button, 2, WHITE);
    Vector2 connect_text_size = MeasureTextEx(font, "Connect", 28, 1);
    DrawTextExCustom(font, "Connect",
                     (Vector2){connect_button.x + (button_width - connect_text_size.x) / 2, connect_button.y + 10},
                     28, 1, BLACK);

    DrawRectangleRec(back_button, back_hover ? LIGHTGRAY : (Color){60, 60, 60, 255});
    DrawRectangleLinesEx(back_button, 2, WHITE);
    Vector2 back_text_size = MeasureTextEx(font, menu->text_back, 28, 1);
    DrawTextExCustom(font, menu->text_back,
                     (Vector2){back_button.x + (button_width - back_text_size.x) / 2, back_button.y + 10},
                     28, 1, BLACK);

    bool connect_pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && connect_hover;
    if (IsKeyPressed(KEY_ENTER)) {
        connect_pressed = true;
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && back_hover) {
        menu->current_state = MENU_STATE_MAIN;
        menu->multiplayer_error = false;
    }

    if (connect_pressed) {
        menu->multiplayer_error = false;
        if (menu->server_address_len == 0) {
            menu->multiplayer_error = true;
            snprintf(menu->multiplayer_error_msg, sizeof(menu->multiplayer_error_msg), "Server address cannot be empty");
        } else if (menu->server_port_len == 0) {
            menu->multiplayer_error = true;
            snprintf(menu->multiplayer_error_msg, sizeof(menu->multiplayer_error_msg), "Port cannot be empty");
        } else {
            int port_value = atoi(menu->server_port);
            if (port_value <= 0 || port_value > 65535) {
                menu->multiplayer_error = true;
                snprintf(menu->multiplayer_error_msg, sizeof(menu->multiplayer_error_msg), "Port must be 1-65535");
            } else {
                if (menu->server_socket >= 0) {
                    close(menu->server_socket);
                    menu->server_socket = -1;
                    menu->multiplayer_connected = false;
                    menu->multiplayer_client = false;
                }

                if (!menu->multiplayer_connect_thread_active) {
                    MenuConnectRequest *request = malloc(sizeof(MenuConnectRequest));
                    if (request) {
                        request->menu = menu;
                        strncpy(request->host, menu->server_address, sizeof(request->host) - 1);
                        request->host[sizeof(request->host) - 1] = '\0';
                        strncpy(request->port, menu->server_port, sizeof(request->port) - 1);
                        request->port[sizeof(request->port) - 1] = '\0';
                        menu->multiplayer_connecting = true;
                        menu->multiplayer_error = false;
                        menu->server_socket = -1;
                        menu->multiplayer_connect_thread_active = true;
                        int create_result = pthread_create(&menu->multiplayer_connect_thread, NULL, menu_connect_thread, request);
                        if (create_result != 0) {
                            menu->multiplayer_connect_thread_active = false;
                            menu->multiplayer_connecting = false;
                            menu->multiplayer_error = true;
                            snprintf(menu->multiplayer_error_msg, sizeof(menu->multiplayer_error_msg), "Thread create failed: %s", strerror(create_result));
                            free(request);
                        }
                    } else {
                        menu->multiplayer_error = true;
                        snprintf(menu->multiplayer_error_msg, sizeof(menu->multiplayer_error_msg), "Internal allocation failed");
                    }
                }
            }
        }
    }

    if (menu->multiplayer_connecting && menu->server_socket >= 0) {
        int connect_state = menu_poll_server_connect(menu->server_socket, menu->multiplayer_error_msg, sizeof(menu->multiplayer_error_msg));
        if (connect_state < 0) {
            close(menu->server_socket);
            menu->server_socket = -1;
            menu->multiplayer_connecting = false;
            menu->multiplayer_error = true;
        } else if (connect_state > 0) {
            char world_name[256] = {0};
            int welcome_state = menu_receive_server_welcome(menu->server_socket,
                                                            world_name,
                                                            sizeof(world_name),
                                                            menu->multiplayer_handshake_buffer,
                                                            sizeof(menu->multiplayer_handshake_buffer),
                                                            &menu->multiplayer_handshake_used,
                                                            menu->multiplayer_error_msg,
                                                            sizeof(menu->multiplayer_error_msg));
            if (welcome_state < 0) {
                close(menu->server_socket);
                menu->server_socket = -1;
                menu->multiplayer_connecting = false;
                menu->multiplayer_error = true;
            } else if (welcome_state > 0) {
                menu->multiplayer_connecting = false;
                menu->multiplayer_connected = true;
                menu->multiplayer_client = true;
                strncpy(menu->server_world_name, world_name, sizeof(menu->server_world_name) - 1);
                menu->server_world_name[sizeof(menu->server_world_name) - 1] = '\0';
                strncpy(menu->selected_world_name, world_name, sizeof(menu->selected_world_name) - 1);
                menu->selected_world_name[sizeof(menu->selected_world_name) - 1] = '\0';
                menu->should_start_game = true;
                menu->current_state = MENU_STATE_GAME;
            }
        }
    }

    if (menu->multiplayer_error) {
        Vector2 error_size = MeasureTextEx(font, menu->multiplayer_error_msg, 24, 1);
        DrawTextExCustom(font, menu->multiplayer_error_msg,
                         (Vector2){(screen_width - error_size.x) / 2, button_y + button_height + 30},
                         24, 1, RED);
    }
}

void menu_draw_world_select(MenuSystem *menu, Font font) {
    int screen_width = GetScreenWidth();
    int screen_height = GetScreenHeight();

    // Clear background
    ClearBackground((Color){20, 20, 20, 255});

    // Draw title
    Vector2 title_size = MeasureTextEx(font, menu->text_title_select_world, 64, 2);
    DrawTextExCustom(font, menu->text_title_select_world,
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
            (float)item_height};

        bool is_hovered = CheckCollisionPointRec(mouse_pos, item_rect);
        bool is_selected = (i == menu->selected_world_index);

        // Draw item background
        Color bg_color = is_selected  ? (Color){80, 120, 200, 255}
                         : is_hovered ? (Color){100, 100, 100, 255}
                                      : (Color){60, 60, 60, 255};
        DrawRectangleRec(item_rect, bg_color);
        DrawRectangleLinesEx(item_rect, 2, WHITE);

        // Draw world name and metadata
        DrawTextExCustom(font, menu->available_worlds[i].name,
                         (Vector2){item_rect.x + 10, item_rect.y + 5},
                         24, 1, WHITE);

        // Draw metadata on second line
        char metadata[256];
        snprintf(metadata, sizeof(metadata), menu->text_last,
                 menu->available_worlds[i].created, menu->available_worlds[i].chunk_count);
        DrawTextExCustom(font, metadata,
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

    // Create World button (left)
    Rectangle create_button = {
        (float)list_x,
        (float)button_y,
        (float)button_width,
        (float)button_height};

    // Back button (right)
    Rectangle back_button = {
        (float)(list_x + list_width - button_width),
        (float)button_y,
        (float)button_width,
        (float)button_height};

    bool create_hover = CheckCollisionPointRec(mouse_pos, create_button);
    bool back_hover = CheckCollisionPointRec(mouse_pos, back_button);

    // Draw Create World button
    DrawRectangleRec(create_button, create_hover ? LIGHTGRAY : (Color){60, 60, 60, 255});
    DrawRectangleLinesEx(create_button, 2, WHITE);
    Vector2 create_text_size = MeasureTextEx(font, menu->text_create_world, 28, 1);
    DrawTextExCustom(font, menu->text_create_world,
                     (Vector2){create_button.x + (button_width - create_text_size.x) / 2, create_button.y + 10},
                     28, 1, BLACK);

    // Draw Back button
    DrawRectangleRec(back_button, back_hover ? LIGHTGRAY : (Color){60, 60, 60, 255});
    DrawRectangleLinesEx(back_button, 2, WHITE);
    Vector2 back_text_size = MeasureTextEx(font, menu->text_back, 28, 1);
    DrawTextExCustom(font, menu->text_back,
                     (Vector2){back_button.x + (button_width - back_text_size.x) / 2, back_button.y + 10},
                     28, 1, BLACK);

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (create_hover) {
            menu->current_state = MENU_STATE_CREATE_WORLD;
            strcpy(menu->new_world_name, "");
            menu->new_world_name_len = 0;
            menu->create_world_error = false;
            menu->create_world_compress = true;
        } else if (back_hover) {
            menu->current_state = MENU_STATE_MAIN;
        }
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
        DrawTextExCustom(font, menu->text_no_worlds,
                         (Vector2){(screen_width - no_worlds_size.x) / 2, screen_height / 2},
                         32, 1, GRAY);
    }
}

void menu_update_input(MenuSystem *menu) {
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
    // ESC key cancels multiplayer join
    if (menu->current_state == MENU_STATE_MULTIPLAYER && IsKeyPressed(KEY_ESCAPE)) {
        menu->current_state = MENU_STATE_MAIN;
        menu->multiplayer_error = false;
    }
}

void menu_draw_create_world(MenuSystem *menu, Font font) {
    int screen_width = GetScreenWidth();
    int screen_height = GetScreenHeight();

    // Clear background
    ClearBackground((Color){20, 20, 20, 255});

    // Draw title
    Vector2 title_size = MeasureTextEx(font, menu->text_title_create_world, 64, 2);
    DrawTextExCustom(font, menu->text_title_create_world,
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
        (float)input_height};

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
    DrawTextExCustom(font, menu->new_world_name,
                     (Vector2){input_x + 10, input_y + 10},
                     32, 1, WHITE);

    // Draw blinking cursor
    if ((int)(GetTime() * 2) % 2 == 0) {
        Vector2 cursor_pos = MeasureTextEx(font, menu->new_world_name, 32, 1);
        DrawLineEx((Vector2){input_x + 10 + cursor_pos.x, input_y + 10},
                   (Vector2){input_x + 10 + cursor_pos.x, input_y + 40}, 2, WHITE);
    }

    // Draw label
    DrawTextExCustom(font, menu->text_world_name_label,
                     (Vector2){input_x, input_y - 40},
                     20, 1, GRAY);

    // Compression checkbox
    int checkbox_size = 24;
    int checkbox_x = input_x;
    int checkbox_y = input_y + input_height + 20;
    Rectangle compression_checkbox = {
        (float)checkbox_x,
        (float)checkbox_y,
        (float)checkbox_size,
        (float)checkbox_size};
    Rectangle compression_checkbox_area = {
        compression_checkbox.x,
        compression_checkbox.y,
        compression_checkbox.width + 400,
        compression_checkbox.height};
    DrawRectangleRec(compression_checkbox, (Color){60, 60, 60, 255});
    DrawRectangleLinesEx(compression_checkbox, 2, WHITE);
    if (menu->create_world_compress) {
        DrawRectangleRec((Rectangle){compression_checkbox.x + 4, compression_checkbox.y + 4, compression_checkbox.width - 8, compression_checkbox.height - 8}, WHITE);
    }
    DrawTextExCustom(font, menu->text_compress_world_files,
                     (Vector2){checkbox_x + checkbox_size + 10, checkbox_y + 2},
                     20, 1, WHITE);

    // Buttons
    int button_width = 150;
    int button_height = 50;
    int button_spacing = 20;
    int buttons_y = screen_height / 2 + 140;

    Rectangle create_btn = {
        (float)(input_x + input_width / 2 - button_width - button_spacing / 2),
        (float)buttons_y,
        (float)button_width,
        (float)button_height};

    Rectangle cancel_btn = {
        (float)(input_x + input_width / 2 + button_spacing / 2),
        (float)buttons_y,
        (float)button_width,
        (float)button_height};

    Vector2 mouse_pos = GetMousePosition();
    bool create_hover = CheckCollisionPointRec(mouse_pos, create_btn);
    bool cancel_hover = CheckCollisionPointRec(mouse_pos, cancel_btn);

    // Draw Create button
    DrawRectangleRec(create_btn, create_hover ? LIGHTGRAY : (Color){60, 60, 60, 255});
    DrawRectangleLinesEx(create_btn, 2, WHITE);
    Vector2 create_text_size = MeasureTextEx(font, menu->text_create_btn, 28, 1);
    DrawTextExCustom(font, menu->text_create_btn,
                     (Vector2){create_btn.x + (button_width - create_text_size.x) / 2, create_btn.y + 10},
                     28, 1, BLACK);

    // Draw Cancel button
    DrawRectangleRec(cancel_btn, cancel_hover ? LIGHTGRAY : (Color){60, 60, 60, 255});
    DrawRectangleLinesEx(cancel_btn, 2, WHITE);
    Vector2 cancel_text_size = MeasureTextEx(font, menu->text_cancel_btn, 28, 1);
    DrawTextExCustom(font, menu->text_cancel_btn,
                     (Vector2){cancel_btn.x + (button_width - cancel_text_size.x) / 2, cancel_btn.y + 10},
                     28, 1, BLACK);

    // Draw error message if any
    if (menu->create_world_error) {
        Vector2 error_size = MeasureTextEx(font, menu->create_world_error_msg, 24, 1);
        DrawTextExCustom(font, menu->create_world_error_msg,
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
        } else if (CheckCollisionPointRec(mouse_pos, compression_checkbox_area)) {
            menu->create_world_compress = !menu->create_world_compress;
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

void menu_draw_settings(MenuSystem *menu, Font font) {
    int screen_width = GetScreenWidth();

    // Clear background
    ClearBackground((Color){20, 20, 20, 255});

    // Draw title
    Vector2 title_size = MeasureTextEx(font, menu->game_text.settings, 64, 2);
    DrawTextExCustom(font, menu->game_text.settings,
                     (Vector2){(screen_width - title_size.x) / 2, 40},
                     64, 2, WHITE);

    // Settings panel - bigger so all options fit without feeling cramped
    int panel_width = screen_width - 200;
    if (panel_width > 720) {
        panel_width = 720;
    }
    int panel_height = 860;
    int panel_x = (screen_width - panel_width) / 2;
    int panel_y = 100;

    DrawRectangle(panel_x - 10, panel_y - 10, panel_width + 20, panel_height + 20, (Color){40, 40, 40, 255});
    DrawRectangleLines(panel_x - 10, panel_y - 10, panel_width + 20, panel_height + 20, WHITE);

    // Render Distance slider
    int slider_y = panel_y + 30;
    int slider_x = panel_x + 50;
    int slider_width = 500;
    int slider_height = 20;

    // Draw label
    DrawTextExCustom(font, menu->game_text.render_dist_label, (Vector2){panel_x + 30, slider_y - 35}, 28, 1, WHITE);

    // Draw value
    char render_dist_str[32];
    snprintf(render_dist_str, sizeof(render_dist_str), "%.0f", menu->render_distance);
    DrawTextExCustom(font, render_dist_str, (Vector2){panel_x + 500, slider_y - 35}, 28, 1, GRAY);

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
    DrawTextExCustom(font, menu->game_text.max_fps_label, (Vector2){panel_x + 30, fps_slider_y - 35}, 28, 1, WHITE);

    // Draw value
    char fps_str[32];
    if (menu->max_fps == 0) {
        snprintf(fps_str, sizeof(fps_str), "%s", menu->game_text.uncapped);
    } else {
        snprintf(fps_str, sizeof(fps_str), "%d", menu->max_fps);
    }
    DrawTextExCustom(font, fps_str, (Vector2){panel_x + 500, fps_slider_y - 35}, 28, 1, GRAY);

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
    // TODO: localize
    DrawTextExCustom(font, menu->game_text.nickname_label, (Vector2){panel_x + 30, nickname_y - 35}, 24, 1, WHITE);
    DrawRectangleRec(nickname_box, (Color){60, 60, 60, 255});
    DrawRectangleLinesEx(nickname_box, 2, menu->nickname_edit_active ? YELLOW : WHITE);
    DrawTextExCustom(font, menu->nickname,
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

    // Draw blinking cursor when active
    if (menu->nickname_edit_active && (int)(GetTime() * 2) % 2 == 0) {
        Vector2 cursor_pos = MeasureTextEx(font, menu->nickname, 24, 1);
        DrawLineEx((Vector2){nickname_box.x + 10 + cursor_pos.x, nickname_box.y + 8},
                   (Vector2){nickname_box.x + 10 + cursor_pos.x, nickname_box.y + nickname_box_height - 8},
                   2, WHITE);
    }

    /*// Cloud render distance slider
    int cloud_dist_y = nickname_y + 90;

    // Draw label
    DrawTextExCustom(font, menu->game_text.cloud_distance_label, (Vector2){panel_x + 30, cloud_dist_y - 35}, 24, 1, WHITE);

    // Draw value
    char cloud_dist_str[32];
    snprintf(cloud_dist_str, sizeof(cloud_dist_str), "%.0f", menu->clouds_render_distance);
    DrawTextExCustom(font, cloud_dist_str, (Vector2){panel_x + 500, cloud_dist_y - 35}, 24, 1, GRAY);

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

    DrawTextExCustom(font, menu->game_text.clouds_enabled_label, (Vector2){checkbox_x + checkbox_size + 15, cloud_toggle_y + 3}, 24, 1, WHITE);

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse_pos, cloud_checkbox)) {
        menu->clouds_enabled = !menu->clouds_enabled;
        menu_save_settings(menu);
    }*/

    // Compass HUD toggle checkbox
    // int compass_toggle_y = cloud_toggle_y + 60;
    int compass_toggle_y = nickname_y + 90; // Adjust position since cloud UI is now disabled
    int checkbox_size = 30;                 // Define for compass checkbox
    int checkbox_x = panel_x + 50;          // Define for compass checkbox
    Rectangle compass_checkbox = {(float)checkbox_x, (float)compass_toggle_y, (float)checkbox_size, (float)checkbox_size};

    DrawRectangleRec(compass_checkbox, (Color){60, 60, 60, 255});
    DrawRectangleLinesEx(compass_checkbox, 2, WHITE);
    if (menu->compass_enabled) {
        DrawRectangle(checkbox_x + 5, compass_toggle_y + 5, checkbox_size - 10, checkbox_size - 10, (Color){100, 200, 100, 255});
    }
    // TODO: localize
    DrawTextExCustom(font, menu->game_text.compass_hud_label, (Vector2){checkbox_x + checkbox_size + 15, compass_toggle_y + 3}, 24, 1, WHITE);

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse_pos, compass_checkbox)) {
        menu->compass_enabled = !menu->compass_enabled;
        menu_save_settings(menu);
    }

    // Font selection
    int font_y = nickname_y + 150; // Adjust position since cloud UI is now disabled

    // Draw label
    DrawTextExCustom(font, menu->game_text.font_family_label, (Vector2){panel_x + 30, font_y - 35}, 24, 1, WHITE);

    // Previous/Next buttons for font family selection
    int button_small_width = 35;
    int button_small_height = 35;
    int prev_button_x = panel_x + 50;
    int next_button_x = panel_x + 500;
    int font_display_y = font_y - 1;

    Rectangle prev_button = {
        (float)prev_button_x,
        (float)font_display_y,
        (float)button_small_width,
        (float)button_small_height};

    Rectangle next_button = {
        (float)next_button_x,
        (float)font_display_y,
        (float)button_small_width,
        (float)button_small_height};

    bool prev_hover = CheckCollisionPointRec(mouse_pos, prev_button);
    bool next_hover = CheckCollisionPointRec(mouse_pos, next_button);

    // Draw previous button
    DrawRectangleRec(prev_button, prev_hover ? LIGHTGRAY : (Color){60, 60, 60, 255});
    DrawRectangleLinesEx(prev_button, 2, WHITE);
    DrawTextExCustom(font, "<", (Vector2){prev_button_x + 7, font_display_y + 3}, 24, 1, BLACK);

    // Draw next button
    DrawRectangleRec(next_button, next_hover ? LIGHTGRAY : (Color){60, 60, 60, 255});
    DrawRectangleLinesEx(next_button, 2, WHITE);
    DrawTextExCustom(font, ">", (Vector2){next_button_x + 9, font_display_y + 3}, 24, 1, BLACK);

    // Draw current font family name in the middle
    Vector2 family_name_size = MeasureTextEx(font, menu->font_families[menu->current_font_family_index], 22, 1);
    DrawTextExCustom(font, menu->font_families[menu->current_font_family_index],
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
    int variant_y = font_display_y + 67;
    DrawTextExCustom(font, menu->game_text.font_variant_label, (Vector2){panel_x + 30, variant_y - 25}, 24, 1, WHITE);

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
    char *ext = strrchr(variant_display, '.');
    if (ext) {
        *ext = '\0';
    }

    DrawTextExCustom(font, variant_display,
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
        (float)variant_button_height};

    Rectangle variant_down_button = {
        (float)variant_down_x,
        (float)variant_down_y,
        (float)variant_button_width,
        (float)variant_button_height};

    bool variant_up_hover = CheckCollisionPointRec(mouse_pos, variant_up_button);
    bool variant_down_hover = CheckCollisionPointRec(mouse_pos, variant_down_button);

    // Draw up button
    DrawRectangleRec(variant_up_button, variant_up_hover ? LIGHTGRAY : (Color){60, 60, 60, 255});
    DrawRectangleLinesEx(variant_up_button, 2, WHITE);
    DrawTextExCustom(font, "^", (Vector2){variant_up_x + 6, variant_up_y + 2}, 20, 1, BLACK);

    // Draw down button
    DrawRectangleRec(variant_down_button, variant_down_hover ? LIGHTGRAY : (Color){60, 60, 60, 255});
    DrawRectangleLinesEx(variant_down_button, 2, WHITE);
    DrawTextExCustom(font, "v", (Vector2){variant_down_x + 6, variant_down_y + 2}, 20, 1, BLACK);

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
        (float)button_height};

    bool back_hover = CheckCollisionPointRec(mouse_pos, back_button);
    DrawRectangleRec(back_button, back_hover ? LIGHTGRAY : (Color){60, 60, 60, 255});
    DrawRectangleLinesEx(back_button, 2, WHITE);
    Vector2 back_text_size = MeasureTextEx(font, menu->text_back, 28, 1);
    DrawTextExCustom(font, menu->text_back,
                     (Vector2){back_button.x + (button_width - back_text_size.x) / 2, back_button.y + 10},
                     28, 1, BLACK);

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && back_hover) {
        menu->current_state = MENU_STATE_MAIN;
    }
}

void menu_draw_credits(MenuSystem *menu, Font font) {
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

    // Draw the credits text directly
    DrawTextExCustom(font, menu->credits_text,
                     (Vector2){text_x, text_y},
                     font_size, spacing, WHITE);

    // Draw "See full info" button
    int button_width = 200;
    int button_height = 50;
    int button_x = (screen_width - button_width) / 2;
    int button_y = screen_height - 110;

    Rectangle info_btn = {
        (float)button_x,
        (float)button_y,
        (float)button_width,
        (float)button_height};

    Vector2 mouse_pos = GetMousePosition();
    bool info_hover = CheckCollisionPointRec(mouse_pos, info_btn);

    // Draw button
    DrawRectangleRec(info_btn, info_hover ? LIGHTGRAY : (Color){60, 60, 60, 255});
    DrawRectangleLinesEx(info_btn, 2, WHITE);
    Vector2 btn_text_size = MeasureTextEx(font, menu->game_text.see_full_info, 24, 1);
    DrawTextExCustom(font, menu->game_text.see_full_info,
                     (Vector2){button_x + (button_width - (int)btn_text_size.x) / 2, button_y + 12},
                     24, 1, BLACK);

    // Handle button click - construct absolute path to info.html
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && info_hover) {
        const char *app_dir = GetApplicationDirectory();
        char info_path[1024];

// Build URL with proper platform-specific formatting
#ifdef _WIN32
        // Windows: file:///C:/path/to/file
        snprintf(info_path, sizeof(info_path), "file:///%s/assets/info.html", app_dir);
        // Replace backslashes with forward slashes
        for (int i = 0; info_path[i]; i++) {
            if (info_path[i] == '\\')
                info_path[i] = '/';
        }
#else
        // Unix/Linux/Mac: file:///path/to/file
        snprintf(info_path, sizeof(info_path), "file://%s/assets/info.html", app_dir);
#endif

        OpenURL(info_path);
    }

    // Draw instructions
    Vector2 instr_size = MeasureTextEx(font, menu->game_text.press_esc_to_return, 18, 1);
    DrawTextExCustom(font, menu->game_text.press_esc_to_return,
                     (Vector2){(screen_width - instr_size.x) / 2, screen_height - 40},
                     18, 1, GRAY);
}