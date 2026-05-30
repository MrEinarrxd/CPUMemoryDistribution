#include "processLog.h"
#include "logger.h"
#include "../domain/process/process.h"
#include "../domain/process/processTable.h"
#include <stdlib.h>

ProcessLog* processLogCreate(Logger* logger) {
    ProcessLog* pl = (ProcessLog*)malloc(sizeof(ProcessLog));
    if (pl) pl->logger = logger;
    return pl;
}
void processLogDestroy(ProcessLog* log) { free(log); }
void processLogRecordCreation(ProcessLog* log, Process* process) {
    if (log && log->logger && process && process->bcp)
        loggerLogFormat(log->logger, LogLevelInfo, "Proceso creado: %s", process->bcp->processId);
}
void processLogRecordTermination(ProcessLog* log, Process* process) {
    if (log && log->logger && process && process->bcp)
        loggerLogFormat(log->logger, LogLevelInfo, "Proceso terminado: %s", process->bcp->processId);
}
void processLogRecordStateChange(ProcessLog* log, Process* process, int newState) {
    if (log && log->logger && process && process->bcp)
        loggerLogFormat(log->logger, LogLevelDebug, "Cambio de estado: %s -> %d", process->bcp->processId, newState);
}
void processLogFlush(ProcessLog* log) { if (log && log->logger) loggerFlush(log->logger); }
void processLogRecordTableSnapshot(ProcessLog* log, ProcessTable* table) {
    if (!log || !log->logger || !table) return;
    /* Linea 1: contadores de ciclo y procesos (10 variables) */
    loggerLogFormat(log->logger, LogLevelInfo,
        "TABLA[1/2]: ciclos=%d simStart=%d totalProc=%d finished=%d"
        " ctxSwitches=%d ctxSwitchTime=%d algoChanges=%d ioOps=%d cpuCyclesExec=%d"
        " pageFaults=%d itersSinceBalance=%d",
        table->currentCycle, table->simulationStartTime,
        table->totalProcesses, table->finishedProcesses,
        table->totalContextSwitches, table->totalContextSwitchTime, table->algorithmChangeCount,
        table->totalIoOperations, table->totalCpuCyclesExecuted,
        table->totalPageFaults, table->iterationsSinceBalance);
    /* Linea 2: metricas de rendimiento y memoria (10 variables) */
    loggerLogFormat(log->logger, LogLevelInfo,
        "TABLA[2/2]: cpuUtil=%.4f avgWait=%.2f avgTurn=%.2f"
        " frag=%.4f intWaste=%d extWaste=%d"
        " quantum=%d readyProp=%.4f waitProp=%.4f shouldSwitch=%d",
        table->cpuUtilization, table->avgWaitingTime, table->avgTurnaroundTime,
        table->fragmentation, table->internalWaste, table->externalWaste,
        table->quantumCurrent, table->proportionReady, table->proportionWaiting,
        table->shouldSwitchAlgorithm);
}

//Parte 3 – Presentación (UI)
