/*
 * source_worker.h
 *
 * Capture thread: drives a VideoSource and publishes every
 * frame into the frame hub.
 */
#ifndef MEDIA_SOURCE_WORKER_H
#define MEDIA_SOURCE_WORKER_H

#include <stdint.h>

#include "frame_hub.h"
#include "video_source.h"

typedef struct SourceWorker SourceWorker;

SourceWorker *source_worker_create(VideoSource *source, FrameHub *hub);

int source_worker_start(SourceWorker *worker);

void source_worker_stop(SourceWorker *worker);

/*
 * Block until the thread exited. Shutdown may take up to one
 * camera poll timeout (200 ms) plus one frame period.
 */
void source_worker_join(SourceWorker *worker);

uint64_t source_worker_captured(const SourceWorker *worker);

void source_worker_destroy(SourceWorker *worker);

#endif
