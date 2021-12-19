#ifndef SIG_HANDLER_H
#define SIG_HANDLER_H

#include <signal.h>

#ifndef UNUSED
# define UNUSED(x) UNUSED_ ## x __attribute__((unused))
#endif

void
init_sig_handler(void);

void
sigint_handler(int UNUSED(sig), siginfo_t* si, void* UNUSED(unused));

#endif
