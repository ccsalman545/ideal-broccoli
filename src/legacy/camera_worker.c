#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>

#include "camera_worker.h"

struct CameraWorker {
    Camera *camera;
    FrameQueue *queue;

    pthread_t thread;
    bool running;
    bool started;
};


static void *camera_worker_thread(void *arg)
{
    CameraWorker *worker = arg;

    printf("Camera capture worker started\n");

    while (worker->running) {

        Frame frame;

        int result = camera_capture(
            worker->camera,
            &frame
        );

        /*
         * camera_capture():
         *
         *   1  = frame captured successfully
         *   0  = no frame / interrupted
         *  -1  = error
         */
        if (result <= 0) {

            if (result < 0) {
                fprintf(
                    stderr,
                    "camera_capture() failed\n"
                );
            }

            /*
             * Avoid a tight error loop.
             */
            usleep(10000);

            continue;
        }

        /*
         * Successful frame.
         *
         * frame.data points directly to a V4L2
         * MMAP buffer, so frame_queue_push()
         * must copy the image before we release
         * the V4L2 buffer.
         */
        if (frame_queue_push(
                worker->queue,
                &frame
            ) != 0) {

            fprintf(
                stderr,
                "Frame queue full; dropping frame %llu\n",
                (unsigned long long) frame.sequence
            );
        }

        /*
         * The queue now owns its own copy.
         *
         * Return the original V4L2 MMAP buffer
         * back to the camera driver.
         */
        camera_release_frame(
            worker->camera,
            &frame
        );
    }

    printf("Camera capture worker stopped\n");

    return NULL;
}


CameraWorker *camera_worker_create(
    Camera *camera,
    FrameQueue *queue)
{
    if (camera == NULL || queue == NULL) {
        return NULL;
    }

    CameraWorker *worker = calloc(
        1,
        sizeof(*worker)
    );

    if (worker == NULL) {
        perror("calloc");
        return NULL;
    }

    worker->camera = camera;
    worker->queue = queue;
    worker->running = false;
    worker->started = false;

    return worker;
}


int camera_worker_start(
    CameraWorker *worker)
{
    if (worker == NULL) {
        return -1;
    }

    if (worker->started) {
        return 0;
    }

    worker->running = true;

    int result = pthread_create(
        &worker->thread,
        NULL,
        camera_worker_thread,
        worker
    );

    if (result != 0) {

        fprintf(
            stderr,
            "Failed to create camera worker thread\n"
        );

        worker->running = false;

        return -1;
    }

    worker->started = true;

    return 0;
}


void camera_worker_stop(
    CameraWorker *worker)
{
    if (worker == NULL) {
        return;
    }

    worker->running = false;
}


void camera_worker_join(
    CameraWorker *worker)
{
    if (worker == NULL || !worker->started) {
        return;
    }

    pthread_join(
        worker->thread,
        NULL
    );

    worker->started = false;
}


void camera_worker_destroy(
    CameraWorker *worker)
{
    if (worker == NULL) {
        return;
    }

    camera_worker_stop(worker);
    camera_worker_join(worker);

    free(worker);
}

