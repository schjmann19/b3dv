/* compat: for unix-like systems only (note: for now only tested on linux) */

#ifndef TEXTURAWASD_PROCESS_UTILS
#define TEXTURAWASD_PROCESS_UTILS

#include <stdbool.h>
#include <sys/types.h>

/* old function that just runs a thing, not saving the output */
int run(const char *cmd);

/* note: free it, will you? */
char *capture_output(const char *cmd);

/* runs a thing and explicitly throws out the output*/
int run_quiet(const char *cmd);

/* shorthand to check if its running */
bool is_process_running(pid_t pid);

/* pidof without shelling out */
pid_t pidof(const char *name);

#endif /* TEXTURAWASD_PROCESS_UTILS */

/* Implementation */
#ifdef PROCESS_UTILS_IMPLEMENTATION
#include "src/process_utils.c"
#endif