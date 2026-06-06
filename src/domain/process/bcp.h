#ifndef CpuMemoryBcpH
#define CpuMemoryBcpH

#include "../../utils/constants.h"

typedef enum ProcessState {
    processStateNew = 0,
    processStateReady,
    processStateRunning,
    processStateWaitingIo,
    processStateFinished
} ProcessState;

typedef struct Bcp {
    char processId[ProcessIdLen];
    int pid;
    ProcessState state;
    int priority;
    int generatedAsActive;
    int arrivalTime;
    int creationDelay;
    int startTime;
    int finishTime;
    int lastReadyTime;
    int totalCpuCycles;
    int remainingCpuCycles;
    int currentInstanceCycles;
    int contextSwitchTime;
    int contextSwitchCount;
    int totalContextSwitchTime;
    int timeInExecution;
    int timesExecuted;
    int waitingTime;
    int turnaroundTime;
    int ioDevice;
    int ioTimeRemaining;
    int timesInIo;
    int ioOperationsPending;
    char ioPhrase[PhraseLen];
    int memoryRequested;
    int totalMemoryAllocated;
    int pageCount;
    int pageFaults;
    int swapIns;
    int swapOuts;
    int quantumAssigned;
    int quantumUsed;
    int rrExecutionCount;
    int rrQuantumAssignedTotal;
    int rrQuantumUsedTotal;
    int cpuWasteCycles;
    float cpuWasteRatio;
    int agingCounter;
    int timesReturnedToReady;
    int privileged;
} Bcp;

void bcpInit(Bcp* bcp, int pid, const char* processId, int arrival, int cycles, int active);
const char* processStateName(ProcessState state);

#endif
