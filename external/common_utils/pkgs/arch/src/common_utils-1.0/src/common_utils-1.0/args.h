/* compat: pure C */

#ifndef TEXTURAWASD_ARG_H
#define TEXTURAWASD_ARG_H

#include <stdbool.h>
#include "simple_strings.h"

/*
 * Returns true if the named argument is present in any form:
 *   -verbose  --verbose  /verbose
 * Position-independent. */
bool arg_is_present(const char *arg, int argc, char **argv);

/*
 * Returns true if the named argument is present AND has the given value,
 * in any of these forms:
 *   --dir=/var/www/files   -dir=/var/www/files   /dir=/var/www/files
 */
bool arg_is_present_with_value(const char *arg, const char *value, int argc, char **argv);

/*
 * Returns true if the named argument appears at the nth position (1-based).
 * Prefixes are stripped before comparison, so --verbose, -verbose, /verbose
 * all match "verbose" at position n.
 */
bool arg_is_present_at_nth_position(const char *arg, int n, int argc, char **argv);

/*
 * Returns true if arg is found in a comma-separated list string.
 * e.g. arg_is_in_list("verbose", "help,verbose,debug") -> true
 */
bool arg_is_in_list(const char *arg, const char *list);

/*
 * Returns the value associated with the named key, or NULL if not found /
 * no value present. The returned pointer points into the original argv
 * string - do not free it.
 *   --dir=/var/www/files  =>  get_arg_value("dir", ...) returns "/var/www/files"
 */
str get_arg_value(const char *arg, int argc, char **argv);

/*
 * Returns true if the named key has the given value, regardless of prefix.
 *   --dir=/var/www/files  ->  config_arg_is_val("dir", "/var/www/files", ...) -> true
 */
bool config_arg_is_val(const char *key, const char *val, int argc, char **argv);

#endif /* TEXTURAWASD_ARG */

/* Implementation */
#ifdef ARGS_IMPLEMENTATION
#include "src/args.c"
#endif