#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include "sig_handler.h"

void
sigint_handler(int UNUSED(sig), siginfo_t* si, void* UNUSED(unused))
{
    printf("Got a SIGINT!\n");
    exit(0);
}

void
init_sig_handler(void)
{
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = sigint_handler;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Failed to install signal handler");
        exit(1);
    }
}
