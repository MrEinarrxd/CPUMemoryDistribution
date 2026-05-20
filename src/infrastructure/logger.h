// === src/infrastructure/logger.h ===

#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include "../../utils/constants.h"

typedef enum {
    LogLevelDebug,
    LogLevelInfo,
    LogLevelWarning,
    LogLevelError,
    LogLevelCritical
} LogLevel;

// Propietario exclusivo de FILE
typedef struct Logger {
    FILE* logFile;
    LogLevel minLevel;
} Logger;

Logger* loggerCreate(const char* fileName, LogLevel minLevel);

void loggerDestroy(Logger* logger);

void loggerLog(Logger* logger, LogLevel level, const char* message);

void loggerFlush(Logger* logger);

#endif
