/* compat: for unix-like systems only */

/* simple HTTPS server. Serves argv[1], or "index.html"
 *
 * Requires OpenSSL: link with -lssl -lcrypto
 * Requires a cert/key pair. Generate a self-signed one for testing with:
 *   openssl req -x509 -newkey rsa:2048 -nodes -keyout key.pem -out cert.pem -days 365
 *
 * Cert/key paths default to "cert.pem" / "key.pem" in the working directory.
 * Override with the HTTPSRV_CERT / HTTPSRV_KEY environment variables.
 */

#include <arpa/inet.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <openssl/err.h>
#include <openssl/ssl.h>

#include "../httpsrv.h"

#define BUFFER_SIZE 4096

static volatile sig_atomic_t running = 1;

static void handle_sigint(int sig) {
    (void)sig;   /* unused */
    running = 0; /* just set the flag to 0 */
}

/* Write exactly `len` bytes over TLS, looping on partial writes / retryable
 * SSL conditions. Returns 0 on success, -1 on failure. */
static int ssl_write_all(SSL *ssl, const char *buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        int n = SSL_write(ssl, buf + sent, (int)(len - sent));
        if (n > 0) {
            sent += (size_t)n;
            continue;
        }
        int err = SSL_get_error(ssl, n);
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
            continue; /* retry, per SSL_write's documented retry semantics */
        }
        return -1;
    }
    return 0;
}

static SSL_CTX *create_ssl_context(void) {
    const SSL_METHOD *method = TLS_server_method();
    SSL_CTX *ctx = SSL_CTX_new(method);
    if (!ctx) {
        ERR_print_errors_fp(stderr);
        return NULL;
    }

    /* Reasonable modern floor; bump further if you don't need old clients. */
    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);

    /* Security options: disable compression and prefer server cipher order */
    SSL_CTX_set_options(ctx,
        SSL_OP_NO_COMPRESSION | SSL_OP_CIPHER_SERVER_PREFERENCE);

    return ctx;
}

static int load_cert_and_key(SSL_CTX *ctx) {
    const char *cert_path = getenv("HTTPSRV_CERT");
    const char *key_path = getenv("HTTPSRV_KEY");
    if (!cert_path) { cert_path = "cert.pem"; }
    if (!key_path) { key_path = "key.pem"; }

    if (SSL_CTX_use_certificate_file(ctx, cert_path, SSL_FILETYPE_PEM) <= 0) {
        fprintf(stderr, "failed to load certificate: %s\n", cert_path);
        ERR_print_errors_fp(stderr);
        return -1;
    }

    if (SSL_CTX_use_PrivateKey_file(ctx, key_path, SSL_FILETYPE_PEM) <= 0) {
        fprintf(stderr, "failed to load private key: %s\n", key_path);
        ERR_print_errors_fp(stderr);
        return -1;
    }

    if (!SSL_CTX_check_private_key(ctx)) {
        fprintf(stderr, "private key does not match certificate\n");
        return -1;
    }

    return 0;
}

#ifdef _STANDALONE_HTTPSRV
int main(int argc, char **argv)
#else
int https_server(const char *file_to_serve_path)
#endif
{
#ifdef _STANDALONE_HTTPSRV
    (void)argc; /* unused */
#endif
/* get which file to serve: */
#ifdef _STANDALONE_HTTPSRV
    const char *FILE_TO_SERVE = argv[1] ? argv[1] : "index.html";
#else
    const char *FILE_TO_SERVE = file_to_serve_path ? file_to_serve_path : "index.html";
#endif
    int port = 443;

    struct sigaction sa = {0};
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) < 0) {
        perror("sigaction");
        return EXIT_FAILURE;
    }

    SSL_CTX *ctx = create_ssl_context();
    if (!ctx) {
        return EXIT_FAILURE;
    }
    if (load_cert_and_key(ctx) < 0) {
        SSL_CTX_free(ctx);
        return EXIT_FAILURE;
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0) {
        perror("socket");
        SSL_CTX_free(ctx);
        return EXIT_FAILURE;
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        SSL_CTX_free(ctx);
        close(server_fd);
        return EXIT_FAILURE;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        SSL_CTX_free(ctx);
        return EXIT_FAILURE;
    }

    if (listen(server_fd, 10) < 0) {
        perror("listen");
        SSL_CTX_free(ctx);
        return EXIT_FAILURE;
    }

