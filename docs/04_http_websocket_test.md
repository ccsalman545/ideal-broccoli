# Stage 04 — HTTP and WebSocket Communication

## Objective

Integrate the Mongoose networking library into the portable C project and establish HTTP and WebSocket communication between the Fedora sender PC and the Ubuntu receiver PC.

## Network Configuration

| Device | IP Address | Role |
|---|---|---|
| Fedora PC | 192.168.1.10 | Camera/server |
| Ubuntu PC | 192.168.1.20 | Receiver/client |

HTTP/WebSocket server:

- Address: 192.168.1.10
- Port: 8080
- HTTP endpoint: http://192.168.1.10:8080/
- WebSocket endpoint: ws://192.168.1.10:8080/ws

## Software Components

- C11
- GCC
- Linux V4L2
- Mongoose 7.23
- POSIX networking APIs
- Make build system

## Implementation

The HTTP server was separated into a modular interface:

```text
include/http_server.h
        |
        v
src/http_server.c
        |
        v
Mongoose 7.23
