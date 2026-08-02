/* compat: for unix-like systems only */

#ifndef TEXTURAWASD_PATH_UTILS_H
#define TEXTURAWASD_PATH_UTILS_H

#include "simple_strings.h"

/* expand `~` to home directory of the user */
str expand_home(const char *path);

/* join paths */
str path_join(const char *a, const char *b);

/* actual real path */
str canonical_path(const char *path);

/* get the file's extension. not for files with no extension or hidden */
str file_extension(const char *path);

/* clean up the path (remove duplicated `/`, only leave the final `/` if the last item is actually a directory)*/
str tidy_up_path(const char *path);

#endif /* TEXTURAWASD_PATH_UTILS_H */

/* Implementation */
#ifdef PATH_UTILS_IMPLEMENTATION
#include "src/path_utils.c"
#endif