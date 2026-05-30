#ifndef PROCESS_TABLE_H
#define PROCESS_TABLE_H

#include "../../utils/constants.h"
#include "process.h"

struct ReadyQueue;
struct IoQueue;

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
    int totalCpuCyclesExecuted;
    int totalCpuWasteCycles;
    int totalContextSwitchTime;
    float cpuWasteRatio;
    int memoryUsedBlocks;
    int memoryFreeBlocks;
    int largestFreeRun;
    int freeRunCount;
    int totalWaitingTimeFinished;
    int totalTurnaroundTimeFinished;
    int totalExecutionTimeFinished;
    int totalContextSwitches;
    int algorithmChangeCount;
    int totalIoOperations;
    float cpuUtilization;
    int internalWaste;
    int externalWaste;
    int totalPageFaults;
    float fragmentation;
    int quantumCurrent;
    int quantumHistory[historialAlgoritmoMaximo];
    float proportionReady;
    float proportionWaiting;
    int iterationsSinceBalance;
    int shouldSwitchAlgorithm;
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
