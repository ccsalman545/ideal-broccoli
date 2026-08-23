#define _POSIX_C_SOURCE 200809L

#include "camera_v4l2.h"
#include "frame.h"
#include "transport_tcp.h"

#include <stdio.h>
#include <stdlib.h>

#define CAMERA_DEVICE "/dev/video0"

#define CAMERA_WIDTH 640
#define CAMERA_HEIGHT 480
#define CAMERA_FPS 30

#define SERVER_ADDRESS "192.168.1.20"
#define SERVER_PORT 5000

#define FRAME_COUNT 100

int main(void)
{
    Camera *camera = camera_open(
        CAMERA_DEVICE,
        CAMERA_WIDTH,
        CAMERA_HEIGHT,
        CAMERA_FPS
    );

    if (!camera) {
        fprintf(stderr, "Failed to initialize camera\n");
        return EXIT_FAILURE;
    }

    if (camera_start(camera) != 0) {
        fprintf(stderr, "Failed to start camera\n");
        camera_close(camera);
        return EXIT_FAILURE;
    }

    TcpConnection *connection = tcp_connect(
        SERVER_ADDRESS,
        SERVER_PORT
    );

    if (!connection) {
        camera_close(camera);
        return EXIT_FAILURE;
    }

    printf("\nStarting camera transport...\n\n");

    unsigned int captured = 0;

    while (captured < FRAME_COUNT) {

        Frame frame;

        int result = camera_capture(
            camera,
            &frame
        );

        if (result < 0) {
            fprintf(stderr, "Camera capture failed\n");
            break;
        }

        if (result == 0)
            continue;

        double average_y =
            frame_average_luminance(&frame);

        if (tcp_send_all(
            connection,
            frame.data,
            frame.size
        ) != 0) {

            fprintf(
                stderr,
                "Frame transmission failed\n"
            );

            camera_release_frame(
                camera,
                &frame
            );

            break;
        }

        captured++;

        printf(
            "Frame %3u | sequence=%llu | "
            "size=%zu | average Y=%.2f\n",
            captured,
            (unsigned long long)frame.sequence,
            frame.size,
            average_y
        );

        camera_release_frame(
            camera,
            &frame
        );
    }

    tcp_close(connection);
    camera_close(camera);

    printf(
        "\nCamera transport finished.\n"
        "Frames sent: %u\n",
        captured
    );

    return captured == FRAME_COUNT
        ? EXIT_SUCCESS
        : EXIT_FAILURE;
}
