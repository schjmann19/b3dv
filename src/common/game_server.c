#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include "../../common_utils/simple_strings.h"
#include "../../include/console.h"
#include "../../include/world.h"
#include "../../include/game_server.h"
static void game_server_apply_command(GameServer *srv,
                                      Player *player,
                                      const ConsoleCommand *cmd,
                                      const char *raw_input,
                                      bool *should_quit,
                                      bool *flight_enabled,
                                      bool *show_chunk_borders,
                                      char *out_msg,
                                      size_t out_size) {
    if (!srv || !srv->world || !player || !cmd) {
        if (out_msg && out_size > 0) {
            out_msg[0] = '\0';
        }
        return;
    }

    World *world = srv->world;
    switch (cmd->type) {
    case CMD_QUIT:
        if (should_quit) {
            *should_quit = true;
        }
        if (out_msg && out_size > 0) {
            snprintf(out_msg, out_size, "Quitting...");
        }
        break;

    case CMD_CHAT:
        if (out_msg && out_size > 0) {
            snprintf(out_msg, out_size, "%s", cmd->args[0] != '\0' ? cmd->args : raw_input);
        }
        break;

    case CMD_TP: {
        float x = 0.0f, y = 0.0f, z = 0.0f;
        if (sscanf(cmd->args, "%f %f %f", &x, &y, &z) == 3) {
            player->position = (Vector3){x, y, z};
            player->velocity = (Vector3){0, 0, 0};
            if (out_msg && out_size > 0) {
                snprintf(out_msg, out_size, "Teleported to (%.1f, %.1f, %.1f)", x, y, z);
            }
        } else if (out_msg && out_size > 0) {
            snprintf(out_msg, out_size, "Usage: /tp <x> <y> <z>");
        }
        break;
    }

    case CMD_FLY: {
        char action[32] = {0};
        if (sscanf(cmd->args, "%31s", action) == 1) {
            if (strcasecmp(action, "enable") == 0 || strcasecmp(action, "on") == 0) {
                if (flight_enabled) {
                    *flight_enabled = true;
                    player->is_flying = true;
                    if (out_msg && out_size > 0) {
                        snprintf(out_msg, out_size, "Flight enabled");
                    }
                }
            } else if (strcasecmp(action, "disable") == 0 || strcasecmp(action, "off") == 0) {
                if (flight_enabled) {
                    *flight_enabled = false;
                    player->is_flying = false;
                    if (out_msg && out_size > 0) {
                        snprintf(out_msg, out_size, "Flight disabled");
                    }
                }
            } else if (out_msg && out_size > 0) {
                snprintf(out_msg, out_size, "Usage: /fly <enable|disable>");
            }
        } else if (out_msg && out_size > 0) {
            snprintf(out_msg, out_size, "Usage: /fly <enable|disable>");
        }
        break;
    }

    case CMD_NOCLIP: {
        char action[32] = {0};
        if (sscanf(cmd->args, "%31s", action) == 1) {
            if (strcasecmp(action, "enable") == 0 || strcasecmp(action, "on") == 0) {
                player->no_clip = true;
                if (out_msg && out_size > 0) {
                    snprintf(out_msg, out_size, "No-clip enabled");
                }
            } else if (strcasecmp(action, "disable") == 0 || strcasecmp(action, "off") == 0) {
                player->no_clip = false;
                if (out_msg && out_size > 0) {
                    snprintf(out_msg, out_size, "No-clip disabled");
                }
            } else if (out_msg && out_size > 0) {
                snprintf(out_msg, out_size, "Usage: /noclip <enable|disable>");
            }
        } else if (out_msg && out_size > 0) {
            snprintf(out_msg, out_size, "Usage: /noclip <enable|disable>");
        }
        break;
    }

    case CMD_GIVE: {
        char item_buf[64] = {0};
        int count = 1;
        sscanf(cmd->args, "%63s %d", item_buf, &count);
        BlockType block_type = BLOCK_AIR;
        const char *type_str = NULL;

        if (strcmp(item_buf, "stone") == 0) {
            block_type = BLOCK_STONE;
            type_str = "stone";
        } else if (strcmp(item_buf, "dirt") == 0) {
            block_type = BLOCK_DIRT;
            type_str = "dirt";
        } else if (strcmp(item_buf, "grass") == 0) {
            block_type = BLOCK_GRASS;
            type_str = "grass";
        } else if (strcmp(item_buf, "sand") == 0) {
            block_type = BLOCK_SAND;
            type_str = "sand";
        } else if (strcmp(item_buf, "wood") == 0) {
            block_type = BLOCK_WOOD;
            type_str = "wood";
        } else if (strcmp(item_buf, "glowstone") == 0) {
            block_type = BLOCK_GLOWSTONE;
            type_str = "glowstone";
        } else if (strcmp(item_buf, "glass") == 0) {
            block_type = BLOCK_GLASS;
            type_str = "glass";
        } else if (strcmp(item_buf, "cobblestone") == 0) {
            block_type = BLOCK_COBBLESTONE;
            type_str = "cobblestone";
        }

        if (type_str && inventory_give(player, block_type, count)) {
            if (out_msg && out_size > 0) {
                snprintf(out_msg, out_size, "Gave %d of %s", count, type_str);
            }
        } else if (out_msg && out_size > 0) {
            snprintf(out_msg, out_size, "Failed to give item: %s", item_buf);
        }
        break;
    }

    case CMD_SELECT: {
        const char *block_name = cmd->args;
        if (strcmp(block_name, "stone") == 0) {
            player->selected_block = BLOCK_STONE;
            if (out_msg && out_size > 0) {
                snprintf(out_msg, out_size, "Selected stone");
            }
        } else if (strcmp(block_name, "dirt") == 0) {
            player->selected_block = BLOCK_DIRT;
            if (out_msg && out_size > 0) {
                snprintf(out_msg, out_size, "Selected dirt");
            }
        } else if (strcmp(block_name, "grass") == 0) {
            player->selected_block = BLOCK_GRASS;
            if (out_msg && out_size > 0) {
                snprintf(out_msg, out_size, "Selected grass");
            }
        } else if (strcmp(block_name, "sand") == 0) {
            player->selected_block = BLOCK_SAND;
            if (out_msg && out_size > 0) {
                snprintf(out_msg, out_size, "Selected sand");
            }
        } else if (strcmp(block_name, "wood") == 0) {
            player->selected_block = BLOCK_WOOD;
            if (out_msg && out_size > 0) {
                snprintf(out_msg, out_size, "Selected wood");
            }
        } else if (strcmp(block_name, "glowstone") == 0) {
            player->selected_block = BLOCK_GLOWSTONE;
            if (out_msg && out_size > 0) {
                snprintf(out_msg, out_size, "Selected glowstone");
            }
        } else if (strcmp(block_name, "glass") == 0) {
            player->selected_block = BLOCK_GLASS;
            if (out_msg && out_size > 0) {
                snprintf(out_msg, out_size, "Selected glass");
            }
        } else if (out_msg && out_size > 0) {
            snprintf(out_msg, out_size, "Unknown block: %s", block_name);
        }
        break;
    }

    case CMD_SAVE: {
        str world_name_str = str_create(cmd->args);
        str_trim(&world_name_str);
        const char *world_name = cstr(&world_name_str);
        bool ok = world_save(world, world_name);
        if (out_msg && out_size > 0) {
            snprintf(out_msg, out_size, ok ? "Saved world '%s'" : "Failed to save world '%s'", world_name);
        }
        str_destroy(&world_name_str);
        break;
    }

    case CMD_LOAD: {
        str world_name_str = str_create(cmd->args);
        str_trim(&world_name_str);
        const char *world_name = cstr(&world_name_str);
        if (strlen(world->world_name) > 0) {
            world_save(world, world->world_name);
        }
        if (world_load(world, world_name)) {
            if (out_msg && out_size > 0) {
                snprintf(out_msg, out_size, "Loaded world '%s'", world_name);
            }
        } else if (out_msg && out_size > 0) {
            snprintf(out_msg, out_size, "Failed to load world '%s'", world_name);
        }
        str_destroy(&world_name_str);
        break;
    }

    case CMD_CREATEWORLD: {
        str world_name_str = str_create(cmd->args);
        str_trim(&world_name_str);
        const char *world_name = cstr(&world_name_str);
        bool valid_name = true;
        if (world_name[0] == '\0') {
            valid_name = false;
        } else {
            for (int i = 0; world_name[i]; i++) {
                char c = world_name[i];
                if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_')) {
                    valid_name = false;
                    break;
                }
            }
        }
        if (!valid_name) {
            str_destroy(&world_name_str);
            if (out_msg && out_size > 0) {
                snprintf(out_msg, out_size, "Invalid world name");
            }
            break;
        }
        World *new_world = world_create();
        if (!new_world) {
            if (out_msg && out_size > 0) {
                snprintf(out_msg, out_size, "Failed to create world");
            }
            break;
        }
        world_load_textures(new_world);
        size_t name_len = strlen(world_name);
        if (name_len >= sizeof(new_world->world_name)) {
            name_len = sizeof(new_world->world_name) - 1;
        }
        memcpy(new_world->world_name, world_name, name_len);
        new_world->world_name[name_len] = '\0';
        world_generate_prism(new_world);
        if (world_save(new_world, world_name)) {
            world_free(world);
            srv->world = new_world;
            if (out_msg && out_size > 0) {
                snprintf(out_msg, out_size, "Created world '%s'", world_name);
            }
        } else {
            world_free(new_world);
            if (out_msg && out_size > 0) {
                snprintf(out_msg, out_size, "Failed to create world '%s'", world_name);
            }
        }
        str_destroy(&world_name_str);
        break;
    }

    case CMD_LOADWORLD: {
        str world_name_str = str_create(cmd->args);
        str_trim(&world_name_str);
        const char *world_name = cstr(&world_name_str);
        World *new_world = world_create();
        if (!new_world) {
            if (out_msg && out_size > 0) {
                snprintf(out_msg, out_size, "Failed to create world");
            }
            break;
        }
        world_load_textures(new_world);
        if (world_load(new_world, world_name)) {
            world_free(world);
            srv->world = new_world;
            if (out_msg && out_size > 0) {
                snprintf(out_msg, out_size, "Loaded world '%s'", world_name);
            }
        } else {
            world_free(new_world);
            world_generate_prism(world);
            if (out_msg && out_size > 0) {
                snprintf(out_msg, out_size, "Failed to load world '%s'", world_name);
            }
        }
        str_destroy(&world_name_str);
        break;
    }

    case CMD_HELP:
        if (out_msg && out_size > 0) {
            snprintf(out_msg, out_size, "Commands: /tp, /give, /select, /save, /load, /addplayer, /removeplayer, /players, /quit, /help");
        }
        break;

    case CMD_UNKNOWN:
        if (out_msg && out_size > 0) {
            snprintf(out_msg, out_size, "Unknown command: %s", raw_input ? raw_input : "");
        }
        break;

    case CMD_NONE:
    default:
        break;
    }

    if (flight_enabled && *flight_enabled) {
        player->is_flying = true;
    }
}

