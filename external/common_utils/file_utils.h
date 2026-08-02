#ifndef TEXTURAWASD_FILE_UTILS_H
#define TEXTURAWASD_FILE_UTILS_H

#include "simple_strings.h"
#include <stdbool.h>
#include <stdlib.h>

/* Check if a file exists at the given path */
bool file_exists(const char *path);

/* Check if a directory exists at the given path */
bool dir_exists(const char *path);

/* Check if a path is a symbolic link */
bool is_symlink(const char *path);

/* Get the size of a file in bytes */
size_t file_size(const char *path);

/* Read the entire contents of a file into a dynamically allocated string */
str read_entire_file(const char *path);

/* Write contents to a file */
void write_entire_file(const char *path, const char *contents);

/* Count the number of lines in a file */
int file_line_count(const char *path);

/* Check if a file has at least/most the specified number of lines
 * pass NULL as third argument to check exactly and not at least/most. */
bool file_is_lines_long(const char *path, int line_count, const char *least_or_most);

/* Read a specified number of lines from a file */
str read_lines(const char *path, int line_count, bool skip_empty, bool trim_newline);

#endif /* TEXTURAWASD_FILE_UTILS_H */

/* Implementation */
#ifdef FILE_UTILS_IMPLEMENTATION
#include "src/file_utils.c"
#endif