#ifndef LOGGER_H
#define LOGGER_H

#include <stdint.h>

enum log_level {
    INFO = 0,
    DEBUG,
    WARNING,
    ERROR,
};

void
ginger_log(uint8_t log_level, const char* fmt, ...);

#endif // LOGGER_H
