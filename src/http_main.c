#include "http_server.h"

#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    HttpServer *server =
        http_server_start(
            "192.168.1.10",
            8080
        );

    if (!server) {
        fprintf(
            stderr,
            "Failed to start HTTP server\n"
        );

        return EXIT_FAILURE;
    }

    printf(
        "Press Ctrl+C to stop the server.\n"
    );

    http_server_run(server);

    http_server_destroy(server);

    return EXIT_SUCCESS;
}
