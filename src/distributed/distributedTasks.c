#include "distributedTasks.h"

#include <stdlib.h>
#include <string.h>

typedef struct RankingItem {
    char processId[ProcessIdLen];
    int primary;
    int secondary;
} RankingItem;

static int compareRankingItems(const void* left, const void* right) {
    const RankingItem* a = (const RankingItem*)left;
    const RankingItem* b = (const RankingItem*)right;
    int diff = b->primary - a->primary;
    if (diff != 0) return diff;
    diff = b->secondary - a->secondary;
    if (diff != 0) return diff;
    return strcmp(a->processId, b->processId);
}

static void copyTopPositive(const RankingItem items[], int count,
                            char ids[TopRankingCount][ProcessIdLen],
                            int values[TopRankingCount], int* outCount) {
    int copied = 0;
    for (int i = 0; i < count && copied < TopRankingCount; ++i) {
        if (items[i].primary <= 0) continue;
        strncpy(ids[copied], items[i].processId, ProcessIdLen - 1);
        ids[copied][ProcessIdLen - 1] = '\0';
        values[copied] = items[i].primary;
        copied++;
    }
    *outCount = copied;
}

void distributedTasksCalculateStats(const ProcessStatsRow rows[], int count, DistributedStatsResult* out) {
    RankingItem wasters[TotalProcesses];
    int wasterCount = 0;

    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!rows || count <= 0) return;
    out->processCount = count;

    for (int i = 0; i < count; ++i) {
        out->totalAssignedCycles += rows[i].totalCpuCycles;
        out->totalExecutedCycles += rows[i].executedCycles;
        out->ioOperations += rows[i].timesInIo;
        if (rows[i].state == 4) {
            out->finishedCount++;
        } else {
            out->activeCount++;
            out->totalRemainingCycles += rows[i].remainingCycles;
            if (rows[i].state == 3) out->waitingCount++;
        }

        strncpy(wasters[wasterCount].processId, rows[i].processId, ProcessIdLen - 1);
        wasters[wasterCount].processId[ProcessIdLen - 1] = '\0';
        wasters[wasterCount].primary = rows[i].wastedCpuCycles;
        wasters[wasterCount].secondary = rows[i].remainingCycles;
        wasterCount++;
    }

    out->avgRemainingCycles = out->activeCount > 0
        ? (int)(out->totalRemainingCycles / out->activeCount)
        : 0;
    out->avgCpuUtilization = out->totalAssignedCycles > 0
        ? (float)out->totalExecutedCycles / (float)out->totalAssignedCycles
        : 0.0f;

    qsort(wasters, wasterCount, sizeof(wasters[0]), compareRankingItems);
    copyTopPositive(wasters, wasterCount, out->topWastersIds,
                    out->topWastersWaste, &out->topWastersCount);
}

void distributedTasksCalculateAging(const RrAnalysisRow rows[], int count, DistributedAgingResult* out) {
    RankingItem aged[TotalProcesses];
    RankingItem wasters[TotalProcesses];
    long totalQuantumAssigned = 0;
    long totalQuantumUsed = 0;
    int agedCount = 0;
    int wasterCount = 0;

    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!rows || count <= 0) return;
    out->processCount = count;

    for (int i = 0; i < count; ++i) {
        totalQuantumAssigned += rows[i].rrQuantumAssignedTotal;
        totalQuantumUsed += rows[i].rrQuantumUsedTotal;
        out->totalReturnsToReady += rows[i].timesReturnedToReady;

        if (rows[i].timesReturnedToReady > 0) {
            strncpy(aged[agedCount].processId, rows[i].processId, ProcessIdLen - 1);
            aged[agedCount].processId[ProcessIdLen - 1] = '\0';
            aged[agedCount].primary = rows[i].timesReturnedToReady;
            aged[agedCount].secondary = rows[i].remainingCycles;
            agedCount++;
        }

        if (rows[i].wastedCpuCycles > 0) {
            strncpy(wasters[wasterCount].processId, rows[i].processId, ProcessIdLen - 1);
            wasters[wasterCount].processId[ProcessIdLen - 1] = '\0';
            wasters[wasterCount].primary = rows[i].wastedCpuCycles;
            wasters[wasterCount].secondary = rows[i].remainingCycles;
            wasterCount++;
        }
    }

    qsort(aged, agedCount, sizeof(aged[0]), compareRankingItems);
    qsort(wasters, wasterCount, sizeof(wasters[0]), compareRankingItems);

    out->topAgedCount = agedCount < TopRankingCount ? agedCount : TopRankingCount;
    for (int i = 0; i < out->topAgedCount; ++i) {
        strncpy(out->topAgedIds[i], aged[i].processId, ProcessIdLen - 1);
        out->topAgedIds[i][ProcessIdLen - 1] = '\0';
        out->topAgedReturns[i] = aged[i].primary;
        out->topAgedRemaining[i] = aged[i].secondary;
    }

    copyTopPositive(wasters, wasterCount, out->topWastersIds,
                    out->topWastersWaste, &out->topWastersCount);
    out->avgCpuUtilization = totalQuantumAssigned > 0
        ? (float)totalQuantumUsed / (float)totalQuantumAssigned
        : 0.0f;
}