static Player *game_server_get_player_by_uid(GameServer *srv, uint32_t player_uid) {
    if (!srv || player_uid == 0) {
        return NULL;
    }
    for (int i = 0; i < srv->player_count; i++) {
        if (srv->players[i] && srv->players[i]->uid == player_uid) {
            return srv->players[i];
        }
    }
    return NULL;
}

static Player *game_server_find_player_by_name(GameServer *srv, const char *name) {
    if (!srv || !name || name[0] == '\0') {
        return NULL;
    }
    for (int i = 0; i < srv->player_count; i++) {
        if (srv->players[i] && strcmp(srv->players[i]->nickname, name) == 0) {
            return srv->players[i];
        }
    }
    return NULL;
}

bool game_server_add_player(GameServer *srv, Player *player) {
    if (!srv || !player || srv->player_count >= GAME_SERVER_MAX_PLAYERS) {
        return false;
    }
    if (game_server_get_player_by_uid(srv, player->uid)) {
        return false;
    }
    srv->players[srv->player_count++] = player;
    return true;
}

bool game_server_remove_player(GameServer *srv, uint32_t player_uid) {
    if (!srv || player_uid == 0) {
        return false;
    }
    for (int i = 0; i < srv->player_count; i++) {
        if (srv->players[i] && srv->players[i]->uid == player_uid) {
            for (int j = i; j + 1 < srv->player_count; j++) {
                srv->players[j] = srv->players[j + 1];
            }
            srv->players[--srv->player_count] = NULL;
            return true;
        }
    }
    return false;
}

