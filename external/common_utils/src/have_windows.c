/* compat: for Windows only */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include "../have.h"

/*
 * HAVE -- returns 1 if the command is found in PATH and executable,
 * returns 0 if not. Idea: check if a command exists via checking PATH.
 *
 * Windows notes:
 *   - PATH entries are ';'-separated, not ':'.
 *   - There's no executable bit; instead a file counts as executable if its
 *     extension is one of PATHEXT (.EXE, .BAT, .CMD, ...). If the command
 *     already has an extension, we check it as-is. If it has none, we try
 *     each PATHEXT suffix in turn, same as cmd.exe does.
 *   - SearchPathA already implements this whole resolution algorithm, so we
 *     lean on it instead of hand-rolling our own PATH walk.
 */

static bool has_extension(const char *command) {
    const char *slash = strrchr(command, '\\');
    const char *fwdslash = strrchr(command, '/');
    if (fwdslash && (!slash || fwdslash > slash)) {
        slash = fwdslash;
    }
    const char *base = slash ? slash + 1 : command;
    return strchr(base, '.') != NULL;
}

int command_exists(const char *command) {
    if (command == NULL || *command == '\0') {
        return 0;
    }

    /* handle explicit paths like .\foo, ./foo, C:\foo, or \\share\foo */
    if (strchr(command, '\\') || strchr(command, '/') ||
        (strlen(command) >= 2 && command[1] == ':')) {
        DWORD attrs = GetFileAttributesA(command);
        if (attrs == INVALID_FILE_ATTRIBUTES ||
            (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            return 0;
        }
        if (has_extension(command)) {
            return 1;
        }
        /* no extension on an explicit path: try PATHEXT suffixes */
        char found[MAX_PATH];
        return SearchPathA(NULL, command, NULL, sizeof(found), found, NULL) != 0;
    }

    /* command has no path separators: let SearchPathA walk PATH for us.
     * Passing NULL as the extension means: if `command` already has an
     * extension, use it as-is; otherwise try each PATHEXT suffix in turn,
     * same as cmd.exe would. */
    char found[MAX_PATH];
    DWORD len = SearchPathA(NULL, command, NULL, sizeof(found), found, NULL);

    return len != 0;
}

const char *which(const char *command) {
    if (command == NULL || *command == '\0') {
        return NULL;
    }

    if (strchr(command, '\\') || strchr(command, '/') ||
        (strlen(command) >= 2 && command[1] == ':')) {
        DWORD attrs = GetFileAttributesA(command);
        if (attrs == INVALID_FILE_ATTRIBUTES ||
            (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            return NULL;
        }
        return strdup(command);
    }

    char found[MAX_PATH];
    DWORD len = SearchPathA(NULL, command, NULL, sizeof(found), found, NULL);

    if (len == 0 || len >= sizeof(found)) {
        return NULL;
    }

    return strdup(found);
}