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
    if (finished == 0) return;
    float totalWaiting = 0.0f, totalTurnaround = 0.0f;
    for (int i = 0; i < procesosEnEjecucion; i++) {
        Process* p = table->runningProcesses[i];
        if (p && p->bcp && p->bcp->state == ProcessStateFinished) {
            totalWaiting += p->bcp->timeInWaiting;
            totalTurnaround += (p->bcp->finishTime - p->bcp->arrivalTime);
        }
    }
    for (int i = 0; i < procesosEnEspera; i++) {
        Process* p = table->newRequests[i];
        if (p && p->bcp && p->bcp->state == ProcessStateFinished) {
            totalWaiting += p->bcp->timeInWaiting;
            totalTurnaround += (p->bcp->finishTime - p->bcp->arrivalTime);
        }
    }
    table->avgWaitingTime = totalWaiting / finished;
    table->avgTurnaroundTime = totalTurnaround / finished;

    if (table->currentCycle > 0) {
        table->cpuUtilization = (float)table->totalCpuCyclesExecuted / (float)table->currentCycle;
        if (table->cpuUtilization > 1.0f) table->cpuUtilization = 1.0f;
        
        // DEBUG: mostrar el cálculo cada 50 ciclos
        if (table->currentCycle % 50 == 0) {
            fprintf(stderr, "[DEBUG STATS] Ciclo: %d | totalCpuCyclesExecuted: %d | Utilización: %.4f (%.1f%%)\n",
                   table->currentCycle, table->totalCpuCyclesExecuted, table->cpuUtilization, table->cpuUtilization * 100.0f);
            fflush(stderr);
        }
    }
}