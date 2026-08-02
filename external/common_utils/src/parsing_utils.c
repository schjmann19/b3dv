/* compat: pure C */

#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "../parsing_utils.h"

/* strcasecmp is not part of C proper, so */
static int strcasecmp_local(const char *a, const char *b) {
    while (*a && *b) {

        int ca = tolower((unsigned char)*a);
        int cb = tolower((unsigned char)*b);

        if (ca != cb)
            return ca - cb;

        a++;
        b++;
    }

    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}
#define _strcasecmp strcasecmp_local

/* Parse a string into an integer */
int parse_int(const char *s) {
    if (!s) {
        return 0;
    }

    char *endptr;
    long val = strtol(s, &endptr, 10);

    if (*endptr != '\0') {
        return 0;
    }

    return (int)val;
}

/* Parse a string into a long integer */
long parse_long(const char *s) {
    if (!s) {
        return 0;
    }

    char *endptr;
    long val = strtol(s, &endptr, 10);

    if (*endptr != '\0') {
        return 0;
    }

    return val;
}

/*
 * Check if a string represents a boolean value.
 * Checks the checks to be checked, but defaults to false.
 * true/false, 1/0, yes/no, on/off enable(d)/disable(d)
 */
bool parse_bool(const char *s) {
    if (!s) {
        return false;
    } else if (_strcasecmp(s, "true") == 0 || strcmp(s, "1") == 0 || _strcasecmp(s, "yes") == 0 || _strcasecmp(s, "y") == 0 || _strcasecmp(s, "on") == 0 || _strcasecmp(s, "enable") == 0 || _strcasecmp(s, "enabled") == 0) {
        return true;
    } else if (_strcasecmp(s, "false") == 0 || strcmp(s, "0") == 0 || _strcasecmp(s, "no") == 0 || _strcasecmp(s, "n") == 0 || _strcasecmp(s, "off") == 0 || _strcasecmp(s, "disable") == 0 || _strcasecmp(s, "disabled") == 0) {
        return false;
    }
    return false;
}

/* Check if a string represents a valid number */
bool is_numeric(const char *s) {
    if (!s || *s == '\0') {
        return false;
    }

    char *p = (char *)s;
    if (*p == '-' || *p == '+') {
        p++;
    }

    bool is_numeric = false;
    while (*p) {
        if (!isdigit((unsigned char)*p)) {
            return false;
        }
        is_numeric = true;
        p++;
    }
    return is_numeric;
}