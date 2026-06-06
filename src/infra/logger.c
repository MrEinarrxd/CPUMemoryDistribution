#include "logger.h"

#include <stdarg.h>
#include <time.h>

int logger_open(Logger* logger, const char* path) {
    if (!logger || !path) return -1;
    logger->file = fopen(path, "w");
    return logger->file ? 0 : -1;
}

void logger_close(Logger* logger) {
    if (!logger) return;
    if (logger->file) fclose(logger->file);
    logger->file = NULL;
}

void logger_info(Logger* logger, const char* fmt, ...) {
    if (!logger || !logger->file || !fmt) return;
    time_t now = time(NULL);
    struct tm* local = localtime(&now);
    if (local) {
        fprintf(logger->file, "[%04d-%02d-%02d %02d:%02d:%02d] ",
                local->tm_year + 1900, local->tm_mon + 1, local->tm_mday,
                local->tm_hour, local->tm_min, local->tm_sec);
    }
    va_list args;
    va_start(args, fmt);
    vfprintf(logger->file, fmt, args);
    va_end(args);
    fputc('\n', logger->file);
}

void logger_flush(Logger* logger) {
    if (logger && logger->file) fflush(logger->file);
}
