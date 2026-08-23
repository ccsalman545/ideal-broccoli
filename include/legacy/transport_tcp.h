#ifndef TRANSPORT_TCP_H
#define TRANSPORT_TCP_H

#include <stddef.h>

typedef struct TcpConnection TcpConnection;

TcpConnection *tcp_connect(
    const char *address,
    unsigned short port
);

int tcp_send_all(
    TcpConnection *connection,
    const void *data,
    size_t size
);

void tcp_close(TcpConnection *connection);

#endif
