#ifndef FRAME_QUEUE_H
#define FRAME_QUEUE_H

#include <stddef.h>

#include "frame.h"

typedef struct FrameQueue FrameQueue;

/*
 * Create a frame queue.
 *
 * capacity:
 *   Maximum number of queued frames.
 */
FrameQueue *frame_queue_create(size_t capacity);

/*
 * Push a frame into the queue.
 *
 * The queue does not take ownership of frame->data.
 * The caller must keep the underlying buffer valid
 * until the frame has been consumed.
 *
 * Returns:
 *   0  success
 *  -1 queue full or invalid argument
 */
int frame_queue_push(
    FrameQueue *queue,
    const Frame *frame
);

/*
 * Pop the oldest frame from the queue.
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
 * Return the number of queued frames.
 */
size_t frame_queue_size(
    const FrameQueue *queue
);

/*
 * Clear all queued entries.
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
