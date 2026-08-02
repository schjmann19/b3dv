/* compat: for unix-like systems only */

#ifndef ELEVATE_H
#define ELEVATE_H

/* determine which privilege elevator program, sudo or doas, is available. checks for sudo first, then doas */
const char *determine_elevator();

/* elevate a commmand by inserting sudo or doas where needed
 example:
 input: `apt update && apt upgrade` (notice that's two commands) will
 output: `sudo apt update && sudo apt upgrade` (notice sudo (or doas) is
 inserted where needed, not just at the begninning)
 NOTE: you must free it */
const char *elevate_command(const char *command);

#endif /* ELEVATE_H */

/* Implementation */
#ifdef ELEVATE_IMPLEMENTATION
#include "src/elevate.c"
#endif