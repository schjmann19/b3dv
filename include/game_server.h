#ifndef GAME_SERVER_H
#define GAME_SERVER_H

#include "console.h"
#include "player.h"
#include "world.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float move_x;
    float move_z;
    bool jump;
    bool shift;
    bool sprint;
    bool fly_toggle;
    int selected_slot;
    bool break_block;
    bool place_block;
} PlayerInputCommand;

#define GAME_SERVER_MAX_PLAYERS 16

typedef struct {
    World *world;
    Player *players[GAME_SERVER_MAX_PLAYERS];
    int player_count;
    bool flight_enabled;
    Vector3 interest_position;
    Vector3 interest_forward;
    float render_distance_blocks;
} GameServer;

void game_server_init(GameServer *srv, World *world, Player *player);
void game_server_reset(GameServer *srv, World *world, Player *player);
void game_server_set_interest(GameServer *srv, Vector3 position, Vector3 forward, float render_distance_blocks);
void game_server_tick(GameServer *srv, float fixed_dt);
void game_server_submit_input(GameServer *srv, uint32_t player_uid, const PlayerInputCommand *cmd);
bool game_server_add_player(GameServer *srv, Player *player);
bool game_server_remove_player(GameServer *srv, uint32_t player_uid);
Player *game_server_get_player(GameServer *srv, uint32_t player_uid);
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
                                size_t out_size);

#endif
