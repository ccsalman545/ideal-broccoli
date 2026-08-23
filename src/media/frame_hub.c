/*
 * frame_hub.c
 *
 * See frame_hub.h for the design rationale.
 */
#include "frame_hub.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

struct FrameHubConsumer {
    FrameHub *hub;
    Frame *slot;
    pthread_mutex_t lock;
    int active;
    struct FrameHubConsumer *next;
    struct FrameHubConsumer *prev;
};

struct FrameHub {
    FramePool *pool;
    pthread_mutex_t lock;           /* guards the consumer list */
    FrameHubConsumer *consumers;
    uint64_t published;
};

FrameHub *frame_hub_create(size_t buffer_capacity, size_t pool_count)
{
    FrameHub *hub = calloc(1, sizeof(*hub));
    if (hub == NULL) {
        return NULL;
    }

    hub->pool = frame_pool_create(buffer_capacity, pool_count);
    if (hub->pool == NULL) {
        free(hub);
        return NULL;
    }

    pthread_mutex_init(&hub->lock, NULL);

    return hub;
}

FrameHubConsumer *frame_hub_subscribe(FrameHub *hub)
{
    if (hub == NULL) {
        return NULL;
    }

    FrameHubConsumer *consumer = calloc(1, sizeof(*consumer));
    if (consumer == NULL) {
        return NULL;
    }

    consumer->hub = hub;
    consumer->active = 1;
    pthread_mutex_init(&consumer->lock, NULL);

    pthread_mutex_lock(&hub->lock);

    consumer->next = hub->consumers;
    if (hub->consumers != NULL) {
        hub->consumers->prev = consumer;
    }
    hub->consumers = consumer;

    pthread_mutex_unlock(&hub->lock);

    return consumer;
}

void frame_hub_unsubscribe(FrameHub *hub, FrameHubConsumer *consumer)
{
    if (hub == NULL || consumer == NULL) {
        return;
    }

    pthread_mutex_lock(&hub->lock);
    pthread_mutex_lock(&consumer->lock);

    if (consumer->active) {
        consumer->active = 0;

        if (consumer->prev != NULL) {
            consumer->prev->next = consumer->next;
        } else {
            hub->consumers = consumer->next;
        }
        if (consumer->next != NULL) {
            consumer->next->prev = consumer->prev;
        }
    }

    pthread_mutex_unlock(&consumer->lock);
    pthread_mutex_unlock(&hub->lock);

    if (consumer->slot != NULL) {
        frame_unref(hub->pool, consumer->slot);
        consumer->slot = NULL;
    }

    pthread_mutex_destroy(&consumer->lock);
    free(consumer);
}

int frame_hub_publish(FrameHub *hub,
                      const uint8_t *data,
                      size_t size,
                      uint32_t width,
                      uint32_t height,
                      uint32_t format,
                      uint32_t stride,
                      uint64_t sequence,
                      uint64_t timestamp_us)
{
    if (hub == NULL || data == NULL || size == 0) {
        return -1;
    }

    Frame *frame = frame_pool_acquire(hub->pool);
    if (frame == NULL) {
        return -1;
    }

    memcpy(frame->data, data, size);

    frame->width = width;
    frame->height = height;
    frame->format = format;
    frame->stride = stride;
    frame->size = size;
    frame->sequence = sequence;
    frame->timestamp_us = timestamp_us;

    pthread_mutex_lock(&hub->lock);

    hub->published++;

    /*
     * Deliver to every consumer mailbox. Keep-newest policy.
     */
    for (FrameHubConsumer *c = hub->consumers; c != NULL; c = c->next) {
        pthread_mutex_lock(&c->lock);

        if (!c->active) {
            pthread_mutex_unlock(&c->lock);
            continue;
        }

        frame_ref(frame);

        if (c->slot != NULL) {
            /*
             * Previous frame was never taken. Drop it so the
             * consumer always sees the newest frame.
             */
            frame_unref(hub->pool, c->slot);
        }

        c->slot = frame;

        pthread_mutex_unlock(&c->lock);
    }

    pthread_mutex_unlock(&hub->lock);

    /*
     * Drop the publisher reference. If nobody subscribed the
     * buffer returns to the pool immediately.
     */
    frame_unref(hub->pool, frame);

    return 0;
}

Frame *frame_hub_take(FrameHubConsumer *consumer)
{
    if (consumer == NULL) {
        return NULL;
    }

    pthread_mutex_lock(&consumer->lock);

    Frame *frame = consumer->slot;
    consumer->slot = NULL;

    pthread_mutex_unlock(&consumer->lock);

    return frame;
}

FramePool *frame_hub_pool(FrameHub *hub)
{
    return hub != NULL ? hub->pool : NULL;
}

uint64_t frame_hub_published_count(const FrameHub *hub)
{
    return hub != NULL ? hub->published : 0;
}

void frame_hub_destroy(FrameHub *hub)
{
    if (hub == NULL) {
        return;
    }

    while (hub->consumers != NULL) {
        frame_hub_unsubscribe(hub, hub->consumers);
    }

    frame_pool_destroy(hub->pool);
    pthread_mutex_destroy(&hub->lock);
    free(hub);
}
