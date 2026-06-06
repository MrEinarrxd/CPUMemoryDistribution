#include "bcp.h"
#include <stdlib.h>
#include <string.h>

Bcp* bcpCreate(const char* processId, int pid) {
    Bcp* bcp = (Bcp*)calloc(1, sizeof(Bcp));
    if (!bcp) return NULL;
    if (processId) {
        strncpy(bcp->processId, processId, idProcesoLen - 1);
        bcp->processId[idProcesoLen - 1] = '\0';
    }
    bcp->pid = pid;
    bcp->state = ProcessStateNew;
    bcp->priority = 0;
    bcp->totalCpuCycles = 0;
    bcp->remainingCycles = 0;
    bcp->arrivalTime = 0;
    bcp->currentTimeSlice = 0;
    bcp->contextSwitchTime = 0;
    bcp->timeInExecution = 0;
    bcp->timesExecuted = 0;
    bcp->creationTime = 0;
    bcp->startTime = 0;
    bcp->finishTime = 0;
    bcp->timeInWaiting = 0;
    bcp->ageingTimeSlices = 0;
    bcp->timesInIo = 0;
    bcp->ioTimeRemaining = 0;
    bcp->pageCount = 0;
    bcp->memoryRequested = 0;
    bcp->totalMemoryAllocated = 0;
    bcp->quantumAssigned = 0;
    bcp->quantumUsed = 0;
    bcp->cpuWasteRatio = 0.0f;
    bcp->agingCounter = 0;
    bcp->timesReturnedToReady = 0;
    bcp->wastedCpuCycles = 0;
    bcp->pageTableBase = -1;
    bcp->swapAddress = -1;
    bcp->ioOperationsPending = 0;
    bcp->contextSwitchCount = 0;
    bcp->requiredWordsCount = 0;
    for (int i = 0; i < maxPaginasPorProceso; i++)
        bcp->assignedPages[i] = -1;
    memset(bcp->ioPhrase, 0, sizeof(bcp->ioPhrase));
    for (int i = 0; i < palabrasPorFrase; i++)
        memset(bcp->requiredWords[i], 0, maxCaracteresPalabra);
    return bcp;
}

void bcpDestroy(Bcp* bcp) { if (bcp) free(bcp); }

void bcpInitialize(Bcp* bcp, int priority, int executionTime, int arrivalTime) {
    if (!bcp) return;
    bcp->priority = priority;
    bcp->totalCpuCycles = executionTime;
    bcp->remainingCycles = executionTime;
    bcp->arrivalTime = arrivalTime;
}

void bcpSetState(Bcp* bcp, ProcessState state) { if (bcp) bcp->state = state; }
ProcessState bcpGetState(Bcp* bcp) { return bcp ? bcp->state : ProcessStateFinished; }
void bcpIncrementContextSwitches(Bcp* bcp) { if (bcp) bcp->contextSwitchCount++; }
void bcpUpdateRemainingTime(Bcp* bcp, int cycles) {
    if (!bcp) return;
    if (cycles > bcp->remainingCycles) cycles = bcp->remainingCycles;
    bcp->remainingCycles -= cycles;
}

void bcpAddPage(Bcp* bcp, int pageIndex) {
    if (!bcp) return;
    for (int i = 0; i < maxPaginasPorProceso; i++) {
        if (bcp->assignedPages[i] == -1) {
            bcp->assignedPages[i] = pageIndex;
            bcp->pageCount++;
            break;
        }
    }
}

void bcpAddPhrase(Bcp* bcp, const char* phrase) {
    if (!bcp || !phrase) return;
    strncpy(bcp->ioPhrase, phrase, tamanoFraseIo - 1);
    bcp->ioPhrase[tamanoFraseIo - 1] = '\0';
    char buffer[tamanoFraseIo];
    strcpy(buffer, bcp->ioPhrase);
    char* ptr = buffer;
    int count = 0;
    while (*ptr && count < palabrasPorFrase) {
        while (*ptr == ' ') ptr++;
        if (!*ptr) break;
        char* start = ptr;
        while (*ptr && *ptr != ' ') ptr++;
        int len = (ptr - start);
        if (len >= maxCaracteresPalabra) len = maxCaracteresPalabra - 1;
        strncpy(bcp->requiredWords[count], start, len);
        bcp->requiredWords[count][len] = '\0';
        count++;
    }
    bcp->requiredWordsCount = count;
}
