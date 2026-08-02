/* compat: for unix-like systems only (note: for now only tested on linux) */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "../process_utils.h"

/* old function that just runs a thing, not saving the output */
int run(const char *cmd) {
    FILE *pipe = popen(cmd, "r");
    if (!pipe) {
        return -1;
    }
    char buffer[128];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        // Read and discard output
    }
    int status = pclose(pipe);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

/* note: free it, will you? */
char *capture_output(const char *cmd) {
    char *output = NULL;
    size_t size = 0;
    FILE *pipe = popen(cmd, "r");
    if (!pipe) {
        return NULL;
    }
    char buffer[128];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        size_t len = strlen(buffer);
        char *new_output = realloc(output, size + len + 1);
        if (!new_output) {
            free(output);
            pclose(pipe);
            return NULL;
        }
        output = new_output;
        memcpy(output + size, buffer, len);
        size += len;
        output[size] = '\0';
    }
    pclose(pipe);
    return output;
}

/* runs a thing and explicitly throws out the output*/
int run_quiet(const char *cmd) {
    char full_cmd[512];
    snprintf(full_cmd, sizeof(full_cmd), "%s >/dev/null 2>&1", cmd);
    FILE *pipe = popen(full_cmd, "r");
    if (!pipe) {
        return -1;
    }
    int status = pclose(pipe);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

/* shorthand to check if its running */
bool is_process_running(pid_t pid) {
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "kill -0 %d 2>/dev/null", (int)pid);
    return run_quiet(cmd) == 0;
}

/* pidof without shelling out */
pid_t pidof(const char *name) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "pgrep -x %s", name);
    char *output = capture_output(cmd);
    if (!output || strlen(output) == 0) {
        free(output);
        return -1;
    }
    pid_t pid = (pid_t)atoi(output);
    free(output);
    return pid > 0 ? pid : -1;
}