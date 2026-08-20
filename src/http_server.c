#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "mongoose.h"
#include "http_server.h"
#include "frame_stream.h"

struct HttpServer {
    struct mg_mgr mgr;
    struct mg_connection *listener;

    FrameStream *frame_stream;

    bool running;
};


/*
 * HTTP + WebSocket event handler.
 */
static void http_event_handler(
    struct mg_connection *c,
    int ev,
    void *ev_data)
{
    HttpServer *server =
        (HttpServer *) c->fn_data;

    switch (ev) {

    /*
     * HTTP request received.
     */
    case MG_EV_HTTP_MSG: {
        struct mg_http_message *hm =
            (struct mg_http_message *) ev_data;

        /*
         * WebSocket endpoint.
         */
        if (mg_match(
                hm->uri,
                mg_str("/ws"),
                NULL)) {

            mg_ws_upgrade(
                c,
                hm,
                NULL
            );

            return;
        }

        /*
         * Main HTTP status page.
         */
        if (mg_match(
                hm->uri,
                mg_str("/"),
                NULL)) {

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
                "<p>Camera frame streaming is enabled.</p>"
                "</body>"
                "</html>"
            );

            return;
        }

        /*
         * Unknown HTTP path.
         */
        mg_http_reply(
            c,
            404,
            "Content-Type: text/plain\r\n",
            "404 Not Found\n"
        );

        break;
    }


    /*
     * WebSocket connection established.
     */
    case MG_EV_WS_OPEN: {

        printf(
            "WebSocket client connected\n"
        );

        /*
         * Register this connection as
         * the current frame-stream client.
         */
        if (server != NULL &&
            server->frame_stream != NULL) {

            frame_stream_set_client(
                server->frame_stream,
                c
            );
        }

        /*
         * Send connection confirmation.
         */
        const char *message =
            "Camera server WebSocket connected";

        mg_ws_send(
            c,
            message,
            strlen(message),
            WEBSOCKET_OP_TEXT
        );

        break;
    }


    /*
     * WebSocket message received.
     */
    case MG_EV_WS_MSG: {

        struct mg_ws_message *wm =
            (struct mg_ws_message *) ev_data;

        printf(
            "WebSocket message received: %.*s\n",
            (int) wm->data.len,
            wm->data.buf
        );

        /*
         * Echo the received message.
         *
         * This is currently used as a basic
         * communication test.
         */
        mg_ws_send(
            c,
            wm->data.buf,
            wm->data.len,
            WEBSOCKET_OP_TEXT
        );

        break;
    }


    /*
     * Connection closed.
     */
    case MG_EV_CLOSE: {

        printf(
            "Client connection closed\n"
        );

        /*
         * Remove the connection from the
         * frame streaming context.
         */
        if (server != NULL &&
            server->frame_stream != NULL) {

            frame_stream_clear_client(
                server->frame_stream,
                c
            );
        }

        break;
    }


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
    HttpServer *server =
        calloc(1, sizeof(*server));

    if (server == NULL) {
        fprintf(
            stderr,
            "Failed to allocate HttpServer\n"
        );

        return NULL;
    }

    /*
     * Initialize Mongoose manager.
     */
    mg_mgr_init(
        &server->mgr
    );

    /*
     * Create frame-stream context.
     */
    server->frame_stream =
        frame_stream_create();

    if (server->frame_stream == NULL) {

        fprintf(
            stderr,
            "Failed to create FrameStream\n"
        );

        mg_mgr_free(
            &server->mgr
        );

        free(server);

        return NULL;
    }

    /*
     * Construct listen address.
     *
     * Example:
     *
     * 192.168.1.10:8080
     */
    char address[64];

    snprintf(
        address,
        sizeof(address),
        "%s:%u",
        listen_address,
        (unsigned int) port
    );

    printf(
        "Starting HTTP server on %s\n",
        address
    );

    /*
     * Start Mongoose HTTP listener.
     *
     * The HttpServer pointer is stored in
     * c->fn_data and retrieved by the
     * event handler.
     */
    server->listener =
        mg_http_listen(
            &server->mgr,
            address,
            http_event_handler,
            server
        );

    if (server->listener == NULL) {

        fprintf(
            stderr,
            "Failed to start HTTP server on %s\n",
            address
        );

        frame_stream_destroy(
            server->frame_stream
        );

        mg_mgr_free(
            &server->mgr
        );

        free(server);

        return NULL;
    }

    server->running = true;

    printf(
        "HTTP server started successfully\n"
    );

    printf(
        "HTTP endpoint: http://%s/\n",
        address
    );

    printf(
        "WebSocket endpoint: ws://%s/ws\n",
        address
    );

    printf(
        "Press Ctrl+C to stop the server.\n"
    );

    return server;
}


/*
 * Process Mongoose events once.
 *
 * This function is useful when the camera
 * capture loop needs to run alongside the
 * HTTP/WebSocket event loop.
 */
void http_server_poll(
    HttpServer *server,
    int timeout_ms)
{
    if (server == NULL) {
        return;
    }

    if (!server->running) {
        return;
    }

    mg_mgr_poll(
        &server->mgr,
        timeout_ms
    );
}


/*
 * Run the Mongoose event loop continuously.
 */
void http_server_run(
    HttpServer *server)
{
    if (server == NULL) {
        return;
    }

    printf(
        "HTTP server event loop started\n"
    );

    while (server->running) {

        mg_mgr_poll(
            &server->mgr,
            1000
        );
    }

    printf(
        "HTTP server event loop stopped\n"
    );
}


/*
 * Send a camera frame through the
 * currently connected WebSocket client.
 */
int http_server_send_frame(
    HttpServer *server,
    const Frame *frame)
{
    if (server == NULL) {
        return -1;
    }

    if (server->frame_stream == NULL) {
        return -1;
    }

    if (frame == NULL) {
        return -1;
    }

    return frame_stream_send(
        server->frame_stream,
        frame
    );
}


/*
 * Stop the server.
 */
void http_server_stop(
    HttpServer *server)
{
    if (server == NULL) {
        return;
    }

    server->running = false;
}


/*
 * Destroy the server and release resources.
 */
void http_server_destroy(
    HttpServer *server)
{
    if (server == NULL) {
        return;
    }

    server->running = false;

    /*
     * Destroy frame streaming context.
     */
    if (server->frame_stream != NULL) {

        frame_stream_destroy(
            server->frame_stream
        );

        server->frame_stream = NULL;
    }

    /*
     * Release Mongoose resources.
     */
    mg_mgr_free(
        &server->mgr
    );

    free(server);

    printf(
        "HTTP server destroyed\n"
    );
}
