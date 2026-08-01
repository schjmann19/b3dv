#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#include "../../include/world.h"
#include "../../include/player.h"
#include "../../include/game_server.h"
#include "../../include/console.h"

static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return -1;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static bool recv_line_nonblocking(int sock, char *buffer, size_t *used, char *out_line, size_t out_line_size) {
    if (*used < 1 || buffer[*used - 1] != '\n') {
        ssize_t bytes = recv(sock, buffer + *used, 1024 - *used, 0);
        if (bytes > 0) {
            *used += (size_t)bytes;
        } else if (bytes == 0) {
            return false;
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            return false;
        }
    }

    char *newline = memchr(buffer, '\n', *used);
    if (!newline) {
        return false;
    }

    size_t line_len = (size_t)(newline - buffer);
    if (line_len >= out_line_size) {
        line_len = out_line_size - 1;
    }
    memcpy(out_line, buffer, line_len);
    out_line[line_len] = '\0';
    if (line_len > 0 && out_line[line_len - 1] == '\r') {
        out_line[line_len - 1] = '\0';
    }

    size_t remaining = *used - (line_len + 1);
    memmove(buffer, newline + 1, remaining);
    *used = remaining;
    return true;
}

static int create_listen_socket(int port) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        return -1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(server_fd);
        return -1;
    }

    if (listen(server_fd, 1) < 0) {
        close(server_fd);
        return -1;
    }

    if (set_nonblocking(server_fd) < 0) {
        close(server_fd);
        return -1;
    }

    return server_fd;
}

static int accept_client_connection(int listen_fd, char *out_addr, size_t addr_size) {
    struct sockaddr_storage client_addr = {0};
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd < 0) {
        return -1;
    }

    if (set_nonblocking(client_fd) < 0) {
        close(client_fd);
        return -1;
    }

    if (out_addr) {
        if (client_addr.ss_family == AF_INET) {
            struct sockaddr_in *addr_in = (struct sockaddr_in *)&client_addr;
            inet_ntop(AF_INET, &addr_in->sin_addr, out_addr, addr_size);
        } else if (client_addr.ss_family == AF_INET6) {
            struct sockaddr_in6 *addr_in6 = (struct sockaddr_in6 *)&client_addr;
            inet_ntop(AF_INET6, &addr_in6->sin6_addr, out_addr, addr_size);
        } else {
            snprintf(out_addr, addr_size, "unknown");
        }
    }

    return client_fd;
}

