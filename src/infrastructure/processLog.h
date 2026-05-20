// === src/infrastructure/processLog.h ===

#ifndef PROCESS_LOG_H
#define PROCESS_LOG_H

struct Logger;
struct Process;

// Solo referencia, no destruye Logger
typedef struct ProcessLog {
    struct Logger* logger;
} ProcessLog;

ProcessLog* processLogCreate(struct Logger* logger);

void processLogDestroy(ProcessLog* log);

void processLogRecordCreation(ProcessLog* log, struct Process* process);

void processLogRecordTermination(ProcessLog* log, struct Process* process);

void processLogRecordStateChange(ProcessLog* log, struct Process* process, int newState);

void processLogFlush(ProcessLog* log);

#endif
