#ifndef CAMERA_V4L2_H
#define CAMERA_V4L2_H

#include <stdint.h>

#include "frame.h"

typedef struct Camera Camera;

Camera *camera_open(
    const char *device,
    uint32_t width,
    uint32_t height,
    uint32_t fps
);

int camera_start(Camera *camera);

int camera_capture(
    Camera *camera,
    Frame *frame
);

void camera_release_frame(
    Camera *camera,
    const Frame *frame
);

void camera_close(Camera *camera);

#endif