Player *game_server_get_player(GameServer *srv, uint32_t player_uid) {
    return game_server_get_player_by_uid(srv, player_uid);
}

void game_server_init(GameServer *srv, World *world, Player *player) {
    if (!srv) {
        return;
    }
    memset(srv, 0, sizeof(*srv));
    srv->world = world;
    srv->flight_enabled = false;
    srv->interest_position = (Vector3){0.0f, 0.0f, 0.0f};
    srv->interest_forward = (Vector3){0.0f, 0.0f, 1.0f};
    srv->render_distance_blocks = 50.0f;
    if (player) {
        game_server_add_player(srv, player);
    }
}

void game_server_reset(GameServer *srv, World *world, Player *player) {
    game_server_init(srv, world, player);
}

void game_server_set_interest(GameServer *srv, Vector3 position, Vector3 forward, float render_distance_blocks) {
    if (!srv) {
        return;
    }
    srv->interest_position = position;
    srv->interest_forward = forward;
    srv->render_distance_blocks = render_distance_blocks;
}

void game_server_tick(GameServer *srv, float fixed_dt) {
    if (!srv || !srv->world || srv->player_count == 0) {
        return;
    }

    for (int i = 0; i < srv->player_count; i++) {
        Player *player = srv->players[i];
        if (player) {
            player_update(player, srv->world, fixed_dt, srv->flight_enabled);
        }
    }

    Player *focus = srv->players[0];
    if (!focus) {
        return;
    }
    world_update_chunks(srv->world, focus->position, srv->interest_forward, srv->render_distance_blocks);
}

