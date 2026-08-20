#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "frame_queue.h"

struct FrameQueue {
    Frame *frames;
    size_t capacity;
    size_t head;
    size_t tail;
    size_t count;
};


FrameQueue *frame_queue_create(size_t capacity)
{
    if (capacity == 0) {
        fprintf(stderr,
                "Frame queue capacity must be greater than zero\n");
        return NULL;
    }

    FrameQueue *queue = calloc(1, sizeof(*queue));

    if (queue == NULL) {
        perror("calloc");
        return NULL;
    }

    queue->frames = calloc(
        capacity,
        sizeof(*queue->frames)
    );

    if (queue->frames == NULL) {
        perror("calloc");
        free(queue);
        return NULL;
    }

    queue->capacity = capacity;

    return queue;
}


int frame_queue_push(
    FrameQueue *queue,
    const Frame *frame)
{
    if (queue == NULL || frame == NULL) {
        return -1;
    }

    if (frame->data == NULL || frame->size == 0) {
        return -1;
    }

    /*
     * Queue is full.
     */
    if (queue->count >= queue->capacity) {
        return -1;
    }

    /*
     * Copy Frame metadata.
     */
    Frame *destination = &queue->frames[queue->tail];

    *destination = *frame;

    /*
     * Allocate independent storage for the image.
     */
    destination->data = malloc(frame->size);

    if (destination->data == NULL) {
        fprintf(stderr,
                "Failed to allocate frame queue buffer\n");

        memset(destination, 0, sizeof(*destination));

        return -1;
    }

    /*
     * Copy the camera image.
     */
    memcpy(
        destination->data,
        frame->data,
        frame->size
    );

    /*
     * Advance circular queue tail.
     */
    queue->tail++;

    if (queue->tail >= queue->capacity) {
        queue->tail = 0;
    }

    queue->count++;

    return 0;
}


int frame_queue_pop(
    FrameQueue *queue,
    Frame *frame)
{
    if (queue == NULL || frame == NULL) {
        return -1;
    }

    if (queue->count == 0) {
        return 0;
    }

    /*
     * Transfer ownership of the stored Frame
     * to the caller.
     */
    *frame = queue->frames[queue->head];

    /*
     * Clear the queue slot so it no longer owns
     * the returned buffer.
     */
    memset(
        &queue->frames[queue->head],
        0,
        sizeof(queue->frames[queue->head])
    );

    /*
     * Advance circular queue head.
     */
    queue->head++;

    if (queue->head >= queue->capacity) {
        queue->head = 0;
    }

    queue->count--;

    return 1;
}


size_t frame_queue_size(
    const FrameQueue *queue)
{
    if (queue == NULL) {
        return 0;
    }

    return queue->count;
}


void frame_queue_clear(
    FrameQueue *queue)
{
    if (queue == NULL) {
        return;
    }

    /*
     * Free every queued image buffer.
     */
    for (size_t i = 0; i < queue->capacity; i++) {
        free(queue->frames[i].data);
        queue->frames[i].data = NULL;
    }

    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
}


void frame_queue_destroy(
    FrameQueue *queue)
{
    if (queue == NULL) {
        return;
    }

    frame_queue_clear(queue);

    free(queue->frames);
    free(queue);
}

