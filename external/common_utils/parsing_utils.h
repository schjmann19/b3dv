/* compat: pure C */

#ifndef TEXTURAWASD_PARSING_UTILS_H
#define TEXTURAWASD_PARSING_UTILS_H

#include <stdbool.h>

int parse_int(const char *s);
long parse_long(const char *s);
bool parse_bool(const char *s);
bool is_numeric(const char *s);

#endif /* TEXTURAWASD_PARSING_UTILS_H */

/* Implementation */
#ifdef PARSING_UTILS_IMPLEMENTATION
#include "src/parsing_utils.c"
#endif
