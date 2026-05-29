#include "logger.h"
#include "../utils/timeHelper.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char* levelToString(LogLevel level) {
    switch (level) {
        case LogLevelDebug:    return "DEBUG";
        case LogLevelInfo:     return "INFO";
        case LogLevelWarning:  return "WARNING";
        case LogLevelError:    return "ERROR";
        case LogLevelCritical: return "CRITICAL";
        default:               return "UNKNOWN";
    }
}

Logger* loggerCreate(const char* fileName, LogLevel minLevel) {
    if (!fileName) return NULL;
    Logger* logger = (Logger*)malloc(sizeof(Logger));
    if (!logger) return NULL;
    logger->logFile = fopen(fileName, "a");
    if (!logger->logFile) {
        free(logger);
        return NULL;
    }
    logger->minLevel = minLevel;
    return logger;
}

void loggerDestroy(Logger* logger) {
    if (logger) {
        if (logger->logFile) fclose(logger->logFile);
        free(logger);
    }
}

void loggerLog(Logger* logger, LogLevel level, const char* message) {
    if (!logger || !logger->logFile) return;
    if (level < logger->minLevel) return;
    long ms = timeHelperGetCurrentTime();
    time_t sec = ms / 1000;
    struct tm* tm_info = localtime(&sec);
    char timeBuf[20];
    strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", tm_info);
    fprintf(logger->logFile, "[%s.%03ld] %s: %s\n", timeBuf, ms % 1000, levelToString(level), message);
    fflush(logger->logFile);
}

void loggerLogFormat(Logger* logger, LogLevel level, const char* format, ...) {
    if (!logger || !logger->logFile) return;
    if (level < logger->minLevel) return;
    va_list args;
    va_start(args, format);
    char buffer[tamanoBufferLog];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    loggerLog(logger, level, buffer);
}

void loggerFlush(Logger* logger) {
    if (logger && logger->logFile) fflush(logger->logFile);
}