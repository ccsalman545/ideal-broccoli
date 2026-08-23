#define _POSIX_C_SOURCE 200809L

#include "camera_v4l2.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>

#define CAMERA_BUFFER_COUNT 4

struct CameraBuffer {
    void *start;
    size_t length;
};

struct Camera {
    int fd;

    uint32_t width;
    uint32_t height;
    uint32_t fps;

    uint32_t stride;
    size_t frame_size;

    struct CameraBuffer *buffers;
    unsigned int buffer_count;

    int streaming;
};

static int xioctl(
    int fd,
    unsigned long request,
    void *arg
)
{
    int result;

    do {
        result = ioctl(fd, request, arg);
    } while (result == -1 && errno == EINTR);

    return result;
}

static uint64_t timestamp_us(void)
{
    struct timeval tv;

    if (gettimeofday(&tv, NULL) != 0)
        return 0;

    return (uint64_t)tv.tv_sec * 1000000ULL +
           (uint64_t)tv.tv_usec;
}

Camera *camera_open(
    const char *device,
    uint32_t width,
    uint32_t height,
    uint32_t fps
)
{
    if (!device) {
        fprintf(stderr, "Camera device is NULL\n");
        return NULL;
    }

    if (fps == 0) {
        fprintf(stderr, "FPS must be greater than zero\n");
        return NULL;
    }

    Camera *camera = calloc(1, sizeof(*camera));

    if (!camera) {
        perror("calloc");
        return NULL;
    }

    camera->fd = -1;

    camera->fd = open(
        device,
        O_RDWR | O_NONBLOCK
    );

    if (camera->fd == -1) {
        fprintf(
            stderr,
            "Cannot open camera %s: %s\n",
            device,
            strerror(errno)
        );

        free(camera);
        return NULL;
    }

    struct v4l2_capability capability;

    memset(&capability, 0, sizeof(capability));

    if (xioctl(
        camera->fd,
        VIDIOC_QUERYCAP,
        &capability
    ) == -1) {

        perror("VIDIOC_QUERYCAP");
        camera_close(camera);
        return NULL;
    }

    if (!(capability.capabilities &
          V4L2_CAP_VIDEO_CAPTURE)) {

        fprintf(
            stderr,
            "Device does not support video capture\n"
        );

        camera_close(camera);
        return NULL;
    }

    if (!(capability.capabilities &
          V4L2_CAP_STREAMING)) {

        fprintf(
            stderr,
            "Device does not support streaming\n"
        );

        camera_close(camera);
        return NULL;
    }

    printf(
        "Camera: %s\n",
        capability.card
    );

    /*
     * Configure YUYV capture.
     */
    struct v4l2_format format;

    memset(&format, 0, sizeof(format));

    format.type =
        V4L2_BUF_TYPE_VIDEO_CAPTURE;

    format.fmt.pix.width = width;
    format.fmt.pix.height = height;

    format.fmt.pix.pixelformat =
        V4L2_PIX_FMT_YUYV;

    format.fmt.pix.field =
        V4L2_FIELD_NONE;

    if (xioctl(
        camera->fd,
        VIDIOC_S_FMT,
        &format
    ) == -1) {

        perror("VIDIOC_S_FMT");
        camera_close(camera);
        return NULL;
    }

    /*
     * Use the values returned by the driver.
     * The driver is allowed to adjust the
     * requested format.
     */
    if (format.fmt.pix.pixelformat !=
        V4L2_PIX_FMT_YUYV) {

        fprintf(
            stderr,
            "Camera did not accept YUYV format\n"
        );

        camera_close(camera);
        return NULL;
    }

    camera->width =
        format.fmt.pix.width;

    camera->height =
        format.fmt.pix.height;

    camera->stride =
        format.fmt.pix.bytesperline;

    camera->frame_size =
        format.fmt.pix.sizeimage;

    /*
     * Request frame rate.
     */
    struct v4l2_streamparm streamparm;

    memset(&streamparm, 0, sizeof(streamparm));

    streamparm.type =
        V4L2_BUF_TYPE_VIDEO_CAPTURE;

    streamparm.parm.capture.timeperframe.numerator = 1;

    streamparm.parm.capture.timeperframe.denominator =
        fps;

    if (xioctl(
        camera->fd,
        VIDIOC_S_PARM,
        &streamparm
    ) == -1) {

        perror("VIDIOC_S_PARM");
        camera_close(camera);
        return NULL;
    }

    if (streamparm.parm.capture.timeperframe.numerator != 0) {

        camera->fps =
            streamparm.parm.capture.timeperframe.denominator /
            streamparm.parm.capture.timeperframe.numerator;

    } else {
        camera->fps = fps;
    }

    printf(
        "Resolution : %ux%u\n",
        camera->width,
        camera->height
    );

    printf(
        "Pixel format: YUYV\n"
    );

    printf(
        "Bytes/line : %u\n",
        camera->stride
    );

    printf(
        "Frame size : %zu bytes\n",
        camera->frame_size
    );

    printf(
        "Frame rate : %u FPS\n",
        camera->fps
    );

    /*
     * Request memory-mapped buffers.
     */
    struct v4l2_requestbuffers request;

    memset(&request, 0, sizeof(request));

    request.count =
        CAMERA_BUFFER_COUNT;

    request.type =
        V4L2_BUF_TYPE_VIDEO_CAPTURE;

    request.memory =
        V4L2_MEMORY_MMAP;

    if (xioctl(
        camera->fd,
        VIDIOC_REQBUFS,
        &request
    ) == -1) {

        perror("VIDIOC_REQBUFS");
        camera_close(camera);
        return NULL;
    }

    if (request.count < 2) {

        fprintf(
            stderr,
            "Insufficient V4L2 buffers: %u\n",
            request.count
        );

        camera_close(camera);
        return NULL;
    }

    camera->buffer_count =
        request.count;

    camera->buffers =
        calloc(
            camera->buffer_count,
            sizeof(*camera->buffers)
        );

    if (!camera->buffers) {
        perror("calloc");
        camera_close(camera);
        return NULL;
    }

    /*
     * Map each buffer into user space.
     */
    for (unsigned int i = 0;
         i < camera->buffer_count;
         i++) {

        struct v4l2_buffer buffer;

        memset(&buffer, 0, sizeof(buffer));

        buffer.type =
            V4L2_BUF_TYPE_VIDEO_CAPTURE;

        buffer.memory =
            V4L2_MEMORY_MMAP;

        buffer.index = i;

        if (xioctl(
            camera->fd,
            VIDIOC_QUERYBUF,
            &buffer
        ) == -1) {

            perror("VIDIOC_QUERYBUF");
            camera_close(camera);
            return NULL;
        }

        camera->buffers[i].length =
            buffer.length;

        camera->buffers[i].start =
            mmap(
                NULL,
                buffer.length,
                PROT_READ | PROT_WRITE,
                MAP_SHARED,
                camera->fd,
                buffer.m.offset
            );

        if (camera->buffers[i].start ==
            MAP_FAILED) {

            perror("mmap");

            camera->buffers[i].start = NULL;

            camera_close(camera);
            return NULL;
        }

        printf(
            "Buffer %u mapped: %zu bytes\n",
            i,
            camera->buffers[i].length
        );
    }

    return camera;
}

