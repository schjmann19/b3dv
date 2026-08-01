#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "../common_utils/simple_strings.h"
#include "../include/world.h"
#include "../include/game_server.h"
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
            snprintf(out_msg, out_size, "Commands: /tp, /give, /select, /save, /load, /quit");
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

void game_server_init(GameServer *srv, World *world, Player *player) {
    if (!srv) {
        return;
    }
    memset(srv, 0, sizeof(*srv));
    srv->world = world;
    srv->players[0] = player;
    srv->player_count = player ? 1 : 0;
    srv->flight_enabled = false;
    srv->interest_position = (Vector3){0.0f, 0.0f, 0.0f};
    srv->interest_forward = (Vector3){0.0f, 0.0f, 1.0f};
    srv->render_distance_blocks = 50.0f;
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
    if (!srv || !srv->world || !srv->players[0]) {
        return;
    }

    Player *player = srv->players[0];
    player_update(player, srv->world, fixed_dt, srv->flight_enabled);
    world_update_chunks(srv->world, player->position, srv->interest_forward, srv->render_distance_blocks);
}

void game_server_submit_input(GameServer *srv, uint32_t player_uid, const PlayerInputCommand *cmd) {
    (void)player_uid;
    if (!srv || !srv->players[0] || !cmd) {
        return;
    }

    Player *player = srv->players[0];
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
    (void)player_uid;
    if (!srv || !srv->players[0] || !cmd) {
        return false;
    }

    Player *player = srv->players[0];
    game_server_apply_command(srv, player, cmd, raw_input, should_quit, flight_enabled, show_chunk_borders, out_msg, out_size);
    if (world_out) {
        *world_out = srv->world;
    }
    if (player_out) {
        *player_out = player;
    }
    return true;
}