void distributedTasksIntegrateStats(const DistributedStatsResult partials[], int count, DistributedStatsResult* out) {
    RankingItem wasters[TopRankingCount * 2];
    int wasterCount = 0;

    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!partials || count <= 0) return;
    for (int i = 0; i < count; ++i) {
        out->processCount += partials[i].processCount;
        out->activeCount += partials[i].activeCount;
        out->finishedCount += partials[i].finishedCount;
        out->waitingCount += partials[i].waitingCount;
        out->totalRemainingCycles += partials[i].totalRemainingCycles;
        out->totalAssignedCycles += partials[i].totalAssignedCycles;
        out->totalExecutedCycles += partials[i].totalExecutedCycles;
        out->ioOperations += partials[i].ioOperations;
        for (int j = 0; j < partials[i].topWastersCount && wasterCount < TopRankingCount * 2; ++j) {
            strncpy(wasters[wasterCount].processId, partials[i].topWastersIds[j], ProcessIdLen - 1);
            wasters[wasterCount].processId[ProcessIdLen - 1] = '\0';
            wasters[wasterCount].primary = partials[i].topWastersWaste[j];
            wasters[wasterCount].secondary = 0;
            wasterCount++;
        }
    }
    out->avgRemainingCycles = out->activeCount > 0
        ? (int)(out->totalRemainingCycles / out->activeCount)
        : 0;
    out->avgCpuUtilization = out->totalAssignedCycles > 0
        ? (float)out->totalExecutedCycles / (float)out->totalAssignedCycles
        : 0.0f;
    qsort(wasters, wasterCount, sizeof(wasters[0]), compareRankingItems);
    copyTopPositive(wasters, wasterCount, out->topWastersIds,
                    out->topWastersWaste, &out->topWastersCount);
}

void distributedTasksIntegrateAging(const DistributedAgingResult partials[], int count, DistributedAgingResult* out) {
    RankingItem aged[TopRankingCount * 2];
    RankingItem wasters[TopRankingCount * 2];
    int agedCount = 0;
    int wasterCount = 0;
    float weightedUtilSum = 0.0f;

    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!partials || count <= 0) return;
    for (int i = 0; i < count; ++i) {
        out->processCount += partials[i].processCount;
        out->totalReturnsToReady += partials[i].totalReturnsToReady;
        weightedUtilSum += partials[i].avgCpuUtilization * (float)partials[i].processCount;
        for (int j = 0; j < partials[i].topAgedCount && agedCount < TopRankingCount * 2; ++j) {
            strncpy(aged[agedCount].processId, partials[i].topAgedIds[j], ProcessIdLen - 1);
            aged[agedCount].processId[ProcessIdLen - 1] = '\0';
            aged[agedCount].primary = partials[i].topAgedReturns[j];
            aged[agedCount].secondary = partials[i].topAgedRemaining[j];
            agedCount++;
        }
        for (int j = 0; j < partials[i].topWastersCount && wasterCount < TopRankingCount * 2; ++j) {
            strncpy(wasters[wasterCount].processId, partials[i].topWastersIds[j], ProcessIdLen - 1);
            wasters[wasterCount].processId[ProcessIdLen - 1] = '\0';
            wasters[wasterCount].primary = partials[i].topWastersWaste[j];
            wasters[wasterCount].secondary = 0;
            wasterCount++;
        }
    }

    qsort(aged, agedCount, sizeof(aged[0]), compareRankingItems);
    qsort(wasters, wasterCount, sizeof(wasters[0]), compareRankingItems);
    out->topAgedCount = agedCount < TopRankingCount ? agedCount : TopRankingCount;
    for (int i = 0; i < out->topAgedCount; ++i) {
        strncpy(out->topAgedIds[i], aged[i].processId, ProcessIdLen - 1);
        out->topAgedIds[i][ProcessIdLen - 1] = '\0';
        out->topAgedReturns[i] = aged[i].primary;
        out->topAgedRemaining[i] = aged[i].secondary;
    }
    copyTopPositive(wasters, wasterCount, out->topWastersIds,
                    out->topWastersWaste, &out->topWastersCount);
    out->avgCpuUtilization = out->processCount > 0
        ? weightedUtilSum / (float)out->processCount
        : 0.0f;
}
