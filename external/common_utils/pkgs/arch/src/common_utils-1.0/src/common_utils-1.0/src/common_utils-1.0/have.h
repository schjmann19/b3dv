/* compat: for unix-like and windows */

#ifndef HAVE_H
#define HAVE_H

int command_exists(const char *command);

const char *which(const char *command);

#endif /* HAVE_H */

/* Implementation */
#ifdef _WIN32
#ifdef HAVE_IMPLEMENTATION
#include "src/have_windows.c"
#endif
#else
#ifdef HAVE_IMPLEMENTATION
#include "src/have.c"
#endif
#endif