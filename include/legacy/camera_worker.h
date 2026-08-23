#ifndef CAMERA_WORKER_H
#define CAMERA_WORKER_H

#include "camera_v4l2.h"
#include "frame_queue.h"

typedef struct CameraWorker CameraWorker;

/*
 * Create a camera worker.
 *
 * The worker captures frames from the camera and
 * places them into the frame queue.
 */
CameraWorker *camera_worker_create(
    Camera *camera,
    FrameQueue *queue
);

/*
 * Start the camera worker thread.
 */
int camera_worker_start(
    CameraWorker *worker
);

/*
 * Request the camera worker to stop.
 */
void camera_worker_stop(
    CameraWorker *worker
);

/*
 * Wait for the camera worker thread to exit.
 */
void camera_worker_join(
    CameraWorker *worker
);

/*
 * Destroy the camera worker.
 */
void camera_worker_destroy(
    CameraWorker *worker
);

#endif
