#include "processTable.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

ProcessTable* processTableCreate(void) {
    ProcessTable* table = (ProcessTable*)calloc(1, sizeof(ProcessTable));
    if (!table) return NULL;
    table->totalProcesses = 0;
    table->finishedProcesses = 0;
    table->currentCycle = 0;
    table->totalCpuCyclesExecuted = 0;
    table->totalCpuWasteCycles = 0;
    table->cpuWasteRatio = 0.0f;
    table->memoryUsedBlocks = 0;
    table->memoryFreeBlocks = 0;
    table->largestFreeRun = 0;
    table->freeRunCount = 0;
    table->totalWaitingTimeFinished = 0;
    table->totalTurnaroundTimeFinished = 0;
    table->totalExecutionTimeFinished = 0;
    table->totalContextSwitches = 0;
    table->algorithmChangeCount = 0;
    table->totalIoOperations = 0;
    table->cpuUtilization = 0.0f;
    table->internalWaste = 0;
    table->externalWaste = 0;
    table->totalPageFaults = 0;
    table->fragmentation = 0.0f;
    table->quantumCurrent = 0;
    table->proportionReady = 0.0f;
    table->proportionWaiting = 0.0f;
    table->iterationsSinceBalance = 0;
    table->shouldSwitchAlgorithm = 0;
    table->avgWaitingTime = 0.0f;
    table->avgTurnaroundTime = 0.0f;
    table->simulationStartTime = 0;
    memset(table->runningProcesses, 0, sizeof(table->runningProcesses));
    memset(table->newRequests, 0, sizeof(table->newRequests));
    memset(table->quantumHistory, 0, sizeof(table->quantumHistory));
    return table;
}

void processTableDestroy(ProcessTable* table) {
    if (!table) return;
    for (int i = 0; i < procesosEnEjecucion; i++)
        if (table->runningProcesses[i]) processDestroy(table->runningProcesses[i]);
    for (int i = 0; i < procesosEnEspera; i++)
        if (table->newRequests[i]) processDestroy(table->newRequests[i]);
    free(table);
}

int processTableAddRunning(ProcessTable* table, Process* process) {
    if (!table || !process) return -1;
    for (int i = 0; i < procesosEnEjecucion; i++)
        if (table->runningProcesses[i] == NULL) { table->runningProcesses[i] = process; return 0; }
    return -1;
}

int processTableAddNew(ProcessTable* table, Process* process) {
    if (!table || !process) return -1;
    for (int i = 0; i < procesosEnEspera; i++)
        if (table->newRequests[i] == NULL) { table->newRequests[i] = process; return 0; }
    return -1;
}

Process* processTableGetRunning(ProcessTable* table, int index) {
    if (!table || index < 0 || index >= procesosEnEjecucion) return NULL;
    return table->runningProcesses[index];
}

Process* processTableGetNew(ProcessTable* table, int index) {
    if (!table || index < 0 || index >= procesosEnEspera) return NULL;
    return table->newRequests[index];
}

void processTableRemoveRunning(ProcessTable* table, int index) {
    if (table && index >= 0 && index < procesosEnEjecucion) table->runningProcesses[index] = NULL;
}

int processTableGetTotalProcesses(ProcessTable* table) { return table ? table->totalProcesses : 0; }
int processTableGetFinishedProcesses(ProcessTable* table) { return table ? table->finishedProcesses : 0; }
void processTableIncrementFinished(ProcessTable* table) { if (table) table->finishedProcesses++; }

void processTableUpdateAverages(ProcessTable* table) {
    if (!table) return;
    int finished = table->finishedProcesses;
    if (finished > 0) {
        table->avgWaitingTime = (float)table->totalWaitingTimeFinished / finished;
        table->avgTurnaroundTime = (float)table->totalTurnaroundTimeFinished / finished;
    }

    if (table->currentCycle > 0) {
        float capacity = (float)table->currentCycle * (float)ciclosPorInstanciaMax;
        table->cpuUtilization = capacity > 0.0f
            ? (float)table->totalCpuCyclesExecuted / capacity
            : 0.0f;
        if (table->cpuUtilization > 1.0f) table->cpuUtilization = 1.0f;
        if (table->cpuUtilization < 0.0f) table->cpuUtilization = 0.0f;

        int accountedCpu = table->totalCpuCyclesExecuted + table->totalCpuWasteCycles;
        table->cpuWasteRatio = accountedCpu > 0
            ? (float)table->totalCpuWasteCycles / (float)accountedCpu
            : 0.0f;
        
#ifdef DEBUG_VERBOSE
        if (table->currentCycle % 50 == 0) {
            fprintf(stderr, "[DEBUG STATS] Ciclo: %d | totalCpuCyclesExecuted: %d | Utilización: %.4f (%.1f%%)\n",
                   table->currentCycle, table->totalCpuCyclesExecuted, table->cpuUtilization, table->cpuUtilization * 100.0f);
            fflush(stderr);
        }
#endif
    }
}
