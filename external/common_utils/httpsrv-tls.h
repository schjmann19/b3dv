/* compat: for unix-like and windows */

#ifndef HTTPSRV_TLS_H
#define HTTPSRV_TLS_H

/* simple HTTPS server. Serves argv[1], or "index.html" */
int https_server(const char *file_to_serve_path);

#endif

/* Implementation */
#ifdef _WIN32
#ifdef _HTTPSRV_IMPLEMENTATION
#include "src/httpsrv-tls_windows.c"
#endif
#else
#ifdef _HTTPSRV_IMPLEMENTATION
#include "src/httpsrv-tls.c"
#endif
#endif