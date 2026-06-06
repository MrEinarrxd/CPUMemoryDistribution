#include "fakePvmMaster.h"
#include "distributedTasks.h"
#include "../core/process.h"
#include "../core/processTable.h"
#include "../../presentation/consoleIo.h"
#include "../../utils/constants.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct RankingEntry {
    char processId[idProcesoLen];
    int primary;
    int secondary;
} RankingEntry;

static int compareRankingEntries(const void* a, const void* b) {
    const RankingEntry* left = (const RankingEntry*)a;
    const RankingEntry* right = (const RankingEntry*)b;
    int diff = right->primary - left->primary;
    if (diff != 0) return diff;
    diff = right->secondary - left->secondary;
    return diff != 0 ? diff : strcmp(left->processId, right->processId);
}

static int collectProcesses(ProcessTable* table, Process** outProcesses, int maxSize) {
    int count = 0;
    if (!table || !outProcesses) return 0;
    for (int i = 0; i < procesosEnEjecucion && count < maxSize; i++) {
        if (table->runningProcesses[i] && table->runningProcesses[i]->bcp)
            outProcesses[count++] = table->runningProcesses[i];
    }
    for (int i = 0; i < procesosEnEspera && count < maxSize; i++) {
        if (table->newRequests[i] && table->newRequests[i]->bcp)
            outProcesses[count++] = table->newRequests[i];
    }
    return count;
}

static DistributedStats calculateStats(Process** processes, int start, int count) {
    DistributedStats stats;
    BcpSummary rows[totalProcesos];
    memset(rows, 0, sizeof(rows));
    for (int i = start; i < start + count; i++) {
        Bcp* bcp = processes[i]->bcp;
        int row = i - start;
        rows[row].pid = bcp->pid;
        strncpy(rows[row].processId, bcp->processId, idProcesoLen - 1);
        rows[row].state = (int)bcp->state;
        rows[row].remainingCycles = bcp->remainingCycles;
        rows[row].totalCpuCycles = bcp->totalCpuCycles;
        rows[row].timeInExecution = bcp->timeInExecution;
        rows[row].timesInIo = bcp->timesInIo;
        rows[row].wastedCpuCycles = bcp->wastedCpuCycles;
    }
    distributedTasksCalculateStats(rows, count, &stats);
    return stats;
}

static AgingResults calculateAging(Process** processes, int start, int count) {
    AgingResults aging;
    RrProcessData rows[totalProcesos];
    memset(rows, 0, sizeof(rows));
    for (int i = start; i < start + count; i++) {
        Bcp* bcp = processes[i]->bcp;
        int row = i - start;
        rows[row].pid = bcp->pid;
        strncpy(rows[row].processId, bcp->processId, idProcesoLen - 1);
        rows[row].remainingCycles = bcp->remainingCycles;
        rows[row].totalCpuCycles = bcp->totalCpuCycles;
        rows[row].timeInExecution = bcp->timeInExecution;
        rows[row].quantumAssigned = bcp->quantumAssigned;
        rows[row].quantumUsed = bcp->quantumUsed;
        rows[row].timesReturnedToReady = bcp->timesReturnedToReady;
        rows[row].wastedCpuCycles = bcp->wastedCpuCycles;
        rows[row].cpuWasteRatio = bcp->cpuWasteRatio;
    }
    distributedTasksCalculateAging(rows, count, &aging);
    return aging;
}

FakePvmMaster* fakePvmMasterCreate(void) {
    return (FakePvmMaster*)calloc(1, sizeof(FakePvmMaster));
}

int fakePvmMasterRunStatsTask(FakePvmMaster* master, ProcessTable* table) {
    if (!master || !table) return -1;
    Process* snapshot[totalProcesos];
    int total = collectProcesses(table, snapshot, totalProcesos);
    int base = total / pvmNumEsclavos;
    int rem = total % pvmNumEsclavos;
    int start = 0;
    memset(master->task1Results, 0, sizeof(master->task1Results));
    for (int i = 0; i < pvmNumEsclavos; i++) {
        int count = base + (i < rem ? 1 : 0);
        master->task1Results[i] = calculateStats(snapshot, start, count);
        start += count;
    }
    return 0;
}

int fakePvmMasterRunAgingTask(FakePvmMaster* master, ProcessTable* table) {
    if (!master || !table) return -1;
    Process* snapshot[totalProcesos];
    int total = collectProcesses(table, snapshot, totalProcesos);
    int base = total / pvmNumEsclavos;
    int rem = total % pvmNumEsclavos;
    int start = 0;
    memset(master->task2Results, 0, sizeof(master->task2Results));
    for (int i = 0; i < pvmNumEsclavos; i++) {
        int count = base + (i < rem ? 1 : 0);
        master->task2Results[i] = calculateAging(snapshot, start, count);
        start += count;
    }
    return 0;
}

