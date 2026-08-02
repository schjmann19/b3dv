/* compat: for unix-like systems only */

/* simple HTTP server. Serves argv[1], or "index.html"; port is argv[2] or 80 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdatomic.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "../httpsrv.h"

#define BUFFER_SIZE 4096

static volatile sig_atomic_t running = 1;



static void handle_sigint(int sig){
    (void)sig;      /* unused */
    running = 0;    /* just set the flag to 0 */
}

#ifdef _STANDALONE_HTTPSRV
int main(int argc, char **argv)
#else
int http_server(const char *file_to_serve_path, int port)
#endif
{
    #ifdef _STANDALONE_HTTPSRV
    (void)argc;     /* unused */
    #endif
    /* get which file to serve: */
    #ifdef _STANDALONE_HTTPSRV
    const char *FILE_TO_SERVE = argv[1]? argv[1] : "index.html";
    int port = argv[2]? atoi(argv[2]) : 80;
    #else
    const char *FILE_TO_SERVE = file_to_serve_path? file_to_serve_path : "index.html";
    #endif

    struct sigaction sa = {0};
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0) { perror("socket"); return EXIT_FAILURE; }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return EXIT_FAILURE;
    }

    if (listen(server_fd, 10) < 0) {
        perror("listen");
        return EXIT_FAILURE;
    }

    #ifndef _ONESHOT_HTTPSRV
    do {
    #endif
        /* server shall serve now */
        printf("serving on port %d\n", port);

        int client = accept(server_fd, NULL, NULL);
        if (client < 0)
        #ifndef _ONESHOT_HTTPSRV
        continue;
        #else
        { perror("accept"); return EXIT_FAILURE; }
        #endif

        char request[BUFFER_SIZE];
        read(client, request, sizeof(request) - 1);

        FILE *f = fopen(FILE_TO_SERVE, "rb");
        if (!f) { puts("[!!!!!]: file not found"); puts(FILE_TO_SERVE);
            const char *notfound =
                "HTTP/1.1 404 not found :(\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 13\r\n"
                "Connection: close\r\n"
                "\r\n"
                "404 Not Found";
            write(client, notfound, strlen(notfound));
            close(client);
            #ifndef _ONESHOT_HTTPSRV
            continue;
            #else
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
        write(client, header, n);
        write(client, body, len);

        free(body);
        close(client);
    #ifndef _ONESHOT_HTTPSRV
    } while (running);
    #endif

    /* cleanup SHOULD happen here*/

    return EXIT_SUCCESS; /* fixme */
}
