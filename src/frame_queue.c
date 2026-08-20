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
        fprintf(stderr, "Frame queue capacity must be greater than zero\n");
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

    if (queue->count >= queue->capacity) {
        return -1;
    }

    queue->frames[queue->tail] = *frame;

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

    *frame = queue->frames[queue->head];

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

    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;

    memset(
        queue->frames,
        0,
        queue->capacity * sizeof(*queue->frames)
    );
}


void frame_queue_destroy(
    FrameQueue *queue)
{
    if (queue == NULL) {
        return;
    }

    free(queue->frames);
    free(queue);
}