void fakePvmMasterIntegrateResults(FakePvmMaster* master) {
    if (!master) return;
    DistributedStats stats = {0};
    AgingResults aging = {0};
    RankingEntry wasters[pvmNumEsclavos * totalRankingProcesos];
    RankingEntry aged[pvmNumEsclavos * totalRankingProcesos];
    int wasterCount = 0;
    int agedCount = 0;

    for (int i = 0; i < pvmNumEsclavos; i++) {
        stats.processCount += master->task1Results[i].processCount;
        stats.activeCount += master->task1Results[i].activeCount;
        stats.totalProcessesFinished += master->task1Results[i].totalProcessesFinished;
        stats.totalProcessesWaiting += master->task1Results[i].totalProcessesWaiting;
        stats.totalRemainingCycles += master->task1Results[i].totalRemainingCycles;
        stats.totalAssignedCycles += master->task1Results[i].totalAssignedCycles;
        stats.totalExecutedCycles += master->task1Results[i].totalExecutedCycles;
        stats.totalIoOperations += master->task1Results[i].totalIoOperations;
        for (int j = 0; j < master->task1Results[i].topWastersCount; j++) {
            strncpy(wasters[wasterCount].processId, master->task1Results[i].topWastersIds[j], idProcesoLen - 1);
            wasters[wasterCount].primary = master->task1Results[i].topWastersCpuWaste[j];
            wasters[wasterCount].secondary = 0;
            wasterCount++;
        }
        aging.totalReturnsToReady += master->task2Results[i].totalReturnsToReady;
        aging.avgCpuUtilizationPerSlave += master->task2Results[i].avgCpuUtilizationPerSlave;
        for (int j = 0; j < master->task2Results[i].topAgedCount; j++) {
            strncpy(aged[agedCount].processId, master->task2Results[i].topAgedIds[j], idProcesoLen - 1);
            aged[agedCount].primary = master->task2Results[i].topAgedReturns[j];
            aged[agedCount].secondary = master->task2Results[i].topAgedRemainingCycles[j];
            agedCount++;
        }
    }

    stats.avgRemainingCycles = stats.activeCount > 0 ? (int)(stats.totalRemainingCycles / stats.activeCount) : 0;
    stats.avgCpuUtilization = stats.totalAssignedCycles > 0
        ? (float)stats.totalExecutedCycles / (float)stats.totalAssignedCycles : 0.0f;
    qsort(wasters, wasterCount, sizeof(RankingEntry), compareRankingEntries);
    stats.topWastersCount = wasterCount < totalRankingProcesos ? wasterCount : totalRankingProcesos;
    for (int i = 0; i < stats.topWastersCount; i++) {
        strncpy(stats.topWastersIds[i], wasters[i].processId, idProcesoLen - 1);
        stats.topWastersCpuWaste[i] = wasters[i].primary;
    }

    qsort(aged, agedCount, sizeof(RankingEntry), compareRankingEntries);
    aging.avgCpuUtilizationPerSlave /= (float)pvmNumEsclavos;
    aging.topAgedCount = agedCount < totalRankingProcesos ? agedCount : totalRankingProcesos;
    for (int i = 0; i < aging.topAgedCount; i++) {
        strncpy(aging.topAgedIds[i], aged[i].processId, idProcesoLen - 1);
        aging.topAgedReturns[i] = aged[i].primary;
        aging.topAgedRemainingCycles[i] = aged[i].secondary;
    }
    aging.topWastersCount = stats.topWastersCount;
    for (int i = 0; i < aging.topWastersCount; i++) {
        strncpy(aging.topWastersIds[i], stats.topWastersIds[i], idProcesoLen - 1);
        aging.topWastersCpuWaste[i] = stats.topWastersCpuWaste[i];
    }

    master->task1Results[0] = stats;
    master->task2Results[0] = aging;
}

void fakePvmMasterPrintResults(FakePvmMaster* master) {
    if (!master) return;
    consoleIoPrintSeparator();
    consoleIoPrintLine("=== RESULTADOS PVM SIMULADO: TAREA 1 ===");
    consoleIoPrintInt("Procesos analizados: ", master->task1Results[0].processCount);
    consoleIoPrintInt("Procesos finalizados: ", master->task1Results[0].totalProcessesFinished);
    consoleIoPrintInt("Procesos en espera/E/S: ", master->task1Results[0].totalProcessesWaiting);
    consoleIoPrintInt("Promedio ciclos pendientes: ", master->task1Results[0].avgRemainingCycles);
    consoleIoPrintFloat("Utilización CPU promedio: ", master->task1Results[0].avgCpuUtilization);
    consoleIoPrintLine("=== RESULTADOS PVM SIMULADO: TAREA 2 ===");
    consoleIoPrintLine("Top procesos envejecidos/perjudicados:");
    for (int i = 0; i < master->task2Results[0].topAgedCount; i++) {
        char line[256];
        snprintf(line, sizeof(line), "%d. %s (retornos: %d, pendientes: %d)", i + 1,
                 master->task2Results[0].topAgedIds[i],
                 master->task2Results[0].topAgedReturns[i],
                 master->task2Results[0].topAgedRemainingCycles[i]);
        consoleIoPrintLine(line);
    }
    consoleIoPrintSeparator();
}

void fakePvmMasterDestroy(FakePvmMaster* master) {
    free(master);
}
