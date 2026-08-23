/*
 * frame_pool.h
 *
 * Fixed size pool of pre-allocated frame buffers with
 * reference counting.
 *
 * Purpose:
 *   Eliminate per-frame malloc()/free() in the hot capture
 *   path. Buffers are allocated once at startup and recycled
 *   forever. This removes allocator jitter and heap
 *   fragmentation from the streaming pipeline.
 */
#ifndef MEDIA_FRAME_POOL_H
#define MEDIA_FRAME_POOL_H

#include <stddef.h>
#include <stdint.h>

/*
 * A single video frame.
 *
 * 'data' points into a pool buffer and must not be freed
 * directly. Use frame_pool_release() / frame_unref().
 */
typedef struct Frame {
    uint32_t width;
    uint32_t height;
    uint32_t format;        /* V4L2 fourcc, for example V4L2_PIX_FMT_YUYV */
    uint32_t stride;
    size_t size;            /* valid bytes in data */

    uint64_t sequence;      /* frame counter */
    uint64_t timestamp_us;  /* CLOCK_MONOTONIC based timestamp */

    uint8_t *data;

    /* Pool bookkeeping, do not touch from consumer code. */
    int pool_index;
    int refcount;
} Frame;

typedef struct FramePool FramePool;

/*
 * Create a pool with 'count' buffers, each 'capacity' bytes.
 */
FramePool *frame_pool_create(size_t capacity, size_t count);

/*
 * Take a buffer out of the pool.
 * Returned frame has refcount 1 and is zeroed except data.
 * Returns NULL when the pool is momentarily empty.
 */
Frame *frame_pool_acquire(FramePool *pool);

/*
 * Increment the reference count.
 */
void frame_ref(Frame *frame);

/*
 * Decrement the reference count and return the buffer to
 * the pool when it drops to zero.
 */
void frame_unref(FramePool *pool, Frame *frame);

size_t frame_pool_available(const FramePool *pool);
size_t frame_pool_capacity(const FramePool *pool);

void frame_pool_destroy(FramePool *pool);

#endif