int camera_start(Camera *camera)
{
    if (!camera)
        return -1;

    if (camera->streaming)
        return 0;

    /*
     * Queue every buffer before starting.
     */
    for (unsigned int i = 0;
         i < camera->buffer_count;
         i++) {

        struct v4l2_buffer buffer;

        memset(&buffer, 0, sizeof(buffer));

        buffer.type =
            V4L2_BUF_TYPE_VIDEO_CAPTURE;

        buffer.memory =
            V4L2_MEMORY_MMAP;

        buffer.index = i;

        if (xioctl(
            camera->fd,
            VIDIOC_QBUF,
            &buffer
        ) == -1) {

            perror("VIDIOC_QBUF");
            return -1;
        }
    }

    enum v4l2_buf_type type =
        V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (xioctl(
        camera->fd,
        VIDIOC_STREAMON,
        &type
    ) == -1) {

        perror("VIDIOC_STREAMON");
        return -1;
    }

    camera->streaming = 1;

    return 0;
}

int camera_capture(
    Camera *camera,
    Frame *frame
)
{
    if (!camera || !frame)
        return -1;

    memset(frame, 0, sizeof(*frame));

    fd_set fds;
    struct timeval timeout;

    FD_ZERO(&fds);
    FD_SET(camera->fd, &fds);

    timeout.tv_sec = 2;
    timeout.tv_usec = 0;

    int result = select(
        camera->fd + 1,
        &fds,
        NULL,
        NULL,
        &timeout
    );

    if (result < 0) {

        if (errno == EINTR)
            return 0;

        perror("select");
        return -1;
    }

    if (result == 0) {

        fprintf(
            stderr,
            "Camera capture timeout\n"
        );

        return -1;
    }

    struct v4l2_buffer buffer;

    memset(&buffer, 0, sizeof(buffer));

    buffer.type =
        V4L2_BUF_TYPE_VIDEO_CAPTURE;

    buffer.memory =
        V4L2_MEMORY_MMAP;

    if (xioctl(
        camera->fd,
        VIDIOC_DQBUF,
        &buffer
    ) == -1) {

        if (errno == EAGAIN)
            return 0;

        perror("VIDIOC_DQBUF");
        return -1;
    }

    if (buffer.index >= camera->buffer_count) {

        fprintf(
            stderr,
            "Invalid V4L2 buffer index: %u\n",
            buffer.index
        );

        return -1;
    }

    frame->width =
        camera->width;

    frame->height =
        camera->height;

    frame->pixel_format =
        V4L2_PIX_FMT_YUYV;

    frame->stride =
        camera->stride;

    frame->size =
        buffer.bytesused;

    frame->sequence =
        buffer.sequence;

    frame->timestamp_us =
        timestamp_us();

    /*
     * Important:
     * Remember the exact MMAP buffer from
     * which this frame originated.
     */
    frame->buffer_index =
        buffer.index;

    frame->data =
        camera->buffers[buffer.index].start;

    return 1;
}

