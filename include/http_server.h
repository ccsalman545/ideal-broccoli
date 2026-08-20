#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <stdint.h>

typedef struct HttpServer HttpServer;

HttpServer *http_server_start(
    const char *listen_address,
    uint16_t port
);

void http_server_run(HttpServer *server);

void http_server_stop(HttpServer *server);

void http_server_destroy(HttpServer *server);

#endif
