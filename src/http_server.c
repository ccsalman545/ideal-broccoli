#define _POSIX_C_SOURCE 200809L

#include "http_server.h"
#include "mongoose.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct HttpServer {
    struct mg_mgr mgr;
    struct mg_connection *listener;
    int running;
};

static const char *HTML_PAGE =
    "<!doctype html>"
    "<html>"
    "<head>"
    "<meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>Camera Transport Server</title>"
    "<style>"
    "body{font-family:Arial,sans-serif;margin:40px;}"
    "pre{padding:15px;background:#f2f2f2;border-radius:8px;}"
    "button{padding:10px 16px;margin-top:10px;cursor:pointer;}"
    "</style>"
    "</head>"
    "<body>"
    "<h1>Camera Transport Server</h1>"
    "<p>HTTP Status: <strong>Online</strong></p>"
    "<pre id=\"status\">"
    "Host: Fedora 44\n"
    "Camera: /dev/video0\n"
    "Format: YUYV 4:2:2\n"
    "Resolution: 640x480\n"
    "Frame rate: 30 FPS"
    "</pre>"
    "<button onclick=\"connectWS()\">Connect WebSocket</button>"
    "<pre id=\"ws\">WebSocket: not connected</pre>"
    "<script>"
    "let ws;"
    "function connectWS() {"
    "  if (ws && ws.readyState === WebSocket.OPEN) return;"
    "  ws = new WebSocket('ws://' + location.host + '/ws');"
    "  ws.onopen = function() {"
    "    document.getElementById('ws').textContent ="
    "      'WebSocket: connected';"
    "    ws.send(JSON.stringify({type:'hello',client:'ubuntu'}));"
    "  };"
    "  ws.onmessage = function(event) {"
    "    document.getElementById('ws').textContent ="
    "      'Received: ' + event.data;"
    "  };"
    "  ws.onerror = function() {"
    "    document.getElementById('ws').textContent ="
    "      'WebSocket: error';"
    "  };"
    "  ws.onclose = function() {"
    "    document.getElementById('ws').textContent ="
    "      'WebSocket: closed';"
    "  };"
    "}"
    "</script>"
    "</body>"
    "</html>";

static void send_text(
    struct mg_connection *connection,
    const char *text
)
{
    mg_ws_send(
        connection,
        text,
        strlen(text),
        WEBSOCKET_OP_TEXT
    );
}

static void http_event_handler(
    struct mg_connection *connection,
    int event,
    void *event_data
)
{
    if (event == MG_EV_HTTP_MSG) {
        struct mg_http_message *message = event_data;

        if (mg_match(
                message->uri,
                mg_str("/"),
                NULL
            )) {

            mg_http_reply(
                connection,
                200,
                "Content-Type: text/html; charset=utf-8\r\n",
                "%s",
                HTML_PAGE
            );

            return;
        }

        if (mg_match(
                message->uri,
                mg_str("/status"),
                NULL
            )) {

            mg_http_reply(
                connection,
                200,
                "Content-Type: application/json\r\n",
                "{"
                "\"status\":\"online\","
                "\"camera\":\"/dev/video0\","
                "\"format\":\"YUYV\","
                "\"width\":640,"
                "\"height\":480,"
                "\"fps\":30"
                "}"
            );

            return;
        }

        if (mg_match(
                message->uri,
                mg_str("/ws"),
                NULL
            )) {

            mg_ws_upgrade(
                connection,
                message,
                NULL
            );

            return;
        }

        mg_http_reply(
            connection,
            404,
            "Content-Type: text/plain\r\n",
            "Not Found\n"
        );

        return;
    }

    if (event == MG_EV_WS_OPEN) {
        printf("WebSocket client connected.\n");

        send_text(
            connection,
            "{\"type\":\"welcome\",\"server\":\"fedora\"}"
        );

        return;
    }

    if (event == MG_EV_WS_MSG) {
        struct mg_ws_message *message = event_data;

        printf(
            "WebSocket message received: %.*s\n",
            (int) message->data.len,
            message->data.buf
        );

        if (mg_strcmp(
                message->data,
                mg_str("{\"type\":\"hello\",\"client\":\"ubuntu\"}")
            ) == 0) {

            send_text(
                connection,
                "{\"type\":\"hello_ack\",\"server\":\"fedora\"}"
            );

        } else {

            send_text(
                connection,
                "{\"type\":\"message_received\",\"server\":\"fedora\"}"
            );
        }

        return;
    }

    if (event == MG_EV_CLOSE) {
        printf("Client connection closed.\n");
        return;
    }
}

HttpServer *http_server_start(
    const char *listen_address,
    uint16_t port
)
{
    if (!listen_address || port == 0) {
        fprintf(
            stderr,
            "Invalid HTTP server configuration\n"
        );
        return NULL;
    }

    HttpServer *server = calloc(1, sizeof(*server));

    if (!server) {
        perror("calloc");
        return NULL;
    }

    mg_mgr_init(&server->mgr);

    char url[128];

    snprintf(
        url,
        sizeof(url),
        "http://%s:%u",
        listen_address,
        port
    );

    printf(
        "Starting HTTP/WebSocket server at %s\n",
        url
    );

    server->listener = mg_http_listen(
        &server->mgr,
        url,
        http_event_handler,
        server
    );

    if (!server->listener) {
        fprintf(
            stderr,
            "Failed to start listener at %s\n",
            url
        );

        mg_mgr_free(&server->mgr);
        free(server);

        return NULL;
    }

    server->running = 1;

    printf(
        "HTTP/WebSocket server listening at %s\n",
        url
    );

    return server;
}

void http_server_run(HttpServer *server)
{
    if (!server)
        return;

    while (server->running) {
        mg_mgr_poll(
            &server->mgr,
            100
        );
    }
}

void http_server_stop(HttpServer *server)
{
    if (!server)
        return;

    server->running = 0;
}

void http_server_destroy(HttpServer *server)
{
    if (!server)
        return;

    mg_mgr_free(&server->mgr);
    free(server);
}
