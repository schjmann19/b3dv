/* compat: for unix-like systems only */

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../elevate.h"
#include "../have.h"

/* determine which privilege elevator program, sudo or doas, is available. checks for sudo first, then doas */
const char* determine_elevator() {
	const char *elevator = "neither_sudo_nor_doas_found.";
	if (command_exists("sudo")) { elevator = "sudo"; }
	if (command_exists("doas")) { elevator = "doas"; }
    return elevator;
}

/* check if the input command is simple enough or not */
static bool is_simple_command_list(const char *s)
{
    bool in_single = false;
    bool in_double = false;
    bool escaped = false;

    for (; *s; s++) {
        char c = *s;

        if (escaped) {
            escaped = false;
            continue;
        }

        if (c == '\\') {
            escaped = true;
            continue;
        }

        if (!in_double && c == '\'') {
            in_single = !in_single;
            continue;
        }

        if (!in_single && c == '"') {
            in_double = !in_double;
            continue;
        }

        if (in_single || in_double)
            continue;

        switch (c) {

        case '|':
            if (s[1] == '|') {
                s++;
                continue;
            }
            return false;

        case '&':
            if (s[1] == '&') {
                s++;
                continue;
            }
            return false;

        case ';':
            continue;

        case '(':
        case ')':
        case '{':
        case '}':
        case '<':
        case '>':
        case '`':
        case '\n':
            return false;

        case '$':
            if (s[1] == '(' || s[1] == '{')
                return false;
            break;
        }
    }

    return !in_single && !in_double;
}


/* just wrap it in sudo sh -c "s" */
static char *shell_single_quote(const char *s)
{
    size_t len = 2; /* opening + closing quote */

    for (const char *p = s; *p; p++) {
        if (*p == '\'') {
            len += 4; /* '\'' */
        } else {
            len += 1;
        }
    }

    char *out = malloc(len + 1);
    char *q = out;

    *q++ = '\'';

    for (const char *p = s; *p; p++) {
        if (*p == '\'') {
            memcpy(q, "'\\''", 4);
            q += 4;
        } else {
            *q++ = *p;
        }
    }

    *q++ = '\'';
    *q = '\0';

    return out;
}

/* examine the input command to see if where to insert the elevator command.
 this is done because maybe the elevator command has to be inserted more than once,
 because the plain command actually runs multiple commands, separated by shell
 operators such as `&&`, `||`, `;`, possibly `|`, `&`, etc. */
static char *examine_command_and_insert_elevator(const char *command)
{
    const char *elevator = determine_elevator();

    if (!is_simple_command_list(command)) {
        char *quoted = shell_single_quote(command);

        size_t len =
            strlen(elevator)  +
            strlen(" sh -c ") +
            strlen(quoted);

        char *out = malloc(len + 1);

        sprintf(out, "%s sh -c %s", elevator, quoted);

        free(quoted);
        return out;
    }

    size_t cmdlen = strlen(command);
    size_t elen = strlen(elevator);

    /*
     * Worst case:
     * every character starts a command.
     * Ridiculously oversized but simple.
     */
    size_t cap = cmdlen * (elen + 2) + cmdlen + 64;

    char *out = malloc(cap);
    size_t used = 0;

    bool in_single = false;
    bool in_double = false;
    bool escaped = false;
    bool need_elevator = true;

    for (const char *p = command; *p; p++) {

        if (need_elevator) {

            while (*p && isspace((unsigned char)*p)) {
                out[used++] = *p;
                p++;
            }

            if (!*p) {
                break;
            }

            size_t elen = strlen(elevator);
            memcpy(out + used, elevator, elen);
            used += elen;
            out[used++] = ' ';

            need_elevator = false;
        }

        char c = *p;

        if (escaped) {
            escaped = false;
            out[used++] = c;
            continue;
        }

        if (c == '\\') {
            escaped = true;
            out[used++] = c;
            continue;
        }

        if (!in_double && c == '\'') {
            in_single = !in_single;
            out[used++] = c;
            continue;
        }

        if (!in_single && c == '"') {
            in_double = !in_double;
            out[used++] = c;
            continue;
        }

        if (!in_single && !in_double) {

            if (c == ';') {
                out[used++] = c;
                need_elevator = true;
                continue;
            }

            if (c == '&' && p[1] == '&') {
                out[used++] = '&';
                out[used++] = '&';
                p++;
                need_elevator = true;
                continue;
            }

            if (c == '|' && p[1] == '|') {
                out[used++] = '|';
                out[used++] = '|';
                p++;
                need_elevator = true;
                continue;
            }
        }

        out[used++] = c;
    }

    out[used] = '\0';
    return out;
}

/* elevate a commmand by inserting sudo or doas where needed
 example:
 input: `apt update && apt upgrade` (notice that's two commands) will
 output: `sudo apt update && sudo apt upgrade` (notice sudo (or doas) is
 inserted where needed, not just at the begninning)
 NOTE: you must free it */
const char *elevate_command(const char *command) {
    // check if the command is rm or mv
    if (strncmp(command, "rm ", 3) == 0 || strncmp(command, "mv ", 3) == 0) {
        // extract the command name for the prompt
        const char *cmd_name = (strncmp(command, "rm ", 3) == 0) ? "RM" : "MV";
        printf("You are using the %s command, are you sure to elevate it? (y/n): ", cmd_name);
        char confirmation = getchar();
        // y/n, strip newline
        if (confirmation != '\n' && confirmation != EOF) {
            int c;
            while ((c = getchar()) != '\n' && c != EOF);
        }
        if (confirmation != 'Y' && confirmation != 'y') {
            exit(EXIT_FAILURE);
        }
    }

    return examine_command_and_insert_elevator(command);
}

