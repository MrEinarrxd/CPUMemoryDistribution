// === src/domain/process/bcp.h ===

#ifndef BCP_H
#define BCP_H

#include "../../utils/constants.h"

typedef enum {
    ProcessStateNew,
    ProcessStateReady,
    ProcessStateRunning,
    ProcessStateWaitingIo,
    ProcessStateSwap,
    ProcessStateSuspended,
    ProcessStateFinished
} ProcessState;

typedef struct Bcp {
    char processId[idProcesoLen];
    int pid;
    ProcessState state;
    int priority;
    int totalCpuCycles;
    int remainingCycles;
    int arrivalTime;
    int currentTimeSlice;
    int contextSwitchTime;
    int timeInExecution;
    int timesExecuted;
    int creationTime;
    int startTime;
    int finishTime;
    int timeInWaiting;
    int ageingTimeSlices;
    int timesInIo;
    int ioTimeRemaining;   // Tiempo restante en cola de E/S (1-100 * multiplicador)
    char phraseBuffer[500];
    char requiredWords[palabrasPorFrase][maxCaracteresPalabra];
    int requiredWordsCount;
    int pageCount;
    int memoryRequested;
    int totalMemoryAllocated;
    int assignedPages[maxPaginasPorProceso];
    int quantumAssigned;
    int quantumUsed;
    float cpuWasteRatio;
    int agingCounter;
    int timesReturnedToReady;
    int wastedCpuCycles;
    int pageTableBase;
    int swapAddress;
    int ioOperationsPending;
    int contextSwitchCount;
} Bcp;

Bcp* bcpCreate(const char* processId, int pid);

void bcpDestroy(Bcp* bcp);

void bcpInitialize(Bcp* bcp, int priority, int executionTime, int arrivalTime);

void bcpSetState(Bcp* bcp, ProcessState state);

ProcessState bcpGetState(Bcp* bcp);

void bcpIncrementContextSwitches(Bcp* bcp);

void bcpUpdateRemainingTime(Bcp* bcp, int cycles);

void bcpAddPage(Bcp* bcp, int pageIndex);

void bcpAddPhrase(Bcp* bcp, const char* phrase);

#endif
