#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdarg.h>
#include "../utils/constants.h"

typedef enum {
    LogLevelDebug,
    LogLevelInfo,
    LogLevelWarning,
    LogLevelError,
    LogLevelCritical
} LogLevel;

typedef struct Logger {
    FILE* logFile;
    LogLevel minLevel;
} Logger;

Logger* loggerCreate(const char* fileName, LogLevel minLevel);
void loggerDestroy(Logger* logger);
void loggerLog(Logger* logger, LogLevel level, const char* message);
void loggerLogFormat(Logger* logger, LogLevel level, const char* format, ...);
void loggerFlush(Logger* logger);

#endif
