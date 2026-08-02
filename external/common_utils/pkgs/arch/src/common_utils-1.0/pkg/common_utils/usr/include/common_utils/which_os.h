#ifndef WHICH_OS
#define WHICH_OS

#include <stdbool.h>

#if defined(_WIN32) || defined(_WIN64)
const char *try_to_determine_windows_version(void);
#elif defined(__linux__)
const char *try_to_determine_linux_distro(const bool like);
#else
extern const char *os;
#endif

#endif /* WHICH_OS */

/* Implementation */
#ifdef WHICH_OS_IMPLEMENTATION
#include "src/which_os.c"
#endif