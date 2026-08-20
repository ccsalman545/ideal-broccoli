#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "mongoose.h"
#include "frame_stream.h"

/*
 * Simple frame-streaming context.
 *
 * The WebSocket connection itself is owned by Mongoose.
 * We only keep a non-owning pointer to it.
 */
struct FrameStream {
    struct mg_connection *client;
};

/*
 * Wire-format header.
 *
 * This metadata is sent before every frame.
 *
 * All fields are fixed-width integers so the network
 * protocol does not depend on the size of C types.
 */
typedef struct {
    uint32_t magic;
    uint32_t width;
    uint32_t height;
    uint32_t pixel_format;
    uint32_t stride;
    uint32_t frame_size;
    uint32_t sequence;
} FramePacketHeader;


/*
 * Create frame stream.
 */
FrameStream *frame_stream_create(void)
{
    FrameStream *stream = calloc(1, sizeof(*stream));

    if (stream == NULL) {
        fprintf(stderr, "Failed to allocate FrameStream\n");
        return NULL;
    }

    stream->client = NULL;

    return stream;
}


/*
 * Attach a WebSocket client.
 */
void frame_stream_set_client(
    FrameStream *stream,
    struct mg_connection *connection)
{
    if (stream == NULL) {
        return;
    }

    stream->client = connection;

    printf("Frame stream client attached\n");
}


/*
 * Detach a WebSocket client.
 */
void frame_stream_clear_client(
    FrameStream *stream,
    struct mg_connection *connection)
{
    if (stream == NULL) {
        return;
    }

    /*
     * Only clear the client if it is the same connection.
     */
    if (stream->client == connection) {
        stream->client = NULL;

        printf("Frame stream client detached\n");
    }
}


/*
 * Send one camera frame.
 */
int frame_stream_send(
    FrameStream *stream,
    const Frame *frame)
{
    if (stream == NULL || frame == NULL) {
        return -1;
    }

    if (stream->client == NULL) {
        return -1;
    }

    if (frame->data == NULL || frame->size == 0) {
        return -1;
    }

    /*
     * Construct frame metadata.
     */
    FramePacketHeader header;

    memset(&header, 0, sizeof(header));

    /*
     * "FRAM" in hexadecimal.
     *
     * Used to identify our frame packet.
     */
    header.magic = 0x4652414D;

    header.width = frame->width;
    header.height = frame->height;
    header.pixel_format = frame->pixel_format;
    header.stride = frame->stride;
    header.frame_size = (uint32_t) frame->size;
    header.sequence = frame->sequence;

    /*
     * Send metadata first.
     */
    mg_ws_send(
        stream->client,
        &header,
        sizeof(header),
        WEBSOCKET_OP_BINARY
    );

    /*
     * Send the actual camera buffer.
     */
    mg_ws_send(
        stream->client,
        frame->data,
        frame->size,
        WEBSOCKET_OP_BINARY
    );

    return 0;
}


/*
 * Destroy frame stream.
 */
void frame_stream_destroy(
    FrameStream *stream)
{
    if (stream == NULL) {
        return;
    }

    stream->client = NULL;

    free(stream);
}
