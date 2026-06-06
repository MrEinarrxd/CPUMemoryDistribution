#include "bcp.h"

#include <string.h>

void bcpInit(Bcp* bcp, int pid, const char* processId, int arrival, int cycles, int active) {
    if (!bcp) return;
    memset(bcp, 0, sizeof(*bcp));
    strncpy(bcp->processId, processId, ProcessIdLen - 1);
    bcp->processId[ProcessIdLen - 1] = '\0';
    bcp->pid = pid;
    bcp->state = active ? processStateReady : processStateNew;
    bcp->generatedAsActive = active;
    bcp->arrivalTime = arrival;
    bcp->totalCpuCycles = cycles;
    bcp->remainingCpuCycles = cycles;
    bcp->startTime = -1;
    bcp->finishTime = -1;
    bcp->lastReadyTime = arrival;
    bcp->ioDevice = -1;
}

const char* processStateName(ProcessState state) {
    switch (state) {
        case processStateNew: return "NEW";
        case processStateReady: return "READY";
        case processStateRunning: return "RUNNING";
        case processStateWaitingIo: return "WAITING_IO";
        case processStateFinished: return "FINISHED";
        default: return "UNKNOWN";
    }
}
