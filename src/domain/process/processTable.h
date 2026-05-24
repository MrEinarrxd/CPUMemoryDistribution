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
    int totalCpuCyclesExecuted;                         // Total CPU cycles executed by all processes
    int totalContextSwitches;                           // Total context switches performed
    int algorithmChangeCount;                           // Number of times algorithm was changed
    int totalIoOperations;                              // Total I/O operations performed
    float cpuUtilization;                               // CPU utilization percentage
    int internalWaste;                                  // Internal fragmentation
    int externalWaste;                                  // External fragmentation
    int totalPageFaults;                                // Total page faults occurred
    float fragmentation;                                // Current fragmentation level
    int quantumCurrent;                                 // Current quantum value
    int quantumHistory[20];                             // History of quantum changes
    float proportionReady;                              // Current proportion ready/total
    float proportionWaiting;                            // Current proportion waiting/total
    int iterationsSinceBalance;                         // Iterations since last balance
    int shouldSwitchAlgorithm;                          // Flag: should switch algorithm
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