#ifndef _ONESHOT_HTTPSRV
    do {
#endif
        /* server shall serve now */
        printf("serving https on port %d\n", port);

        int client = accept(server_fd, NULL, NULL);
        if (client < 0) {
            perror("accept");
#ifndef _ONESHOT_HTTPSRV
            continue;
#else
            SSL_CTX_free(ctx);
            close(server_fd);
            return EXIT_FAILURE;
#endif
        }

        SSL *ssl = SSL_new(ctx);
        if (!ssl) {
            ERR_print_errors_fp(stderr);
            close(client);
#ifndef _ONESHOT_HTTPSRV
            continue;
#else
            SSL_CTX_free(ctx);
            close(server_fd);
            return EXIT_FAILURE;
#endif
        }

        if (SSL_set_fd(ssl, client) <= 0) {
            ERR_print_errors_fp(stderr);
            SSL_free(ssl);
            close(client);
#ifndef _ONESHOT_HTTPSRV
            continue;
#else
            SSL_CTX_free(ctx);
            close(server_fd);
            return EXIT_FAILURE;
#endif
        }

        if (SSL_accept(ssl) <= 0) {
            /* Handshake failed (bad cert, client bailed, plain-HTTP request
             * hitting a TLS port, etc). Log and move on instead of dying. */
            ERR_print_errors_fp(stderr);
            SSL_free(ssl);
            close(client);
#ifndef _ONESHOT_HTTPSRV
            continue;
#else
            SSL_CTX_free(ctx);
            return EXIT_FAILURE;
#endif
        }

        /* Read HTTP request until we see \r\n\r\n (end of headers) */
        char request[BUFFER_SIZE];
        size_t request_len = 0;
        int read_complete = 0;

        while (request_len < sizeof(request) - 1 && !read_complete) {
            int n = SSL_read(ssl, request + request_len,
                           (int)(sizeof(request) - 1 - request_len));

            if (n > 0) {
                request_len += (size_t)n;
                /* Check for end of HTTP headers (\r\n\r\n) */
                if (request_len >= 4) {
                    if (strstr(request, "\r\n\r\n") != NULL) {
                        read_complete = 1;
                    }
                }
            } else if (n == 0) {
                /* Connection closed by peer */
                break;
            } else {
                int err = SSL_get_error(ssl, n);
                if (err == SSL_ERROR_ZERO_RETURN) {
                    /* Clean close */
                    break;
                } else if (err == SSL_ERROR_WANT_READ ||
                          err == SSL_ERROR_WANT_WRITE) {
                    /* Retry (would block) */
                    continue;
                } else {
                    /* Fatal error */
                    fprintf(stderr, "SSL_read error: %d\n", err);
                    break;
                }
            }
        }

        /* NUL-terminate the request */
        request[request_len] = '\0';

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
            ssl_write_all(ssl, notfound, strlen(notfound));
            /* Proper TLS shutdown: two-phase close-notify exchange */
            int ret = SSL_shutdown(ssl);
            if (ret == 0) {
                SSL_shutdown(ssl);
            }
            SSL_free(ssl);
            close(client);
#ifndef _ONESHOT_HTTPSRV
            continue;
#else
            SSL_CTX_free(ctx);
            return EXIT_SUCCESS;
#endif
        }

        if (fseek(f, 0, SEEK_END) < 0) {
            perror("fseek");
            fclose(f);
            const char *error_response =
                "HTTP/1.1 500 Internal Server Error\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 21\r\n"
                "Connection: close\r\n"
                "\r\n"
                "500 Internal Error";
            ssl_write_all(ssl, error_response, strlen(error_response));
            int ret = SSL_shutdown(ssl);
            if (ret == 0) {
                SSL_shutdown(ssl);
            }
            SSL_free(ssl);
            close(client);
#ifndef _ONESHOT_HTTPSRV
            continue;
#else
            SSL_CTX_free(ctx);
            close(server_fd);
            return EXIT_FAILURE;
#endif
        }

        long len = ftell(f);
        if (len < 0) {
            perror("ftell");
            fclose(f);
            const char *error_response =
                "HTTP/1.1 500 Internal Server Error\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 21\r\n"
                "Connection: close\r\n"
                "\r\n"
                "500 Internal Error";
            ssl_write_all(ssl, error_response, strlen(error_response));
            int ret = SSL_shutdown(ssl);
            if (ret == 0) {
                SSL_shutdown(ssl);
            }
            SSL_free(ssl);
            close(client);
#ifndef _ONESHOT_HTTPSRV
            continue;
#else
            SSL_CTX_free(ctx);
            close(server_fd);
            return EXIT_FAILURE;
#endif
        }

        if (fseek(f, 0, SEEK_SET) < 0) {
            perror("fseek");
            fclose(f);
            const char *error_response =
                "HTTP/1.1 500 Internal Server Error\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 21\r\n"
                "Connection: close\r\n"
                "\r\n"
                "500 Internal Error";
            ssl_write_all(ssl, error_response, strlen(error_response));
            int ret = SSL_shutdown(ssl);
            if (ret == 0) {
                SSL_shutdown(ssl);
            }
            SSL_free(ssl);
            close(client);
#ifndef _ONESHOT_HTTPSRV
            continue;
#else
            SSL_CTX_free(ctx);
            close(server_fd);
            return EXIT_FAILURE;
#endif
        }

        char *body = (char *)malloc((size_t)len);
        if (!body) {
            perror("malloc");
            fclose(f);
            const char *error_response =
                "HTTP/1.1 500 Internal Server Error\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 21\r\n"
                "Connection: close\r\n"
                "\r\n"
                "500 Internal Error";
            ssl_write_all(ssl, error_response, strlen(error_response));
            int ret = SSL_shutdown(ssl);
            if (ret == 0) {
                SSL_shutdown(ssl);
            }
            SSL_free(ssl);
            close(client);
#ifndef _ONESHOT_HTTPSRV
            continue;
#else
            SSL_CTX_free(ctx);
            close(server_fd);
            return EXIT_FAILURE;
#endif
        }

        size_t bytes_read = fread(body, 1, (size_t)len, f);
        if (bytes_read != (size_t)len) {
            fprintf(stderr, "fread: expected %ld bytes, got %zu\n", len, bytes_read);
            free(body);
            fclose(f);
            const char *error_response =
                "HTTP/1.1 500 Internal Server Error\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 21\r\n"
                "Connection: close\r\n"
                "\r\n"
                "500 Internal Error";
            ssl_write_all(ssl, error_response, strlen(error_response));
            int ret = SSL_shutdown(ssl);
            if (ret == 0) {
                SSL_shutdown(ssl);
            }
            SSL_free(ssl);
            close(client);
#ifndef _ONESHOT_HTTPSRV
            continue;
#else
            SSL_CTX_free(ctx);
            close(server_fd);
            return EXIT_FAILURE;
#endif
        }
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
        ssl_write_all(ssl, header, (size_t)n);
        ssl_write_all(ssl, body, (size_t)len);

        free(body);

        /* Proper TLS shutdown: two-phase close-notify exchange */
        int ret = SSL_shutdown(ssl);
        if (ret == 0) {
            /* We sent close_notify, peer hasn't sent one yet. Try again. */
            SSL_shutdown(ssl);
        }
        /* If ret < 0, error occurred but we're closing anyway */

        SSL_free(ssl);
        close(client);
#ifndef _ONESHOT_HTTPSRV
    } while (running);
#endif

    /* cleanup */
    SSL_CTX_free(ctx);
    close(server_fd);

    return EXIT_SUCCESS;
}