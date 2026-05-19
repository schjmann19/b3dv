#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#include "console.h"

// Command queue
#define CONSOLE_QUEUE_SIZE 32

static struct {
    ConsoleCommand queue[CONSOLE_QUEUE_SIZE];
    int read_idx;
    int write_idx;
    pthread_mutex_t lock;
    pthread_t input_thread;
    bool running;
} console_state = {0};

// Trim whitespace from beginning and end of string
static void trim_string(char* str)
{
    if (!str || str[0] == '\0') return;
    
    // Trim start
    int start = 0;
    while (str[start] && isspace(str[start])) start++;
    
    // Trim end
    int end = strlen(str) - 1;
    while (end >= start && isspace(str[end])) end--;
    
    // Copy trimmed string back
    int len = end - start + 1;
    if (start > 0) {
        memmove(str, str + start, len);
    }
    str[len] = '\0';
}

// Parse a line of console input
// Syntax: /command [player_target] [args...]
ConsoleCommand console_parse_command(const char* input)
{
    ConsoleCommand cmd = {0};
    
    // Copy raw input for debugging
    strncpy(cmd.raw_input, input, sizeof(cmd.raw_input) - 1);
    cmd.raw_input[sizeof(cmd.raw_input) - 1] = '\0';
    
    // Skip leading whitespace
    while (input && isspace(*input)) input++;
    
    // If not a command, treat as chat message
    if (!input || input[0] != '/') {
        cmd.type = CMD_CHAT;
        strncpy(cmd.args, input ? input : "", sizeof(cmd.args) - 1);
        cmd.args[sizeof(cmd.args) - 1] = '\0';
        return cmd;
    }
    
    input++;  // Skip the '/'
    
    // Extract command name
    char command[32] = {0};
    int cmd_len = 0;
    while (input[cmd_len] && !isspace(input[cmd_len]) && input[cmd_len] != ' ' && cmd_len < 31) {
        command[cmd_len] = input[cmd_len];
        cmd_len++;
    }
    command[cmd_len] = '\0';
    
    // Determine command type
    if (strcmp(command, "give") == 0) cmd.type = CMD_GIVE;
    else if (strcmp(command, "tp") == 0) cmd.type = CMD_TP;
    else if (strcmp(command, "select") == 0) cmd.type = CMD_SELECT;
    else if (strcmp(command, "save") == 0) cmd.type = CMD_SAVE;
    else if (strcmp(command, "load") == 0) cmd.type = CMD_LOAD;
    else if (strcmp(command, "createworld") == 0) cmd.type = CMD_CREATEWORLD;
    else if (strcmp(command, "loadworld") == 0) cmd.type = CMD_LOADWORLD;
    else if (strcmp(command, "quit") == 0) cmd.type = CMD_QUIT;
    else if (strcmp(command, "help") == 0) cmd.type = CMD_HELP;
    else {
        cmd.type = CMD_UNKNOWN;
        strncpy(cmd.args, input + cmd_len, sizeof(cmd.args) - 1);
        return cmd;
    }
    
    // Move past command name and whitespace
    input += cmd_len;
    while (input && isspace(*input)) input++;
    
    // For some commands, parse player target
    // Commands with player targets: give, tp, select
    // Others don't have player targets (save, load, etc.)
    bool has_player_target = (cmd.type == CMD_GIVE || cmd.type == CMD_TP || cmd.type == CMD_SELECT);
    
    if (has_player_target && input && *input != '\0') {
        // Try to extract player name/target
        // Player target can be:
        // - A name like "Player"
        // - A UID like "00000001"
        // - If it looks like a number, it's part of the command args, not player target
        
        char potential_target[64] = {0};
        int target_len = 0;
        
        // Read until space or end of string
        while (input[target_len] && !isspace(input[target_len]) && target_len < 63) {
            potential_target[target_len] = input[target_len];
            target_len++;
        }
        potential_target[target_len] = '\0';
        
        // Check if this looks like a player name or UID
        // UIDs are 8 hex digits, player names are alphabetic
        bool looks_like_uid = (strlen(potential_target) == 8);
        bool looks_like_name = (isalpha(potential_target[0]));
        
        if (looks_like_uid || looks_like_name) {
            // It's a player target
            strncpy(cmd.player_target, potential_target, sizeof(cmd.player_target) - 1);
            cmd.player_target[sizeof(cmd.player_target) - 1] = '\0';
            
            // Move input past the target
            input += target_len;
            while (input && isspace(*input)) input++;
        }
    }
    
    // Rest of the line is args
    if (input && *input != '\0') {
        strncpy(cmd.args, input, sizeof(cmd.args) - 1);
        cmd.args[sizeof(cmd.args) - 1] = '\0';
        trim_string(cmd.args);
    }
    
    return cmd;
}

