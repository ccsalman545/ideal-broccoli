#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "mongoose.h"
#include "http_server.h"

struct HttpServer {
    struct mg_mgr mgr;
    struct mg_connection *listener;
    bool running;
};

/*
 * HTTP/WebSocket event handler
 */
static void http_event_handler(struct mg_connection *c,
                               int ev,
                               void *ev_data)
{
    (void) ev_data;

    switch (ev) {

    case MG_EV_HTTP_MSG: {
        struct mg_http_message *hm = (struct mg_http_message *) ev_data;

        /*
         * WebSocket endpoint
         */
        if (mg_match(hm->uri, mg_str("/ws"), NULL)) {
            mg_ws_upgrade(c, hm, NULL);
            return;
        }

        /*
         * Simple HTTP status page
         */
        if (mg_match(hm->uri, mg_str("/"), NULL)) {

            mg_http_reply(
                c,
                200,
                "Content-Type: text/html\r\n",
                "<!DOCTYPE html>"
                "<html>"
                "<head>"
                "<meta charset=\"UTF-8\">"
                "<title>Camera Server</title>"
                "</head>"
                "<body>"
                "<h1>Camera Server</h1>"
                "<p>HTTP server is running.</p>"
                "<p>WebSocket endpoint: /ws</p>"
                "</body>"
                "</html>"
            );

            return;
        }

        /*
         * Unknown HTTP path
         */
        mg_http_reply(
            c,
            404,
            "Content-Type: text/plain\r\n",
            "404 Not Found\n"
        );

        break;
    }

    case MG_EV_WS_OPEN:
        printf("WebSocket client connected\n");

        {
            const char *message = "Camera server WebSocket connected";

            mg_ws_send(
                c,
                message,
                strlen(message),
                WEBSOCKET_OP_TEXT
            );
        }

        break;

    case MG_EV_WS_MSG: {
        struct mg_ws_message *wm = (struct mg_ws_message *) ev_data;

        printf(
            "WebSocket message received: %.*s\n",
            (int) wm->data.len,
            wm->data.buf
        );

        /*
         * Echo the received message.
         */
        mg_ws_send(
            c,
            wm->data.buf,
            wm->data.len,
            WEBSOCKET_OP_TEXT
        );

        break;
    }

    case MG_EV_CLOSE:
        printf("Client connection closed\n");
        break;

    default:
        break;
    }
}


/*
 * Start the HTTP/WebSocket server.
 */
HttpServer *http_server_start(
    const char *listen_address,
    uint16_t port)
{
    HttpServer *server = calloc(1, sizeof(*server));

    if (server == NULL) {
        fprintf(stderr, "Failed to allocate HttpServer\n");
        return NULL;
    }

    mg_mgr_init(&server->mgr);

    char address[64];

    snprintf(
        address,
        sizeof(address),
        "%s:%u",
        listen_address,
        (unsigned int) port
    );

    printf("Starting HTTP server on %s\n", address);

    server->listener = mg_http_listen(
        &server->mgr,
        address,
        http_event_handler,
        server
    );

    if (server->listener == NULL) {
        fprintf(stderr, "Failed to start HTTP server on %s\n", address);

        mg_mgr_free(&server->mgr);
        free(server);

        return NULL;
    }

    server->running = true;

    printf("HTTP server started successfully\n");
    printf("HTTP endpoint: http://%s/\n", address);
    printf("WebSocket endpoint: ws://%s/ws\n", address);

    return server;
}


/*
 * Run the Mongoose event loop.
 */
void http_server_run(HttpServer *server)
{
    if (server == NULL) {
        return;
    }

    printf("HTTP server event loop started\n");

    while (server->running) {
        mg_mgr_poll(&server->mgr, 1000);
    }

    printf("HTTP server event loop stopped\n");
}


/*
 * Stop the server.
 */
void http_server_stop(HttpServer *server)
{
    if (server == NULL) {
        return;
    }

    server->running = false;
}


/*
 * Destroy the server and release resources.
 */
void http_server_destroy(HttpServer *server)
{
    if (server == NULL) {
        return;
    }

    server->running = false;

    mg_mgr_free(&server->mgr);

    free(server);

    printf("HTTP server destroyed\n");
}
