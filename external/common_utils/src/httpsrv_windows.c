/* compat: for Windows only (Winsock2) */

/* simple HTTP server. Serves argv[1], or "index.html"; port is argv[2] or 80 */

#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#include "../httpsrv.h"

#define BUFFER_SIZE 4096

static volatile sig_atomic_t running = 1;

static void handle_sigint(int sig) {
    (void)sig;   /* unused */
    running = 0; /* just set the flag to 0 */
}

#ifdef _STANDALONE_HTTPSRV
int main(int argc, char **argv)
#else
int http_server(const char *file_to_serve_path, int port)
#endif
{
#ifdef _STANDALONE_HTTPSRV
    (void)argc; /* unused */
#endif
/* get which file to serve: */
#ifdef _STANDALONE_HTTPSRV
    const char *FILE_TO_SERVE = argv[1] ? argv[1] : "index.html";
    int port = argv[2] ? atoi(argv[2]) : 80;
#else
    const char *FILE_TO_SERVE = file_to_serve_path ? file_to_serve_path : "index.html";
#endif

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return EXIT_FAILURE;
    }

    signal(SIGINT, handle_sigint);

    SOCKET server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd == INVALID_SOCKET) {
        fprintf(stderr, "socket failed: %d\n", WSAGetLastError());
        WSACleanup();
        return EXIT_FAILURE;
    }

    BOOL opt = TRUE;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((u_short)port);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
        fprintf(stderr, "bind failed: %d\n", WSAGetLastError());
        closesocket(server_fd);
        WSACleanup();
        return EXIT_FAILURE;
    }

    if (listen(server_fd, 10) == SOCKET_ERROR) {
        fprintf(stderr, "listen failed: %d\n", WSAGetLastError());
        closesocket(server_fd);
        WSACleanup();
        return EXIT_FAILURE;
    }

#ifndef _ONESHOT_HTTPSRV
    do {
#endif
        /* server shall serve now */
        printf("serving on port %d\n", port);

        SOCKET client = accept(server_fd, NULL, NULL);
        if (client == INVALID_SOCKET)
#ifndef _ONESHOT_HTTPSRV
            continue;
#else
    {
        fprintf(stderr, "accept failed: %d\n", WSAGetLastError());
        closesocket(server_fd);
        WSACleanup();
        return EXIT_FAILURE;
    }
#endif

        char request[BUFFER_SIZE];
        recv(client, request, sizeof(request) - 1, 0);

        FILE *f = fopen(FILE_TO_SERVE, "rb");
        if (!f) {
            puts("[!!!!!]: file not found");
            puts(FILE_TO_SERVE);
            const char *notfound =
                "HTTP/1.1 404 not found :(\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 13\r\n"
                "Connection: close\r\n"
                "\r\n"
                "404 Not Found";
            send(client, notfound, (int)strlen(notfound), 0);
            closesocket(client);
#ifndef _ONESHOT_HTTPSRV
            continue;
#else
        closesocket(server_fd);
        WSACleanup();
        return EXIT_SUCCESS;
#endif
        }

        fseek(f, 0, SEEK_END);
        long len = ftell(f);
        rewind(f);

        char *body = malloc(len);
        fread(body, 1, len, f);
        fclose(f);

        char header[256];
        int n = snprintf(
            header, sizeof(header),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: %ld\r\n"
            "Connection: close\r\n"
            "\r\n",
            len);
        send(client, header, n, 0);
        send(client, body, (int)len, 0);

        free(body);
        closesocket(client);
#ifndef _ONESHOT_HTTPSRV
    } while (running);
#endif

    /* cleanup SHOULD happen here*/
    closesocket(server_fd);
    WSACleanup();

    return EXIT_SUCCESS; /* fixme */
}
