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
#include <sys/socket.h>
#include <arpa/inet.h>
#include <linux/videodev2.h>

#define DEVICE "/dev/video0"

#define WIDTH 640
#define HEIGHT 480
#define FRAME_SIZE (WIDTH * HEIGHT * 2)

#define BUFFER_COUNT 4
#define FRAME_COUNT 100

#define SERVER_IP "192.168.1.20"
#define SERVER_PORT 5000

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

static int send_all(int fd, const void *buffer, size_t length)
{
    size_t sent = 0;
    const char *ptr = buffer;

    while (sent < length) {

        ssize_t n = send(
            fd,
            ptr + sent,
            length - sent,
            0
        );

        if (n < 0) {

            if (errno == EINTR)
                continue;

            perror("send");
            return -1;
        }

        if (n == 0)
            return -1;

        sent += n;
    }

    return 0;
}

int main(void)
{
    int camera_fd;
    int socket_fd;

    struct v4l2_capability cap;
    struct v4l2_format fmt;
    struct v4l2_requestbuffers req;

    struct buffer buffers[BUFFER_COUNT];

    unsigned int n_buffers;
    unsigned int frame_count = 0;

    printf("Opening camera %s...\n", DEVICE);

    camera_fd = open(
        DEVICE,
        O_RDWR | O_NONBLOCK,
        0
    );

    if (camera_fd == -1) {
        perror("Cannot open camera");
        return EXIT_FAILURE;
    }

    /* Query camera */
    memset(&cap, 0, sizeof(cap));

    if (xioctl(
        camera_fd,
        VIDIOC_QUERYCAP,
        &cap
    ) == -1) {

        perror("VIDIOC_QUERYCAP");
        close(camera_fd);
        return EXIT_FAILURE;
    }

    printf("Camera: %s\n", cap.card);

    /* Configure YUYV 640x480 */
    memset(&fmt, 0, sizeof(fmt));

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = WIDTH;
    fmt.fmt.pix.height = HEIGHT;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    if (xioctl(
        camera_fd,
        VIDIOC_S_FMT,
        &fmt
    ) == -1) {

        perror("VIDIOC_S_FMT");
        close(camera_fd);
        return EXIT_FAILURE;
    }

    printf(
        "Camera format: %ux%u YUYV\n",
        fmt.fmt.pix.width,
        fmt.fmt.pix.height
    );

    printf(
        "Frame size: %u bytes\n",
        fmt.fmt.pix.sizeimage
    );

    /* Request MMAP buffers */
    memset(&req, 0, sizeof(req));

    req.count = BUFFER_COUNT;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (xioctl(
        camera_fd,
        VIDIOC_REQBUFS,
        &req
    ) == -1) {

        perror("VIDIOC_REQBUFS");
        close(camera_fd);
        return EXIT_FAILURE;
    }

    n_buffers = req.count;

    printf(
        "Allocated %u camera buffers\n",
        n_buffers
    );

    /* Map buffers */
    for (unsigned int i = 0;
         i < n_buffers;
         i++) {

        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(buf));

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (xioctl(
            camera_fd,
            VIDIOC_QUERYBUF,
            &buf
        ) == -1) {

            perror("VIDIOC_QUERYBUF");
            close(camera_fd);
            return EXIT_FAILURE;
        }

        buffers[i].length = buf.length;

        buffers[i].start = mmap(
            NULL,
            buf.length,
            PROT_READ | PROT_WRITE,
            MAP_SHARED,
            camera_fd,
            buf.m.offset
        );

        if (buffers[i].start == MAP_FAILED) {

            perror("mmap");
            close(camera_fd);
            return EXIT_FAILURE;
        }

        printf(
            "Buffer %u mapped: %zu bytes\n",
            i,
            buffers[i].length
        );
    }

    /* Queue buffers */
    for (unsigned int i = 0;
         i < n_buffers;
         i++) {

        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(buf));

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (xioctl(
            camera_fd,
            VIDIOC_QBUF,
            &buf
        ) == -1) {

            perror("VIDIOC_QBUF");
            return EXIT_FAILURE;
        }
    }

    /* Start camera */
    enum v4l2_buf_type type =
        V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (xioctl(
        camera_fd,
        VIDIOC_STREAMON,
        &type
    ) == -1) {

        perror("VIDIOC_STREAMON");
        return EXIT_FAILURE;
    }

    printf("Camera streaming started.\n");

    /*
     * Create TCP socket
     */
    socket_fd = socket(
        AF_INET,
        SOCK_STREAM,
        0
    );

    if (socket_fd < 0) {

        perror("socket");
        return EXIT_FAILURE;
    }

    struct sockaddr_in server;

    memset(&server, 0, sizeof(server));

    server.sin_family = AF_INET;
    server.sin_port = htons(SERVER_PORT);

    if (inet_pton(
        AF_INET,
        SERVER_IP,
        &server.sin_addr
    ) <= 0) {

        fprintf(
            stderr,
            "Invalid server IP\n"
        );

        return EXIT_FAILURE;
    }

    printf(
        "Connecting to %s:%d...\n",
        SERVER_IP,
        SERVER_PORT
    );

    if (connect(
        socket_fd,
        (struct sockaddr *)&server,
        sizeof(server)
    ) < 0) {

        perror("connect");
        close(socket_fd);
        return EXIT_FAILURE;
    }

    printf("TCP connection established.\n");
    printf("Sending camera frames...\n\n");

    while (frame_count < FRAME_COUNT) {

        fd_set fds;
        struct timeval tv;

        FD_ZERO(&fds);
        FD_SET(camera_fd, &fds);

        tv.tv_sec = 2;
        tv.tv_usec = 0;

        int r = select(
            camera_fd + 1,
            &fds,
            NULL,
            NULL,
            &tv
        );

        if (r == -1) {

            if (errno == EINTR)
                continue;

            perror("select");
            break;
        }

        if (r == 0) {

            fprintf(
                stderr,
                "Camera timeout\n"
            );

            break;
        }

        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(buf));

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        if (xioctl(
            camera_fd,
            VIDIOC_DQBUF,
            &buf
        ) == -1) {

            if (errno == EAGAIN)
                continue;

            perror("VIDIOC_DQBUF");
            break;
        }

        unsigned char *frame =
            buffers[buf.index].start;

        size_t frame_size = buf.bytesused;

        /*
         * Send exactly one complete frame.
         */
        if (frame_size != FRAME_SIZE) {

            fprintf(
                stderr,
                "Unexpected frame size: %zu\n",
                frame_size
            );

            xioctl(
                camera_fd,
                VIDIOC_QBUF,
                &buf
            );

            continue;
        }

        if (send_all(
            socket_fd,
            frame,
            FRAME_SIZE
        ) < 0) {

            fprintf(
                stderr,
                "Failed to send frame\n"
            );

            xioctl(
                camera_fd,
                VIDIOC_QBUF,
                &buf
            );

            break;
        }

        /*
         * Calculate average luminance.
         *
         * YUYV layout:
         * Y0 U0 Y1 V0 ...
         */
        uint64_t luminance_sum = 0;

        for (size_t i = 0;
             i < FRAME_SIZE;
             i += 2) {

            luminance_sum += frame[i];
        }

        double average_y =
            (double)luminance_sum /
            (FRAME_SIZE / 2);

        frame_count++;

        printf(
    "Frame %3u sent | %zu bytes | average Y = %.2f\n",
    frame_count,
    (size_t)FRAME_SIZE,
    average_y
        );

        /*
         * Return buffer to V4L2.
         */
        if (xioctl(
            camera_fd,
            VIDIOC_QBUF,
            &buf
        ) == -1) {

            perror("VIDIOC_QBUF");
            break;
        }
    }

    xioctl(
        camera_fd,
        VIDIOC_STREAMOFF,
        &type
    );

    for (unsigned int i = 0;
         i < n_buffers;
         i++) {

        munmap(
            buffers[i].start,
            buffers[i].length
        );
    }

    close(socket_fd);
    close(camera_fd);

    printf("\nTransmission complete.\n");
    printf(
        "Frames sent: %u\n",
        frame_count
    );

    return EXIT_SUCCESS;
}
