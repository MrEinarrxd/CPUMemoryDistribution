#ifndef INFRA_LOGGER_H
#define INFRA_LOGGER_H

#include <stdio.h>

typedef struct Logger {
    FILE* file;
} Logger;

int logger_open(Logger* logger, const char* path);
void logger_close(Logger* logger);
void logger_info(Logger* logger, const char* fmt, ...);
void logger_flush(Logger* logger);

#endif
