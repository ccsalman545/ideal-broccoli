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
    "body{font-family:Arial,sans-serif;margin:40px;max-width:900px;}"
    "pre{padding:15px;background:#f2f2f2;border-radius:8px;white-space:pre-wrap;word-break:break-word;}"
    "button{padding:10px 16px;margin:6px 6px 6px 0;cursor:pointer;}"
    ".ok{font-weight:bold;}"
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
    "<button onclick=\"startWebRTC()\">Start WebRTC Test</button>"
    "<pre id=\"ws\">WebSocket: not connected</pre>"
    "<pre id=\"webrtc\">WebRTC: idle</pre>"
    "<script>"
    "let ws = null;"
    "let pc = null;"
    "let dataChannel = null;"

    "function setText(id, text) {"
    "  document.getElementById(id).textContent = text;"
    "}"

    "function connectWS() {"
    "  if (ws && ws.readyState === WebSocket.OPEN) {"
    "    setText('ws', 'WebSocket: already connected');"
    "    return;"
    "  }"

    "  ws = new WebSocket('ws://' + location.host + '/ws');"

    "  ws.onopen = function() {"
    "    setText('ws', 'WebSocket: connected');"
    "    ws.send(JSON.stringify({type:'hello',client:'ubuntu'}));"
    "  };"

    "  ws.onmessage = function(event) {"
    "    let prefix = 'Received: ';"
    "    try {"
    "      const message = JSON.parse(event.data);"
    "      if (message.type === 'hello_ack') {"
    "        setText('ws', prefix + event.data);"
    "      } else if (message.type === 'offer_received') {"
    "        setText('ws', prefix + 'SDP offer acknowledged');"
    "      } else if (message.type === 'ice_received') {"
    "        setText('ws', prefix + 'ICE candidate acknowledged');"
    "      } else {"
    "        setText('ws', prefix + event.data);"
    "      }"
    "    } catch (e) {"
    "      setText('ws', prefix + event.data);"
    "    }"
    "  };"

    "  ws.onerror = function() {"
    "    setText('ws', 'WebSocket: error');"
    "  };"

    "  ws.onclose = function() {"
    "    setText('ws', 'WebSocket: closed');"
    "  };"
    "}"

    "function sendSignal(message) {"
    "  if (!ws || ws.readyState !== WebSocket.OPEN) {"
    "    throw new Error('WebSocket is not connected');"
    "  }"
    "  ws.send(JSON.stringify(message));"
    "}"

    "async function startWebRTC() {"
    "  try {"
    "    if (!ws || ws.readyState !== WebSocket.OPEN) {"
    "      connectWS();"
    "      await new Promise((resolve, reject) => {"
    "        const timeout = setTimeout(() => reject(new Error('WebSocket connection timeout')), 5000);"
    "        const check = () => {"
    "          if (ws && ws.readyState === WebSocket.OPEN) {"
    "            clearTimeout(timeout);"
    "            resolve();"
    "          } else {"
    "            setTimeout(check, 50);"
    "          }"
    "        };"
    "        check();"
    "      });"
    "    }"

    "    if (pc) {"
    "      pc.close();"
    "    }"

    "    pc = new RTCPeerConnection({iceServers: []});"
    "    setText('webrtc', 'WebRTC: RTCPeerConnection created\\n');"

    "    dataChannel = pc.createDataChannel('control');"

    "    dataChannel.onopen = function() {"
    "      setText('webrtc', 'WebRTC: data channel open');"
    "    };"

    "    dataChannel.onclose = function() {"
    "      setText('webrtc', 'WebRTC: data channel closed');"
    "    };"

    "    pc.onconnectionstatechange = function() {"
    "      setText('webrtc', 'WebRTC connection state: ' + pc.connectionState);"
    "    };"

    "    pc.onicegatheringstatechange = function() {"
    "      setText('webrtc', 'ICE gathering state: ' + pc.iceGatheringState);"
    "    };"

    "    pc.onicecandidate = function(event) {"
    "      if (event.candidate) {"
    "        sendSignal({"
    "          type: 'ice-candidate',"
    "          candidate: event.candidate.candidate,"
    "          sdpMid: event.candidate.sdpMid,"
    "          sdpMLineIndex: event.candidate.sdpMLineIndex"
    "        });"
    "      }"
    "    };"

    "    const offer = await pc.createOffer();"
    "    await pc.setLocalDescription(offer);"

    "    sendSignal({"
    "      type: 'offer',"
    "      sdp: pc.localDescription.sdp"
    "    });"

    "    setText('webrtc', 'SDP offer generated and sent to Fedora.\\n' + pc.localDescription.sdp);"
    "  } catch (error) {"
    "    console.error(error);"
    "    setText('webrtc', 'WebRTC error: ' + error.message);"
    "  }"
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

static void save_sdp_offer(
    const struct mg_str *sdp
)
{
    FILE *file = fopen(
        "docs/last_offer.sdp",
        "w"
    );

    if (!file) {
        perror("fopen docs/last_offer.sdp");
        return;
    }

    if (fwrite(
        sdp->buf,
        1,
        sdp->len,
        file
    ) != sdp->len) {
        perror("fwrite");
    }

    fclose(file);
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

        if (mg_match(
                message->data,
                mg_str("{\"type\":\"offer\",\"sdp\":*}"),
                NULL
            )) {

            save_sdp_offer(&message->data);
            printf("SDP offer received and saved to docs/last_offer.sdp\n");

            send_text(
                connection,
                "{\"type\":\"offer_received\",\"server\":\"fedora\"}"
            );

            return;
        }

        if (mg_match(
                message->data,
                mg_str("{\"type\":\"ice-candidate\",*}"),
                NULL
            )) {

            printf("ICE candidate received.\n");

            send_text(
                connection,
                "{\"type\":\"ice_received\",\"server\":\"fedora\"}"
            );

            return;
        }

        if (mg_strcmp(
                message->data,
                mg_str("{\"type\":\"hello\",\"client\":\"ubuntu\"}")
            ) == 0) {

            send_text(
                connection,
                "{\"type\":\"hello_ack\",\"server\":\"fedora\"}"
            );

            return;
        }

        send_text(
            connection,
            "{\"type\":\"message_received\",\"server\":\"fedora\"}"
        );

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
        "Starting HTTP/WebSocket/WebRTC signaling server at %s\n",
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
