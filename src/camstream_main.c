#define _POSIX_C_SOURCE 200809L

/*
 * camstream_main.c
 *
 * Process entry point: configure, install signal handlers and
 * hand control to the server until SIGINT or SIGTERM.
 */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/app_config.h"
#include "app/app_server.h"

static volatile sig_atomic_t g_stop = 0;

static void handle_signal(int signal_number)
{
    (void) signal_number;
    g_stop = 1;
}

int main(int argc, char **argv)
{
    AppConfig config;

    app_config_defaults(&config);

    int parsed = app_config_parse(&config, argc, argv);

    if (parsed == 1) {
        app_config_print_usage(argv[0]);
        return 0;
    }

    if (parsed != 0) {
        return 2;
    }

    struct sigaction action;

    memset(&action, 0, sizeof(action));
    action.sa_handler = handle_signal;
    sigemptyset(&action.sa_mask);

    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);

    /*
     * Mongoose writes to sockets; a vanished viewer must not
     * kill the process.
     */
    signal(SIGPIPE, SIG_IGN);

    printf("camstream %s starting\n", APP_VERSION);
    app_config_print_summary(&config);
    printf("\n");

    return app_server_run(&config, &g_stop);
}
