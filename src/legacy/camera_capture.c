#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <linux/videodev2.h>

#define DEVICE      "/dev/video0"
#define WIDTH       640
#define HEIGHT      480
#define BUFFER_COUNT 4
#define FRAME_COUNT 100

struct buffer {
    void *start;
    size_t length;
};

static int xioctl(int fd, unsigned long request, void *arg)
{
    int r;

    do {
        r = ioctl(fd, request, arg);
    } while (r == -1 && errno == EINTR);

    return r;
}

int main(void)
{
    int fd;
    struct v4l2_capability cap;
    struct v4l2_format fmt;
    struct v4l2_requestbuffers req;
    struct buffer buffers[BUFFER_COUNT];

    unsigned int n_buffers;
    unsigned int frame_count = 0;

    printf("Opening %s...\n", DEVICE);

    fd = open(DEVICE, O_RDWR | O_NONBLOCK, 0);

    if (fd == -1) {
        perror("Cannot open camera");
        return EXIT_FAILURE;
    }

    /* Check device capabilities */
    memset(&cap, 0, sizeof(cap));

    if (xioctl(fd, VIDIOC_QUERYCAP, &cap) == -1) {
        perror("VIDIOC_QUERYCAP");
        close(fd);
        return EXIT_FAILURE;
    }

    printf("Camera: %s\n", cap.card);

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        fprintf(stderr, "Device does not support video capture\n");
        close(fd);
        return EXIT_FAILURE;
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        fprintf(stderr, "Device does not support streaming\n");
        close(fd);
        return EXIT_FAILURE;
    }

    /* Configure YUYV 640x480 */
    memset(&fmt, 0, sizeof(fmt));

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = WIDTH;
    fmt.fmt.pix.height = HEIGHT;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    if (xioctl(fd, VIDIOC_S_FMT, &fmt) == -1) {
        perror("VIDIOC_S_FMT");
        close(fd);
        return EXIT_FAILURE;
    }

    printf("Format configured:\n");
    printf("  Resolution : %ux%u\n",
           fmt.fmt.pix.width,
           fmt.fmt.pix.height);

    printf("  Pixel format: YUYV\n");
    printf("  Image size : %u bytes\n",
           fmt.fmt.pix.sizeimage);

    /* Request memory-mapped buffers */
    memset(&req, 0, sizeof(req));

    req.count = BUFFER_COUNT;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (xioctl(fd, VIDIOC_REQBUFS, &req) == -1) {
        perror("VIDIOC_REQBUFS");
        close(fd);
        return EXIT_FAILURE;
    }

    if (req.count < 2) {
        fprintf(stderr, "Insufficient camera buffers\n");
        close(fd);
        return EXIT_FAILURE;
    }

    n_buffers = req.count;

    printf("Allocated %u capture buffers\n", n_buffers);

    /* Map camera buffers into our process */
    for (unsigned int i = 0; i < n_buffers; i++) {

        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(buf));

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (xioctl(fd, VIDIOC_QUERYBUF, &buf) == -1) {
            perror("VIDIOC_QUERYBUF");
            close(fd);
            return EXIT_FAILURE;
        }

        buffers[i].length = buf.length;

        buffers[i].start = mmap(
            NULL,
            buf.length,
            PROT_READ | PROT_WRITE,
            MAP_SHARED,
            fd,
            buf.m.offset
        );

        if (buffers[i].start == MAP_FAILED) {
            perror("mmap");
            close(fd);
            return EXIT_FAILURE;
        }

        printf("Buffer %u mapped: %zu bytes\n",
               i,
               buffers[i].length);
    }

    /* Queue all buffers */
    for (unsigned int i = 0; i < n_buffers; i++) {

        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(buf));

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (xioctl(fd, VIDIOC_QBUF, &buf) == -1) {
            perror("VIDIOC_QBUF");
            return EXIT_FAILURE;
        }
    }

    /* Start streaming */
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (xioctl(fd, VIDIOC_STREAMON, &type) == -1) {
        perror("VIDIOC_STREAMON");
        return EXIT_FAILURE;
    }

    printf("\nCamera streaming started.\n");
    printf("Capturing %d frames...\n\n", FRAME_COUNT);

    while (frame_count < FRAME_COUNT) {

        fd_set fds;
        struct timeval tv;

        FD_ZERO(&fds);
        FD_SET(fd, &fds);

        tv.tv_sec = 2;
        tv.tv_usec = 0;

        int r = select(fd + 1, &fds, NULL, NULL, &tv);

        if (r == -1) {

            if (errno == EINTR)
                continue;

            perror("select");
            break;
        }

        if (r == 0) {
            fprintf(stderr, "Camera timeout\n");
            break;
        }

        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(buf));

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        if (xioctl(fd, VIDIOC_DQBUF, &buf) == -1) {

            if (errno == EAGAIN)
                continue;

            perror("VIDIOC_DQBUF");
            break;
        }

        unsigned char *frame =
            (unsigned char *)buffers[buf.index].start;

        size_t frame_size = buf.bytesused;

        /*
         * YUYV:
         *
         * byte 0 = Y0
         * byte 1 = U0
         * byte 2 = Y1
         * byte 3 = V0
         *
         * Therefore every second byte is luminance Y.
         */

        uint64_t luminance_sum = 0;

        for (size_t i = 0; i + 1 < frame_size; i += 2) {
            luminance_sum += frame[i];
        }

        size_t pixel_count = frame_size / 2;

        double average_luminance =
            (double)luminance_sum / pixel_count;

        printf(
            "Frame %3u | bytes=%6zu | average Y=%.2f\n",
            frame_count + 1,
            frame_size,
            average_luminance
        );

        /* Save the first frame */
        if (frame_count == 0) {

            FILE *out = fopen("frame_000.yuyv", "wb");

            if (out) {
                fwrite(frame, 1, frame_size, out);
                fclose(out);

                printf("First frame saved to frame_000.yuyv\n");
            }
        }

        frame_count++;

        /* Return buffer to driver */
        if (xioctl(fd, VIDIOC_QBUF, &buf) == -1) {
            perror("VIDIOC_QBUF");
            break;
        }
    }

    /* Stop streaming */
    if (xioctl(fd, VIDIOC_STREAMOFF, &type) == -1) {
        perror("VIDIOC_STREAMOFF");
    }

    /* Unmap buffers */
    for (unsigned int i = 0; i < n_buffers; i++) {
        munmap(buffers[i].start, buffers[i].length);
    }

    close(fd);

    printf("\nCapture complete.\n");
    printf("Frames captured: %u\n", frame_count);

    return EXIT_SUCCESS;
}