static uint32_t game_server_generate_unique_uid(GameServer *srv) {
    static uint32_t next_uid = 2;
    if (!srv) {
        return next_uid++;
    }
    while (game_server_get_player_by_uid(srv, next_uid) != NULL || next_uid == 0) {
        next_uid++;
    }
    return next_uid++;
}

void game_server_submit_input(GameServer *srv, uint32_t player_uid, const PlayerInputCommand *cmd) {
    if (!srv || !cmd) {
        return;
    }

    Player *player = game_server_get_player_by_uid(srv, player_uid);
    if (!player) {
        return;
    }

    player->shifting = cmd->shift;

    if (cmd->fly_toggle && srv->flight_enabled) {
        player->is_flying = !player->is_flying;
        if (player->is_flying) {
            player->velocity.y = 0.0f;
        }
    }

    if (player->is_flying) {
        player->velocity.x = cmd->move_x;
        player->velocity.z = cmd->move_z;
        player->velocity.y = 0.0f;
        if (cmd->jump) {
            player->velocity.y = FLY_SPEED;
        }
        if (cmd->shift) {
            player->velocity.y = -FLY_SPEED;
        }
    } else {
        if (cmd->jump && player->on_ground && !player->jump_used) {
            player->velocity.y = JUMP_FORCE;
            player->on_ground = false;
            player->jump_used = true;
        } else if (!cmd->jump) {
            player->jump_used = false;
        }

        player->velocity.x = cmd->move_x;
        player->velocity.z = cmd->move_z;
    }

    if (cmd->selected_slot >= 0) {
        player->selected_slot = cmd->selected_slot;
    }
}

static Player *game_server_get_command_target(GameServer *srv, const ConsoleCommand *cmd, uint32_t default_uid) {
    if (!srv || !cmd) {
        return NULL;
    }

    if (cmd->player_target[0] != '\0') {
        uint32_t target_uid = console_parse_uid(cmd->player_target);
        if (target_uid != 0) {
            Player *target = game_server_get_player_by_uid(srv, target_uid);
            if (target) {
                return target;
            }
        }
        Player *target = game_server_find_player_by_name(srv, cmd->player_target);
        if (target) {
            return target;
        }
        return NULL;
    }

    return game_server_get_player_by_uid(srv, default_uid);
}

