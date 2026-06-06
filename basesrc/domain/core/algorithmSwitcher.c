#include "algorithmSwitcher.h"
#include "processTable.h"
#include "bcp.h"
#include <stdlib.h>

AlgorithmSwitcher* algorithmSwitcherCreate(int switchThreshold) {
    AlgorithmSwitcher* as = (AlgorithmSwitcher*)calloc(1, sizeof(AlgorithmSwitcher));
    if (as) {
        as->switchThreshold = switchThreshold;
        as->shouldSwitchAlgorithm = 0;
        as->nextAlgorithm = SchedulerAlgorithmFcfs;
        as->tableThresholds[0] = 500;
        as->tableThresholds[1] = 80;
        as->tableThresholds[2] = 100;
        as->pcbThresholds[0] = 5;
        as->pcbThresholds[1] = 40;
        as->pcbThresholds[2] = 10;
    }
    return as;
}
void algorithmSwitcherDestroy(AlgorithmSwitcher* switcher) { free(switcher); }

int algorithmSwitcherShouldSwitch(AlgorithmSwitcher* switcher, Scheduler* scheduler, ProcessTable* table) {
    if (!switcher || !scheduler || !table) return 0;

    const int waitThreshold = switcher->tableThresholds[0];
    const float cpuIdleThreshold = 0.30f;
    const int ioThreshold = switcher->tableThresholds[2];
    const int returnsThreshold = switcher->pcbThresholds[0];
    const int avgIoThreshold = switcher->pcbThresholds[2];
    const int wasteThreshold = 40;
    const int remainingThreshold = 50000;
    const int executionThreshold = 100;

    int triggered = 0;
    if (table->avgWaitingTime > waitThreshold) triggered++;
    if (table->currentCycle > 20 && table->cpuUtilization < cpuIdleThreshold) triggered++;
    if (table->totalIoOperations > ioThreshold) triggered++;

    long totalReturns = 0;
    long totalWaste = 0;
    long totalRemaining = 0;
    long totalExecution = 0;
    long totalIo = 0;
    int activeCount = 0;
    for (int i = 0; i < procesosEnEjecucion; i++) {
        Process* p = table->runningProcesses[i];
        if (p && p->bcp && p->bcp->state != ProcessStateFinished) {
            totalReturns += p->bcp->timesReturnedToReady;
            totalWaste += p->bcp->wastedCpuCycles;
            totalRemaining += p->bcp->remainingCycles;
            totalExecution += p->bcp->timeInExecution;
            totalIo += p->bcp->timesInIo;
            activeCount++;
        }
    }
    for (int i = 0; i < procesosEnEspera; i++) {
        Process* p = table->newRequests[i];
        if (p && p->bcp && p->bcp->state != ProcessStateFinished) {
            totalReturns += p->bcp->timesReturnedToReady;
            totalWaste += p->bcp->wastedCpuCycles;
            totalRemaining += p->bcp->remainingCycles;
            totalExecution += p->bcp->timeInExecution;
            totalIo += p->bcp->timesInIo;
            activeCount++;
        }
    }

    if (activeCount > 0) {
        int avgReturns = (int)(totalReturns / activeCount);
        int avgWaste = (int)(totalWaste / activeCount);
        int avgRemaining = (int)(totalRemaining / activeCount);
        int avgExecution = (int)(totalExecution / activeCount);
        int avgIo = (int)(totalIo / activeCount);

        if (avgReturns > returnsThreshold) triggered++;
        if (avgWaste > wasteThreshold) triggered++;
        if (avgRemaining > remainingThreshold) triggered++;
        if (avgExecution < executionThreshold && table->currentCycle > 50) triggered++;
        if (avgIo > avgIoThreshold) triggered++;
    }

    if (triggered >= 3) {
        SchedulerAlgorithm current = schedulerGetAlgorithm(scheduler);
        if (current == SchedulerAlgorithmFcfs)
            switcher->nextAlgorithm = SchedulerAlgorithmRr;
        else
            switcher->nextAlgorithm = SchedulerAlgorithmFcfs;
        return 1;
    }
    return 0;
}

void algorithmSwitcherSetNext(AlgorithmSwitcher* switcher, SchedulerAlgorithm nextAlgorithm) {
    if (switcher) switcher->nextAlgorithm = nextAlgorithm;
}
SchedulerAlgorithm algorithmSwitcherGetNext(AlgorithmSwitcher* switcher) {
    return switcher ? switcher->nextAlgorithm : SchedulerAlgorithmFcfs;
}
void algorithmSwitcherApply(AlgorithmSwitcher* switcher, Scheduler* scheduler) {
    if (!switcher || !scheduler) return;
    schedulerSetAlgorithm(scheduler, switcher->nextAlgorithm);
    switcher->algorithmChangeCount++;
}
int algorithmSwitcherGetChangeCount(AlgorithmSwitcher* switcher) { return switcher ? switcher->algorithmChangeCount : 0; }
