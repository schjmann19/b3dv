#ifndef _SIMPLE_STRINGS_H
#define _SIMPLE_STRINGS_H

#include <stdbool.h>
#include <stdlib.h>

/*
 * Strings, but they feel natural, but they're a string.
 * rule of thumb: `String str; str.data` is the same as `char *str; str` in normal C.
 * that's the idiomatic way to use it as a normal C string.
 */

typedef struct String {
    char *data;
    size_t len;
    size_t cap;
} string;

#define str string

/* Forward declarations */
str str_create(const char *src);
str str_with_cap(size_t cap);
void str_destroy(str *s);
const char *cstr(const str *s);
str *str_append(str *s, const char *suffix);
str *str_prepend(str *s, const char *prefix);
str *str_trim(str *s);
str *str_ltrim(str *s);
str *str_rtrim(str *s);
str *to_upper(str *s);
str *to_lower(str *s);
bool str_ends_with(const str *s, const char *suffix);
bool str_starts_with(const str *s, const char *prefix);
const char *str_find(const str *s, const char *substr);
str str_quoted_substring(const char *src);
str str_join(const char *sep, const char **strs, size_t count);
str str_repeat(const char *src, int n);
str str_replace(const char *src, const char *old, const char *new_str);
str str_remove_prefix(const char *src, const char *prefix);
str str_remove_suffix(const char *src, const char *suffix);
bool str_eq(const str *a, const str *b);
int str_cmp(const str *a, const str *b);
void str_clear(str *s);
void str_reserve(str *s, size_t cap);
str str_substr(const str *s, size_t start, size_t len);
str *str_insert(str *s, size_t pos, const char *substr);
str *str_erase(str *s, size_t pos, size_t len);
str str_clone(const str *s);
str *str_split(const str *s, const char *delim, size_t *out_count);

#define to_cstr(s) cstr(s) /* alias for cstr() */
#define NULL_STRING str_create("")

#endif /* _SIMPLE_STRINGS_H */

/* Implementation */
#ifdef SIMPLE_STRINGS_IMPLEMENTATION
#include "src/strings.c"
#endif
