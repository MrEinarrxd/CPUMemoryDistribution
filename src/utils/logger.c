#include "logger.h"

int loggerOpen(Logger* logger, const char* tablePath, const char* bcpPath) {
    if (!logger) return -1;
    logger->tableLog = fopen(tablePath, "w");
    logger->bcpLog = fopen(bcpPath, "w");
    if (!logger->tableLog || !logger->bcpLog) {
        loggerClose(logger);
        return -1;
    }
    return 0;
}

void loggerClose(Logger* logger) {
    if (!logger) return;
    if (logger->tableLog) fclose(logger->tableLog);
    if (logger->bcpLog) fclose(logger->bcpLog);
    logger->tableLog = NULL;
    logger->bcpLog = NULL;
}

void loggerTableLine(Logger* logger, const char* line) {
    if (!logger || !logger->tableLog || !line) return;
    fputs(line, logger->tableLog);
    fputc('\n', logger->tableLog);
    fflush(logger->tableLog);
}

void loggerBcpLine(Logger* logger, const char* line) {
    if (!logger || !logger->bcpLog || !line) return;
    fputs(line, logger->bcpLog);
    fputc('\n', logger->bcpLog);
    fflush(logger->bcpLog);
}
