#include "distributedTasks.h"
#include "../core/bcp.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    char processId[idProcesoLen];
    int primary;
    int secondary;
} DistributedRankingEntry;

static int compareRankingEntries(const void* a, const void* b) {
    const DistributedRankingEntry* left = (const DistributedRankingEntry*)a;
    const DistributedRankingEntry* right = (const DistributedRankingEntry*)b;
    int diff = right->primary - left->primary;
    if (diff != 0) return diff;
    diff = right->secondary - left->secondary;
    return diff != 0 ? diff : strcmp(left->processId, right->processId);
}

void distributedTasksCalculateStats(const BcpSummary* rows, int count, DistributedStats* outStats) {
    if (!outStats) return;
    memset(outStats, 0, sizeof(*outStats));
    if (!rows || count <= 0) return;

    DistributedRankingEntry wasters[totalProcesos];
    int wasterCount = 0;
    outStats->processCount = count;

    for (int i = 0; i < count; i++) {
        outStats->totalAssignedCycles += rows[i].totalCpuCycles;
        outStats->totalExecutedCycles += rows[i].timeInExecution;
        outStats->totalIoOperations += rows[i].timesInIo;

        if (rows[i].state == ProcessStateFinished) {
            outStats->totalProcessesFinished++;
        } else {
            outStats->activeCount++;
            outStats->totalRemainingCycles += rows[i].remainingCycles;
            if (rows[i].state == ProcessStateWaitingIo || rows[i].state == ProcessStateReady)
                outStats->totalProcessesWaiting++;
        }

        strncpy(wasters[wasterCount].processId, rows[i].processId, idProcesoLen - 1);
        wasters[wasterCount].processId[idProcesoLen - 1] = '\0';
        wasters[wasterCount].primary = rows[i].wastedCpuCycles;
        wasters[wasterCount].secondary = rows[i].remainingCycles;
        wasterCount++;
    }

    outStats->avgRemainingCycles = outStats->activeCount > 0
        ? (int)(outStats->totalRemainingCycles / outStats->activeCount)
        : 0;
    outStats->avgCpuUtilization = outStats->totalAssignedCycles > 0
        ? (float)outStats->totalExecutedCycles / (float)outStats->totalAssignedCycles
        : 0.0f;

    qsort(wasters, wasterCount, sizeof(wasters[0]), compareRankingEntries);
    outStats->topWastersCount = wasterCount < totalRankingProcesos ? wasterCount : totalRankingProcesos;
    for (int i = 0; i < outStats->topWastersCount; i++) {
        strncpy(outStats->topWastersIds[i], wasters[i].processId, idProcesoLen - 1);
        outStats->topWastersIds[i][idProcesoLen - 1] = '\0';
        outStats->topWastersCpuWaste[i] = wasters[i].primary;
    }
}

void distributedTasksCalculateAging(const RrProcessData* rows, int count, AgingResults* outAging) {
    if (!outAging) return;
    memset(outAging, 0, sizeof(*outAging));
    if (!rows || count <= 0) return;

    DistributedRankingEntry aged[totalProcesos];
    DistributedRankingEntry wasters[totalProcesos];
    float totalUtil = 0.0f;

    for (int i = 0; i < count; i++) {
        strncpy(aged[i].processId, rows[i].processId, idProcesoLen - 1);
        aged[i].processId[idProcesoLen - 1] = '\0';
        aged[i].primary = rows[i].timesReturnedToReady;
        aged[i].secondary = rows[i].remainingCycles;

        strncpy(wasters[i].processId, rows[i].processId, idProcesoLen - 1);
        wasters[i].processId[idProcesoLen - 1] = '\0';
        wasters[i].primary = rows[i].wastedCpuCycles;
        wasters[i].secondary = rows[i].remainingCycles;

        outAging->totalReturnsToReady += rows[i].timesReturnedToReady;
        float utilization = 1.0f - rows[i].cpuWasteRatio;
        if (utilization < 0.0f) utilization = 0.0f;
        if (utilization > 1.0f) utilization = 1.0f;
        totalUtil += utilization;
    }

    qsort(aged, count, sizeof(aged[0]), compareRankingEntries);
    qsort(wasters, count, sizeof(wasters[0]), compareRankingEntries);

    outAging->topAgedCount = count < totalRankingProcesos ? count : totalRankingProcesos;
    for (int i = 0; i < outAging->topAgedCount; i++) {
        strncpy(outAging->topAgedIds[i], aged[i].processId, idProcesoLen - 1);
        outAging->topAgedIds[i][idProcesoLen - 1] = '\0';
        outAging->topAgedReturns[i] = aged[i].primary;
        outAging->topAgedRemainingCycles[i] = aged[i].secondary;
    }

    outAging->topWastersCount = count < totalRankingProcesos ? count : totalRankingProcesos;
    for (int i = 0; i < outAging->topWastersCount; i++) {
        strncpy(outAging->topWastersIds[i], wasters[i].processId, idProcesoLen - 1);
        outAging->topWastersIds[i][idProcesoLen - 1] = '\0';
        outAging->topWastersCpuWaste[i] = wasters[i].primary;
    }

    outAging->avgCpuUtilizationPerSlave = totalUtil / (float)count;
}