int b3dv_main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: b3dv-server <world_name> [port]\n");
        return 1;
    }

    const char *world_name = argv[1];
    int port = 42069;
    if (argc >= 3) {
        int parsed_port = atoi(argv[2]);
        if (parsed_port > 0 && parsed_port <= 65535) {
            port = parsed_port;
        }
    }

    console_init();

    World *world = world_create();
    if (!world) {
        fprintf(stderr, "Failed to allocate world\n");
        console_shutdown();
        return 1;
    }

    if (!world_load(world, world_name)) {
        fprintf(stderr, "Failed to load world '%s'\n", world_name);
        world_free(world);
        console_shutdown();
        return 1;
    }

    Player *player = player_create_with_uid(
        world->last_player_position.x,
        world->last_player_position.y,
        world->last_player_position.z,
        0x00000001,
        "Server");
    if (!player) {
        fprintf(stderr, "Failed to create server player\n");
        world_free(world);
        console_shutdown();
        return 1;
    }

    world_apply_players_to(world, player);
    world->current_player = player;
    strncpy(world->player_nickname, player->nickname, sizeof(world->player_nickname) - 1);
    world->player_nickname[sizeof(world->player_nickname) - 1] = '\0';

    GameServer srv;
    game_server_init(&srv, world, player);
    Player *server_player = player;

    int listen_fd = create_listen_socket(port);
    if (listen_fd < 0) {
        perror("Failed to open server socket");
        player_free(player);
        world_free(world);
        console_shutdown();
        return 1;
    }

    int client_fd = -1;
    char client_addr[INET6_ADDRSTRLEN] = "";
    char client_buffer[1024] = {0};
    size_t client_buffer_used = 0;
    printf("Server started for world '%s' on port %d. Type /help for commands.\n", world_name, port);

    bool should_quit = false;
    while (!should_quit) {
        if (client_fd < 0) {
            int accepted = accept_client_connection(listen_fd, client_addr, sizeof(client_addr));
            if (accepted >= 0) {
                client_fd = accepted;
                printf("Client connected from %s\n", client_addr);
                char welcome[256];
                int welcome_len = snprintf(welcome, sizeof(welcome), "WELCOME %s\n", world_name);
                if (welcome_len > 0) {
                    send(client_fd, welcome, welcome_len, 0);
                }
            } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("accept");
            }
        }

        if (client_fd >= 0) {
            char line[512] = {0};
            while (recv_line_nonblocking(client_fd, client_buffer, &client_buffer_used, line, sizeof(line))) {
                if (strncmp(line, "CHAT ", 5) == 0) {
                    const char *chat_msg = line + 5;
                    printf("[chat] %s\n", chat_msg);
                    char response[1024];
                    int len = snprintf(response, sizeof(response), "CHAT %s\n", chat_msg);
                    if (len > 0) {
                        send(client_fd, response, len, 0);
                    }
                } else if (strncmp(line, "CMD ", 4) == 0) {
                    const char *cmd_text = line + 4;
                    char full_cmd[512];
                    snprintf(full_cmd, sizeof(full_cmd), "/%s", cmd_text);
                    ConsoleCommand parsed_cmd = console_parse_command(full_cmd);
                    bool should_quit_cmd = false;
                    bool flight_enabled_cmd = srv.flight_enabled;
                    bool show_chunk_borders_cmd = false;
                    char out_msg[512] = {0};
                    if (game_server_submit_command(&srv,
                                                   player->uid,
                                                   &parsed_cmd,
                                                   full_cmd,
                                                   &should_quit_cmd,
                                                   &flight_enabled_cmd,
                                                   &show_chunk_borders_cmd,
                                                   NULL,
                                                   NULL,
                                                   out_msg,
                                                   sizeof(out_msg))) {
                        if (out_msg[0] != '\0') {
                            char response[1024];
                            int len = snprintf(response, sizeof(response), "SERVERMSG %s\n", out_msg);
                            if (len > 0) {
                                send(client_fd, response, len, 0);
                            }
                        }
                        srv.flight_enabled = flight_enabled_cmd;
                    } else {
                        char response[1024];
                        int len = snprintf(response, sizeof(response), "ERROR Command failed: %s\n", cmd_text);
                        if (len > 0) {
                            send(client_fd, response, len, 0);
                        }
                    }
                } else if (strncmp(line, "INPUT ", 6) == 0) {
                    float move_x = 0.0f;
                    float move_z = 0.0f;
                    int jump = 0;
                    int shift = 0;
                    int sprint = 0;
                    int fly_toggle = 0;
                    int selected_slot = 0;
                    int parsed = sscanf(line + 6, "%f %f %d %d %d %d %d", &move_x, &move_z, &jump, &shift, &sprint, &fly_toggle, &selected_slot);
                    if (parsed >= 6) {
                        PlayerInputCommand input_cmd = {0};
                        input_cmd.move_x = move_x;
                        input_cmd.move_z = move_z;
                        input_cmd.jump = jump != 0;
                        input_cmd.shift = shift != 0;
                        input_cmd.sprint = sprint != 0;
                        input_cmd.fly_toggle = fly_toggle != 0;
                        input_cmd.selected_slot = selected_slot;
                        game_server_submit_input(&srv, player->uid, &input_cmd);
                    }
                } else if (strncmp(line, "BLOCKBREAK ", 11) == 0) {
                    int x = 0;
                    int y = 0;
                    int z = 0;
                    if (sscanf(line + 11, "%d %d %d", &x, &y, &z) == 3) {
                        BlockType current = world_get_block(srv.world, x, y, z);
                        if (current != BLOCK_AIR && current != BLOCK_BEDROCK) {
                            world_set_block(srv.world, x, y, z, BLOCK_AIR);
                            char update[256];
                            int len = snprintf(update, sizeof(update), "BLOCKSET %d %d %d %d\n", x, y, z, BLOCK_AIR);
                            if (len > 0) {
                                send(client_fd, update, len, 0);
                            }
                        }
                    }
                } else if (strncmp(line, "BLOCKPLACE ", 11) == 0) {
                    int x = 0;
                    int y = 0;
                    int z = 0;
                    int block_type = 0;
                    if (sscanf(line + 11, "%d %d %d %d", &x, &y, &z, &block_type) == 4) {
                        BlockType place_type = (BlockType)block_type;
                        if (place_type >= BLOCK_AIR && place_type <= BLOCK_GLASS) {
                            world_set_block(srv.world, x, y, z, place_type);
                            char update[256];
                            int len = snprintf(update, sizeof(update), "BLOCKSET %d %d %d %d\n", x, y, z, place_type);
                            if (len > 0) {
                                send(client_fd, update, len, 0);
                            }
                        }
                    }
                } else {
                    printf("[client] %s\n", line);
                }
            }

            ssize_t bytes = recv(client_fd, client_buffer + client_buffer_used, sizeof(client_buffer) - client_buffer_used, 0);
            if (bytes == 0) {
                printf("Client disconnected\n");
                close(client_fd);
                client_fd = -1;
                client_buffer_used = 0;
            } else if (bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("recv");
                close(client_fd);
                client_fd = -1;
                client_buffer_used = 0;
            } else if (bytes > 0) {
                client_buffer_used += (size_t)bytes;
                if (client_buffer_used > sizeof(client_buffer)) {
                    client_buffer_used = 0;
                }
            }
        }

        ConsoleCommand cmd;
        if (console_get_next_command(&cmd)) {
            char out_msg[512] = {0};
            World *world_out = NULL;
            Player *player_out = NULL;
            game_server_submit_command(&srv,
                                       player->uid,
                                       &cmd,
                                       cmd.raw_input,
                                       &should_quit,
                                       &srv.flight_enabled,
                                       NULL,
                                       &world_out,
                                       &player_out,
                                       out_msg,
                                       sizeof(out_msg));

            if (world_out && world_out != srv.world) {
                srv.world = world_out;
            }

            if (out_msg[0] != '\0') {
                printf("[server] %s\n", out_msg);
            }
        }

        game_server_tick(&srv, 1.0f / 60.0f);

        if (client_fd >= 0) {
            for (int i = 0; i < srv.player_count; i++) {
                Player *sv_player = srv.players[i];
                if (!sv_player) {
                    continue;
                }
                char update[512];
                int len = snprintf(update, sizeof(update), "PLAYERSTATE %u %.3f %.3f %.3f %.3f %.3f %.3f %d %d %s\n",
                                   sv_player->uid,
                                   sv_player->position.x,
                                   sv_player->position.y,
                                   sv_player->position.z,
                                   sv_player->velocity.x,
                                   sv_player->velocity.y,
                                   sv_player->velocity.z,
                                   sv_player->selected_slot,
                                   sv_player->is_flying ? 1 : 0,
                                   sv_player->nickname);
                if (len > 0) {
                    send(client_fd, update, len, 0);
                }
            }
        }

        usleep(16667);
    }

    console_shutdown();
    player_free(player);
    world_free(srv.world);
    return 0;
}
