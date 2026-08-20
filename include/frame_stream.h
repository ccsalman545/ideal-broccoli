#ifndef FRAME_STREAM_H
#define FRAME_STREAM_H

#include <stddef.h>

#include "frame.h"

struct mg_connection;

typedef struct FrameStream FrameStream;

/*
 * Create a frame streaming context.
 */
FrameStream *frame_stream_create(void);

/*
 * Set the currently connected WebSocket client.
 *
 * The connection is owned by Mongoose.
 * FrameStream only stores the pointer.
 */
void frame_stream_set_client(
    FrameStream *stream,
    struct mg_connection *connection
);

/*
 * Remove the currently connected WebSocket client.
 */
void frame_stream_clear_client(
    FrameStream *stream,
    struct mg_connection *connection
);

/*
 * Send one camera frame through WebSocket.
 *
 * Returns:
 *   0  success
 *  -1  no client or transmission failure
 */
int frame_stream_send(
    FrameStream *stream,
    const Frame *frame
);

/*
 * Destroy the streaming context.
 */
void frame_stream_destroy(
    FrameStream *stream
);
#endif
