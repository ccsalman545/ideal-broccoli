/*
 * app_server.h
 *
 * Top level server: wires source, encoder and WebRTC
 * transports into one process and runs the network loop.
 */
#ifndef APP_APP_SERVER_H
#define APP_APP_SERVER_H

#include <signal.h>

#include "app_config.h"

/*
 * Run the server until *stop_flag becomes nonzero.
 * Returns a process exit code.
 */
int app_server_run(const AppConfig *config, volatile sig_atomic_t *stop_flag);

#endif
