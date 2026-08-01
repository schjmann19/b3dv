#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdbool.h>
#include <stdint.h>

// Console command types
typedef enum {
    CMD_NONE,
    CMD_GIVE,
    CMD_TP,
    CMD_SELECT,
    CMD_SAVE,
    CMD_LOAD,
    CMD_CREATEWORLD,
    CMD_LOADWORLD,
    CMD_FLY,
    CMD_NOCLIP,
    CMD_ADDPLAYER,
    CMD_REMOVEPLAYER,
    CMD_LISTPLAYERS,
    CMD_QUIT,
    CMD_HELP,
    CMD_UNKNOWN,
    CMD_CHAT
} CommandType;

// Parsed console command
typedef struct {
    CommandType type;
    char player_target[64]; // Player name or UID to target (empty = current player)
    char args[256];         // Raw argument string after command and player
    char raw_input[256];    // Raw input line for debugging
} ConsoleCommand;

// Initialize the console input system (starts stdin reader thread)
void console_init(void);

// Shutdown the console input system
void console_shutdown(void);

// Get next pending console command (non-blocking)
// Returns true if a command was retrieved, false if queue is empty
bool console_get_next_command(ConsoleCommand *out_cmd);

// Parse a console command line
// Returns the parsed command
ConsoleCommand console_parse_command(const char *input);

// Format a player UID as 8-digit hex string (e.g., "00000001")
void console_format_uid(uint32_t uid, char *buf, int buf_size);

// Parse UID from hex string (e.g., "00000001" -> 1)
// Returns 0 on parse error
uint32_t console_parse_uid(const char *uid_str);

#endif
