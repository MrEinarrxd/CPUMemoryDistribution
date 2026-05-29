#include "bcpLog.h"
#include "logger.h"
#include "../domain/process/bcp.h"
#include <stdlib.h>

BcpLog* bcpLogCreate(Logger* logger) {
    BcpLog* log = (BcpLog*)malloc(sizeof(BcpLog));
    if (log) log->logger = logger;
    return log;
}
void bcpLogDestroy(BcpLog* log) { free(log); }

void bcpLogRecordContextSwitch(BcpLog* log, Bcp* bcp) {
    if (log && log->logger && bcp)
        loggerLogFormat(log->logger, LogLevelInfo, "Context switch: proceso %s, cambios: %d", bcp->processId, bcp->contextSwitchCount);
}
void bcpLogRecordQuantumExpired(BcpLog* log, Bcp* bcp) {
    if (log && log->logger && bcp)
        loggerLogFormat(log->logger, LogLevelInfo, "Quantum expirado: proceso %s", bcp->processId);
}
void bcpLogRecordPageSwap(BcpLog* log, Bcp* bcp, int swapAddress) {
    if (log && log->logger && bcp)
        loggerLogFormat(log->logger, LogLevelInfo, "Page swap: proceso %s, dirección swap %d", bcp->processId, swapAddress);
}
void bcpLogFlush(BcpLog* log) { if (log && log->logger) loggerFlush(log->logger); }
void bcpLogRecordFull(BcpLog* log, Bcp* bcp) {
    if (!log || !log->logger || !bcp) return;
    /* Linea 1: identificacion y planificacion (13 variables) */
    loggerLogFormat(log->logger, LogLevelDebug,
        "BCP[1/2]: id=%s pid=%d state=%d prio=%d totalCpu=%d remaining=%d"
        " arrival=%d creationTime=%d startTime=%d finishTime=%d"
        " timeInExec=%d timeInWaiting=%d timesExecuted=%d",
        bcp->processId, bcp->pid, (int)bcp->state, bcp->priority,
        bcp->totalCpuCycles, bcp->remainingCycles,
        bcp->arrivalTime, bcp->creationTime, bcp->startTime, bcp->finishTime,
        bcp->timeInExecution, bcp->timeInWaiting, bcp->timesExecuted);
    /* Linea 2: quantum, memoria, E/S y envejecimiento (12 variables) */
    loggerLogFormat(log->logger, LogLevelDebug,
        "BCP[2/2]: id=%s quantumAssigned=%d quantumUsed=%d wastedCycles=%d"
        " cpuWasteRatio=%.4f agingCounter=%d timesReturned=%d ageingSlices=%d"
        " ctxSwitches=%d ctxSwitchTime=%d timesInIo=%d ioTimeRemaining=%d"
        " ioPending=%d pageCount=%d memReq=%d totalMemAlloc=%d pageTableBase=%d"
        " swapAddress=%d currentTimeSlice=%d ioOperPending=%d",
        bcp->processId, bcp->quantumAssigned, bcp->quantumUsed, bcp->wastedCpuCycles,
        bcp->cpuWasteRatio, bcp->agingCounter, bcp->timesReturnedToReady,
        bcp->ageingTimeSlices, bcp->contextSwitchCount, bcp->contextSwitchTime,
        bcp->timesInIo, bcp->ioTimeRemaining,
        bcp->ioOperationsPending, bcp->pageCount, bcp->memoryRequested,
        bcp->totalMemoryAllocated, bcp->pageTableBase,
        bcp->swapAddress, bcp->currentTimeSlice, bcp->ioOperationsPending);
}