static bool game_server_remove_player_by_target(GameServer *srv, const char *target_str) {
    if (!srv || !target_str || target_str[0] == '\0') {
        return false;
    }

    uint32_t target_uid = console_parse_uid(target_str);
    if (target_uid != 0) {
        return game_server_remove_player(srv, target_uid);
    }

    Player *target = game_server_find_player_by_name(srv, target_str);
    if (!target) {
        return false;
    }

    return game_server_remove_player(srv, target->uid);
}

bool game_server_submit_command(GameServer *srv,
                                uint32_t player_uid,
                                const ConsoleCommand *cmd,
                                const char *raw_input,
                                bool *should_quit,
                                bool *flight_enabled,
                                bool *show_chunk_borders,
                                World **world_out,
                                Player **player_out,
                                char *out_msg,
                                size_t out_size) {
    if (!srv || !cmd) {
        return false;
    }

    if (cmd->type == CMD_ADDPLAYER) {
        const char *nickname = cmd->args[0] != '\0' ? cmd->args : "Player";
        uint32_t uid = game_server_generate_unique_uid(srv);
        Vector3 spawn_pos = srv->world ? srv->world->last_player_position : (Vector3){0.0f, 0.0f, 0.0f};
        Player *new_player = player_create_with_uid(spawn_pos.x, spawn_pos.y + 1.0f, spawn_pos.z, uid, nickname);
        if (!new_player) {
            if (out_msg && out_size > 0) {
                snprintf(out_msg, out_size, "Failed to allocate new player");
            }
            return false;
        }

        if (!game_server_add_player(srv, new_player)) {
            player_free(new_player);
            if (out_msg && out_size > 0) {
                snprintf(out_msg, out_size, "Failed to add player '%s'", nickname);
            }
            return false;
        }

        if (out_msg && out_size > 0) {
            snprintf(out_msg, out_size, "Added player '%s' (UID %08x)", nickname, uid);
        }
        if (player_out) {
            *player_out = new_player;
        }
        if (world_out) {
            *world_out = srv->world;
        }
        return true;
    }

    if (cmd->type == CMD_REMOVEPLAYER) {
        const char *target = cmd->player_target[0] != '\0' ? cmd->player_target : cmd->args;
        if (target[0] == '\0') {
            if (out_msg && out_size > 0) {
                snprintf(out_msg, out_size, "Usage: /removeplayer <uid|name>");
            }
            return false;
        }

        if (game_server_remove_player_by_target(srv, target)) {
            if (out_msg && out_size > 0) {
                snprintf(out_msg, out_size, "Removed player: %s", target);
            }
            return true;
        }

        if (out_msg && out_size > 0) {
            snprintf(out_msg, out_size, "Player not found: %s", target);
        }
        return false;
    }

    if (cmd->type == CMD_LISTPLAYERS) {
        if (out_msg && out_size > 0) {
            size_t used = 0;
            used += snprintf(out_msg + used, out_size > used ? out_size - used : 0, "Players (%d):", srv->player_count);
            for (int i = 0; i < srv->player_count && used < out_size; i++) {
                Player *player = srv->players[i];
                if (!player) {
                    continue;
                }
                used += snprintf(out_msg + used, out_size > used ? out_size - used : 0, " %s(%08x)", player->nickname, player->uid);
            }
        }
        if (player_out) {
            *player_out = NULL;
        }
        if (world_out) {
            *world_out = srv->world;
        }
        return true;
    }

    Player *target = game_server_get_command_target(srv, cmd, player_uid);
    if (!target) {
        if (out_msg && out_size > 0) {
            snprintf(out_msg, out_size, "Player not found: %s", cmd->player_target[0] != '\0' ? cmd->player_target : "<default>");
        }
        return false;
    }

    game_server_apply_command(srv, target, cmd, raw_input, should_quit, flight_enabled, show_chunk_borders, out_msg, out_size);
    if (world_out) {
        *world_out = srv->world;
    }
    if (player_out) {
        *player_out = target;
    }
    return true;
}
