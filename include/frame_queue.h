#ifndef FRAME_QUEUE_H
#define FRAME_QUEUE_H

#include <stddef.h>

#include "frame.h"

typedef struct FrameQueue FrameQueue;

/*
 * Create a frame queue.
 *
 * Each queued frame owns its copied image buffer.
 */
FrameQueue *frame_queue_create(size_t capacity);

/*
 * Copy a frame into the queue.
 *
 * The queue makes its own copy of frame->data.
 *
 * Returns:
 *   0  success
 *  -1 failure or queue full
 */
int frame_queue_push(
    FrameQueue *queue,
    const Frame *frame
);

/*
 * Remove the oldest frame.
 *
 * The returned Frame owns its data buffer.
 * The caller must eventually free(frame->data).
 *
 * Returns:
 *   1  frame returned
 *   0  queue empty
 *  -1 invalid argument
 */
int frame_queue_pop(
    FrameQueue *queue,
    Frame *frame
);

/*
 * Number of queued frames.
 */
size_t frame_queue_size(
    const FrameQueue *queue
);

/*
 * Remove and free all queued frame data.
 */
void frame_queue_clear(
    FrameQueue *queue
);

/*
 * Destroy the queue.
 */
void frame_queue_destroy(
    FrameQueue *queue
);

#endif
