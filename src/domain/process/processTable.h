// === src/domain/process/processTable.h ===

#ifndef PROCESS_TABLE_H
#define PROCESS_TABLE_H

#include "../../utils/constants.h"
#include "process.h"

struct ReadyQueue;
struct IoQueue;

// Propietario exclusivo de Process arrays
// Solo referencia a ReadyQueue e IoQueue
typedef struct ProcessTable {
    Process* runningProcesses[procesosEnEjecucion];
    Process* newRequests[procesosEnEspera];
    int totalProcesses;
    int finishedProcesses;
    struct ReadyQueue* readyQueue;
    struct IoQueue* ioQueue;
    float avgWaitingTime;
    float avgTurnaroundTime;
    int currentCycle;
    int simulationStartTime;
} ProcessTable;

ProcessTable* processTableCreate(void);

void processTableDestroy(ProcessTable* table);

int processTableAddRunning(ProcessTable* table, Process* process);

int processTableAddNew(ProcessTable* table, Process* process);

Process* processTableGetRunning(ProcessTable* table, int index);

Process* processTableGetNew(ProcessTable* table, int index);

void processTableRemoveRunning(ProcessTable* table, int index);

int processTableGetTotalProcesses(ProcessTable* table);

int processTableGetFinishedProcesses(ProcessTable* table);

void processTableIncrementFinished(ProcessTable* table);

void processTableUpdateAverages(ProcessTable* table);

#endif