// Format a player UID as 8-digit hex string (e.g., "00000001")
void console_format_uid(uint32_t uid, char* buf, int buf_size)
{
    snprintf(buf, buf_size, "%08x", uid);
}

// Parse UID from hex string (e.g., "00000001" -> 1)
uint32_t console_parse_uid(const char* uid_str)
{
    if (!uid_str || strlen(uid_str) != 8) return 0;
    
    uint32_t uid = 0;
    if (sscanf(uid_str, "%08x", &uid) == 1) {
        return uid;
    }
    return 0;
}

// Thread function for reading console input
static void* console_input_thread_fn(void* arg)
{
    (void)arg;  // Unused
    
    char line[256] = {0};
    
    printf("[console] Ready. Type /help for available commands.\n");
    printf("[console] > ");
    fflush(stdout);
    
    while (console_state.running) {
        // Read a line from stdin (blocking)
        if (fgets(line, sizeof(line), stdin) != NULL) {
            // Remove trailing newline
            int len = strlen(line);
            if (len > 0 && line[len - 1] == '\n') {
                line[len - 1] = '\0';
                len--;
            }
            
            // Parse the command
            ConsoleCommand cmd = console_parse_command(line);
            
            if (cmd.type != CMD_NONE) {
                // Add to queue
                pthread_mutex_lock(&console_state.lock);
                
                int next_write = (console_state.write_idx + 1) % CONSOLE_QUEUE_SIZE;
                if (next_write != console_state.read_idx) {
                    // Queue not full, add command
                    console_state.queue[console_state.write_idx] = cmd;
                    console_state.write_idx = next_write;
                } else {
                    // Queue full, print warning
                    fprintf(stderr, "[console] WARNING: Command queue full, dropping message\n");
                }
                
                pthread_mutex_unlock(&console_state.lock);
            }
            
            // Print prompt for next input
            printf("[console] > ");
            fflush(stdout);
        }
    }
    
    return NULL;
}

// Initialize the console input system
void console_init(void)
{
    if (console_state.running) return;  // Already initialized
    
    pthread_mutex_init(&console_state.lock, NULL);
    console_state.read_idx = 0;
    console_state.write_idx = 0;
    console_state.running = true;
    
    pthread_create(&console_state.input_thread, NULL, console_input_thread_fn, NULL);
}

// Shutdown the console input system
void console_shutdown(void)
{
    if (!console_state.running) return;  // Already shutdown
    
    console_state.running = false;
    pthread_join(console_state.input_thread, NULL);
    pthread_mutex_destroy(&console_state.lock);
}

// Get next pending console command (non-blocking)
bool console_get_next_command(ConsoleCommand* out_cmd)
{
    if (!out_cmd) return false;
    
    pthread_mutex_lock(&console_state.lock);
    
    bool has_command = (console_state.read_idx != console_state.write_idx);
    if (has_command) {
        *out_cmd = console_state.queue[console_state.read_idx];
        console_state.read_idx = (console_state.read_idx + 1) % CONSOLE_QUEUE_SIZE;
    }
    
    pthread_mutex_unlock(&console_state.lock);
    
    return has_command;
}