void camera_release_frame(
    Camera *camera,
    const Frame *frame
)
{
    if (!camera || !frame)
        return;

    if (frame->buffer_index >=
        camera->buffer_count) {

        fprintf(
            stderr,
            "Invalid V4L2 buffer index: %u\n",
            frame->buffer_index
        );

        return;
    }

    struct v4l2_buffer buffer;

    memset(&buffer, 0, sizeof(buffer));

    buffer.type =
        V4L2_BUF_TYPE_VIDEO_CAPTURE;

    buffer.memory =
        V4L2_MEMORY_MMAP;

    buffer.index =
        frame->buffer_index;

    if (xioctl(
        camera->fd,
        VIDIOC_QBUF,
        &buffer
    ) == -1) {

        perror("VIDIOC_QBUF");
    }
}

void camera_close(Camera *camera)
{
    if (!camera)
        return;

    if (camera->streaming) {

        enum v4l2_buf_type type =
            V4L2_BUF_TYPE_VIDEO_CAPTURE;

        if (xioctl(
            camera->fd,
            VIDIOC_STREAMOFF,
            &type
        ) == -1) {

            perror("VIDIOC_STREAMOFF");
        }

        camera->streaming = 0;
    }

    if (camera->buffers) {

        for (unsigned int i = 0;
             i < camera->buffer_count;
             i++) {

            if (camera->buffers[i].start &&
                camera->buffers[i].start !=
                    MAP_FAILED) {

                munmap(
                    camera->buffers[i].start,
                    camera->buffers[i].length
                );
            }
        }

        free(camera->buffers);
    }

    if (camera->fd >= 0)
        close(camera->fd);

    free(camera);
}
