#include "scheduler.h"

#include <stdlib.h>
#include <string.h>

static int compareRankingEntries(const void* left, const void* right) {
    const RankingEntry* a = (const RankingEntry*)left;
    const RankingEntry* b = (const RankingEntry*)right;
    int diff = b->primary - a->primary;
    if (diff != 0) return diff;
    diff = b->secondary - a->secondary;
    if (diff != 0) return diff;
    return strcmp(a->processId, b->processId);
}

void schedulerInit(Scheduler* scheduler, SchedulerAlgorithm algorithm, int quantum) {
    if (!scheduler) return;
    memset(scheduler, 0, sizeof(*scheduler));
    scheduler->algorithm = algorithm;
    scheduler->quantum = quantum > 0 ? quantum : DefaultQuantum;
    if (scheduler->quantum < MinQuantum) scheduler->quantum = MinQuantum;
    if (scheduler->quantum > MaxQuantum) scheduler->quantum = MaxQuantum;
    schedulerRecordQuantum(scheduler);
}

const char* schedulerAlgorithmName(SchedulerAlgorithm algorithm) {
    return algorithm == schedulerRr ? "Round Robin" : "FCFS";
}

int schedulerSelectNext(Scheduler* scheduler, ProcessTable* table) {
    int privileged;
    if (!scheduler || !table) return -1;
    if (scheduler->algorithm == schedulerRr && scheduler->hasPrivilegedProcess) {
        privileged = readyQueueExtractPrivileged(&table->readyQueue, table->processes);
        if (privileged >= 0) return privileged;
    }
    return readyQueuePop(&table->readyQueue);
}

void schedulerRecordQuantum(Scheduler* scheduler) {
    if (!scheduler) return;
    if (scheduler->historyCount < HistoryBars) {
        scheduler->quantumHistory[scheduler->historyCount++] = scheduler->quantum;
        return;
    }
    for (int i = 1; i < HistoryBars; ++i) {
        scheduler->quantumHistory[i - 1] = scheduler->quantumHistory[i];
    }
    scheduler->quantumHistory[HistoryBars - 1] = scheduler->quantum;
}

void schedulerRebalanceQuantum(Scheduler* scheduler, ProcessTable* table) {
    if (!scheduler || !table || scheduler->algorithm != schedulerRr) return;
    processTableUpdateQueueMetrics(table);
    if (table->waitingProportion >= QueueImbalanceThreshold) {
        scheduler->quantum += 2;
    } else if (table->readyProportion >= QueueImbalanceThreshold) {
        scheduler->quantum -= 2;
    }
    if (scheduler->quantum < MinQuantum) scheduler->quantum = MinQuantum;
    if (scheduler->quantum > MaxQuantum) scheduler->quantum = MaxQuantum;
    table->currentQuantum = scheduler->quantum;
    schedulerRecordQuantum(scheduler);
}

int schedulerAutoSwitchIfNeeded(Scheduler* scheduler, ProcessTable* table) {
    long returnsSum = 0;
    long wasteSum = 0;
    long remainingSum = 0;
    long ioSum = 0;
    long executedSum = 0;
    int activeCount = 0;
    int triggers = 0;

    if (!scheduler || !table) return 0;
    if (table->cpuIterations < 5000 ||
        table->cpuIterations % AutoSwitchInterval != 0 ||
        table->cpuIterations - scheduler->lastAutoSwitchIteration < AutoSwitchCooldownIterations) {
        return 0;
    }

    processTableUpdateAverages(table);
    processTableUpdateQueueMetrics(table);

    if (table->avgWaitingTime > 500.0f) triggers++;
    if (table->readyProportion >= QueueImbalanceThreshold ||
        table->waitingProportion >= QueueImbalanceThreshold) triggers++;
    if (table->totalPageFaults > table->cpuIterations / 2) triggers++;

    for (int i = 0; i < TotalProcesses; ++i) {
        Bcp* b = &table->processes[i];
        if (b->state == processStateFinished || b->state == processStateNew) continue;
        returnsSum += b->timesReturnedToReady;
        wasteSum += b->cpuWasteCycles;
        remainingSum += b->remainingCpuCycles;
        ioSum += b->timesInIo;
        executedSum += b->timeInExecution;
        activeCount++;
    }

    if (activeCount > 0) {
        if (returnsSum / activeCount > 5) triggers++;
        if (wasteSum / activeCount > 40) triggers++;
        if (remainingSum / activeCount > 50000) triggers++;
        if (ioSum / activeCount > 8) triggers++;
        if (executedSum / activeCount < 100 && table->cpuIterations > 100) triggers++;
    }

    if (triggers >= 6) {
        if (scheduler->algorithm == schedulerFcfs) {
            scheduler->algorithm = schedulerRr;
        } else {
            schedulerClearPrivilegedProcess(scheduler, table);
            scheduler->algorithm = schedulerFcfs;
        }
        scheduler->lastAutoSwitchIteration = table->cpuIterations;
        table->algorithmChanges++;
        return 1;
    }
    return 0;
}

