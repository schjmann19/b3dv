/* compat: for unix-like systems only */

#include <linux/limits.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "../path_utils.h"

/* expand `~` to home directory of the user */
str expand_home(const char *path)
{
    if (!path) { return NULL_STRING; }

    if (path[0] != '~') {
        return str_create(path);
    }

    const char *home = getenv("HOME");

    if (!home) { return NULL_STRING; }

    size_t hlen = strlen(home);
    size_t plen = strlen(path);

    char *buf = malloc(hlen + plen); /* remove '~', add '\0' */

    if (!buf) { return NULL_STRING; }

    memcpy(buf, home, hlen);

    /* copies "/foo\0" */
    memcpy(buf + hlen, path + 1, plen);

    str out = str_create(buf);

    free(buf);

    return out;
}

/* join paths */
str path_join(const char *a, const char *b)
{
    if (!a || !b) { return NULL_STRING; }

    size_t alen = strlen(a);
    size_t blen = strlen(b);

    int a_has_slash = (alen > 0 && a[alen - 1] == '/');
    int b_has_slash = (blen > 0 && b[0] == '/');

    size_t extra = (!a_has_slash && !b_has_slash) ? 1 : 0;
    size_t skip  = (a_has_slash && b_has_slash) ? 1 : 0;

    char *buf = malloc(alen + blen + extra - skip + 1);
    if (!buf) { return NULL_STRING; }

    memcpy(buf, a, alen);

    size_t pos = alen;

    if (extra) {
        buf[pos++] = '/';
    }

    memcpy(buf + pos, b + skip, blen - skip);
    pos += blen - skip;

    buf[pos] = '\0';

    str result = str_create(buf);
    free(buf);

    return result;
}

/* actual real path */
str canonical_path(const char *path)
{
    if (!path) { return NULL_STRING; }


    char resolved[PATH_MAX];

    if (!realpath(path, resolved)) { return NULL_STRING; }

    return str_create(resolved);
}

/* get the file's extension. not for files with no extension or hidden */
str file_extension(const char *path)
{
    if (!path) { return NULL_STRING; }

    const char *filename = strrchr(path, '/');
    filename = filename ? filename + 1 : path;

    const char *dot = strrchr(filename, '.');

    /* no extension or hidden file like ".gitignore" */
    if (!dot || dot == filename) { return NULL_STRING; }

    return str_create(dot + 1);
}

/* clean up the path (remove duplicated `/`, only leave the final `/` if the last item is actually a directory)*/
str tidy_up_path(const char *path)
{
    if (!path) { return NULL_STRING; }

    size_t len = strlen(path);

    /* worst case: output <= input + '\0' */
    char *buf = malloc(len + 1);

    if (!buf) { return NULL_STRING; }

    size_t i = 0;
    size_t j = 0;

    /* collapse repeated '/' */
    while (i < len) {

        buf[j++] = path[i];

        if (path[i] == '/') {
            while (i + 1 < len && path[i + 1] == '/')
                i++;
        }

        i++;
    }

    buf[j] = '\0';

    /*
     * Remove trailing '/'
     * unless:
     *   - it's just "/"
     *   - filesystem says it's a directory
     */

    if (j > 1 && buf[j - 1] == '/') {

        struct stat st;

        if (stat(buf, &st) != 0 || !S_ISDIR(st.st_mode)) {

            /* unknown path or regular file */
            buf[--j] = '\0';
        }
    }

    str out = str_create(buf);

    free(buf);

    return out;
}