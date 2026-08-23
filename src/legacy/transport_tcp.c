#define _POSIX_C_SOURCE 200809L

#include "transport_tcp.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

struct TcpConnection {
    int fd;
};

TcpConnection *tcp_connect(
    const char *address,
    unsigned short port
)
{
    if (!address) {
        fprintf(stderr, "TCP address is NULL\n");
        return NULL;
    }

    TcpConnection *connection =
        calloc(1, sizeof(*connection));

    if (!connection) {
        perror("calloc");
        return NULL;
    }

    connection->fd = socket(
        AF_INET,
        SOCK_STREAM,
        0
    );

    if (connection->fd < 0) {
        perror("socket");
        free(connection);
        return NULL;
    }

    struct sockaddr_in server;

    memset(&server, 0, sizeof(server));

    server.sin_family = AF_INET;
    server.sin_port = htons(port);

    if (inet_pton(
        AF_INET,
        address,
        &server.sin_addr
    ) != 1) {

        fprintf(
            stderr,
            "Invalid IPv4 address: %s\n",
            address
        );

        close(connection->fd);
        free(connection);
        return NULL;
    }

    printf(
        "Connecting to %s:%u...\n",
        address,
        port
    );

    if (connect(
        connection->fd,
        (struct sockaddr *)&server,
        sizeof(server)
    ) < 0) {

        fprintf(
            stderr,
            "TCP connection failed: %s\n",
            strerror(errno)
        );

        close(connection->fd);
        free(connection);
        return NULL;
    }

    printf("TCP connection established.\n");

    return connection;
}

int tcp_send_all(
    TcpConnection *connection,
    const void *data,
    size_t size
)
{
    if (!connection || connection->fd < 0 ||
        (!data && size > 0)) {
        return -1;
    }

    const unsigned char *ptr = data;
    size_t sent = 0;

    while (sent < size) {

        ssize_t result = send(
            connection->fd,
            ptr + sent,
            size - sent,
            0
        );

        if (result < 0) {

            if (errno == EINTR)
                continue;

            perror("send");
            return -1;
        }

        if (result == 0)
            return -1;

        sent += (size_t)result;
    }

    return 0;
}

void tcp_close(TcpConnection *connection)
{
    if (!connection)
        return;

    if (connection->fd >= 0)
        close(connection->fd);

    free(connection);
}
