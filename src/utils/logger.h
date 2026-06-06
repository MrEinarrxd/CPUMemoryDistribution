#ifndef CpuMemoryLoggerH
#define CpuMemoryLoggerH

#include <stdio.h>

typedef struct Logger {
    FILE* tableLog;
    FILE* bcpLog;
} Logger;

int loggerOpen(Logger* logger, const char* tablePath, const char* bcpPath);
void loggerClose(Logger* logger);
void loggerTableLine(Logger* logger, const char* line);
void loggerBcpLine(Logger* logger, const char* line);

#endif
