/* compat: i don't know */

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>

#include "../file_utils.h"

/* Check if a file exists at the given path */
bool file_exists(const char *path)
{
    if (!path) return false;
    struct stat statbuf;
    return stat(path, &statbuf) == 0 && S_ISREG(statbuf.st_mode);
}

/* Check if a directory exists at the given path */
bool dir_exists(const char *path)
{
    if (!path) return false;
    struct stat statbuf;
    return stat(path, &statbuf) == 0 && S_ISDIR(statbuf.st_mode);
}

/* Check if a path is a symbolic link */
bool is_symlink(const char *path)
{
    if (!path) return false;
    struct stat statbuf;
    return lstat(path, &statbuf) == 0 && S_ISLNK(statbuf.st_mode);
}

/* Get the size of a file in bytes */
size_t file_size(const char *path)
{
    if (!path) return 0;
    struct stat statbuf;
    if (stat(path, &statbuf) == 0 && S_ISREG(statbuf.st_mode)) {
        return (size_t)statbuf.st_size;
    }
    return 0;
}

/* Read the entire contents of a file into a dynamically allocated string */
str read_entire_file(const char *path)
{
    str result = NULL_STRING;
    if (!path) return result;

    FILE *file = fopen(path, "rb");
    if (!file) return result;

    /* Get file size */
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (size < 0) {
        fclose(file);
        return result;
    }

    /* Allocate buffer */
    result.cap = (size_t)size + 1;
    result.data = malloc(result.cap);
    if (!result.data) {
        fclose(file);
        return result;
    }

    /* Read file */
    size_t read_bytes = fread(result.data, 1, (size_t)size, file);
    fclose(file);

    result.len = read_bytes;
    result.data[result.len] = '\0';

    return result;
}

/* Write contents to a file */
void write_entire_file(const char *path, const char *contents)
{
    if (!path || !contents) return;

    FILE *file = fopen(path, "wb");
    if (!file) return;

    fputs(contents, file);
    fclose(file);
}

/* Count the number of lines in a file */
int file_line_count(const char *path)
{
    if (!path) return 0;

    FILE *file = fopen(path, "r");
    if (!file) return 0;

    int count = 0;
    int ch;
    bool had_content = false;

    while ((ch = fgetc(file)) != EOF) {
        if (ch == '\n') {
            count++;
        }
        if (ch != '\n') {
            had_content = true;
        }
    }

    /* Count last line if file doesn't end with newline */
    if (had_content && ch != '\n') {
        count++;
    }

    fclose(file);
    return count;
}

/* Check if a file has at least/most the specified number of lines
 * pass NULL as third argument to check exactly and not at least/most. */
bool file_is_lines_long(const char *path, int line_count, const char *least_or_most)
{
    if (!path || line_count < 0)
        return false;

    const char *mode = NULL;

    if (least_or_most) {
        mode =
            strstr(least_or_most, "least") ? "least" :
            strstr(least_or_most, "most")  ? "most"  :
            NULL;
    }

    FILE *file = fopen(path, "r");
    if (!file)
        return false;

    int count = 0;
    int ch;

    while ((ch = fgetc(file)) != EOF) {
        if (ch == '\n') {
            count++;

            if (mode && strcmp(mode, "least") == 0 && count >= line_count) {
                fclose(file);
                return true;
            }

            if (mode && strcmp(mode, "most") == 0 && count > line_count) {
                fclose(file);
                return false;
            }

            /* Exact mode: fail as soon as we exceed */
            if (!mode && count > line_count) {
                fclose(file);
                return false;
            }
        }
    }

    fclose(file);

    if (mode && strcmp(mode, "least") == 0)
        return count >= line_count;

    if (mode && strcmp(mode, "most") == 0)
        return count <= line_count;

    /* Exact mode */
    return count == line_count;
}

/* Read a specified number of lines from a file */
str read_lines(const char *path, int line_count, bool skip_empty, bool trim_newline)
{
    str result = NULL_STRING;
    if (!path || line_count <= 0) return result;

    FILE *file = fopen(path, "r");
    if (!file) return result;

    result = str_with_cap(256);
    int lines_read = 0;
    char buffer[4096];

    while (lines_read < line_count && fgets(buffer, sizeof(buffer), file)) {
        /* Check if line is empty and skip if needed */
        if (skip_empty) {
            bool is_empty = true;
            for (int i = 0; buffer[i]; i++) {
                if (buffer[i] != '\n' && buffer[i] != '\r' && buffer[i] != ' ' && buffer[i] != '\t') {
                    is_empty = false;
                    break;
                }
            }
            if (is_empty) continue;
        }

        /* Trim newline if requested */
        if (trim_newline) {
            size_t len = strlen(buffer);
            if (len > 0 && buffer[len - 1] == '\n') {
                buffer[len - 1] = '\0';
                if (len > 1 && buffer[len - 2] == '\r') {
                    buffer[len - 2] = '\0';
                }
            }
        }

        /* Append to result */
        str_append(&result, buffer);
        if (lines_read < line_count - 1 && !trim_newline) {
            /* Add newline if not trimming and not last line */
            str_append(&result, "\n");
        } else if (lines_read < line_count - 1 && trim_newline) {
            /* Add newline if trimming but still between lines */
            str_append(&result, "\n");
        }

        lines_read++;
    }

    fclose(file);
    return result;
}