void schedulerUpdateRankings(Scheduler* scheduler, ProcessTable* table) {
    RankingEntry aged[TotalProcesses];
    RankingEntry wasters[TotalProcesses];
    int agedCount = 0;
    int wasterCount = 0;

    if (!scheduler || !table) return;
    for (int i = 0; i < TotalProcesses; ++i) {
        Bcp* b = &table->processes[i];
        if (b->rrExecutionCount <= 0) continue;

        if (b->timesReturnedToReady > 0) {
            strncpy(aged[agedCount].processId, b->processId, ProcessIdLen - 1);
            aged[agedCount].processId[ProcessIdLen - 1] = '\0';
            aged[agedCount].primary = b->timesReturnedToReady;
            aged[agedCount].secondary = b->remainingCpuCycles;
            agedCount++;
        }

        if (b->cpuWasteCycles > 0) {
            strncpy(wasters[wasterCount].processId, b->processId, ProcessIdLen - 1);
            wasters[wasterCount].processId[ProcessIdLen - 1] = '\0';
            wasters[wasterCount].primary = b->cpuWasteCycles;
            wasters[wasterCount].secondary = b->quantumUsed;
            wasterCount++;
        }
    }

    qsort(aged, agedCount, sizeof(aged[0]), compareRankingEntries);
    qsort(wasters, wasterCount, sizeof(wasters[0]), compareRankingEntries);
    scheduler->topAgedCount = agedCount < TopRankingCount ? agedCount : TopRankingCount;
    scheduler->topWastersCount = wasterCount < TopRankingCount ? wasterCount : TopRankingCount;
    for (int i = 0; i < scheduler->topAgedCount; ++i) scheduler->topAged[i] = aged[i];
    for (int i = 0; i < scheduler->topWastersCount; ++i) scheduler->topWasters[i] = wasters[i];
}

void schedulerPrivilegeProcess(Scheduler* scheduler, ProcessTable* table, const char* processId) {
    int index;
    if (!scheduler || !table || !processId) return;
    for (int i = 0; i < TotalProcesses; ++i) {
        table->processes[i].privileged = 0;
    }
    index = processTableFindById(table, processId);
    if (index >= 0 && table->processes[index].state != processStateFinished) {
        table->processes[index].privileged = 1;
        scheduler->hasPrivilegedProcess = 1;
        strncpy(scheduler->privilegedProcessId, processId, ProcessIdLen - 1);
        scheduler->privilegedProcessId[ProcessIdLen - 1] = '\0';
    }
}

void schedulerClearPrivilegedProcess(Scheduler* scheduler, ProcessTable* table) {
    if (!scheduler || !table) return;
    for (int i = 0; i < TotalProcesses; ++i) {
        table->processes[i].privileged = 0;
    }
    scheduler->hasPrivilegedProcess = 0;
    scheduler->privilegedProcessId[0] = '\0';
}
