/* compat: for unix-like systems only */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <stdbool.h>

#include "../have.h"


/*
 * HAVE -- returns 1 if the command is found in PATH and executable,
 * returns 0 if not. Idea: check if a command exists via checking PATH.
 */

int command_exists(const char *command)
{
	if (command == NULL || *command == '\0') {
		return 0;
	}

	/* handle explicit paths like ./foo or /usr/bin/foo */
	if (strchr(command, '/')) {
		return access(command, X_OK) == 0;
	}

	const char *path_env = getenv("PATH");
	if (!path_env) { return 0; }

	char *path_copy = strdup(path_env);
	if (!path_env) { return 0; }

	int found = 0;
	char *dir = strtok(path_copy, ":");

	while (dir) {
		char fullpath[PATH_MAX];

		if (snprintf(fullpath, sizeof(fullpath),
					"%s/%s", dir, command) < (int)sizeof(fullpath)) {
			if (access(fullpath, X_OK) == 0) {
				found = 1;
				break;
			}
		}

		dir = strtok(NULL, ":");
	}

	free(path_copy);
	return found;
}

const char *which(const char *command) {
	if (command == NULL || *command == '\0') {
		return NULL;
	}

	if (strchr(command, '/')) {
		return access(command, X_OK) == 0 ? command : NULL;
	}

	const char *path_env = getenv("PATH");
	if (!path_env) { return NULL; }

	char *path_copy = strdup(path_env);
	if (!path_copy) { return NULL; }

	const char *result = NULL;
	char *dir = strtok(path_copy, ":");

	while (dir) {
		char fullpath[PATH_MAX];

		if (snprintf(fullpath, sizeof(fullpath),
					"%s/%s", dir, command) < (int)sizeof(fullpath)) {
			if (access(fullpath, X_OK) == 0) {
				result = strdup(fullpath);
				break;
			}
		}

		dir = strtok(NULL, ":");
	}

	free(path_copy);
	return result;
}