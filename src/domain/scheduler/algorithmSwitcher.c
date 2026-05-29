#include "algorithmSwitcher.h"
#include "../process/processTable.h"
#include "../process/bcp.h"
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

    // Umbrales (ajustables)
    const int WAIT_THRESHOLD = 500;
    const float CPU_IDLE_THRESHOLD = 0.30f;
    const int IO_THRESHOLD = 100;
    const float WASTE_RATIO_THRESHOLD = 0.40f;
    const int RETURNS_THRESHOLD = 10;

    int triggered = 0;
    if (table->avgWaitingTime > WAIT_THRESHOLD) triggered++;
    if (table->cpuUtilization < CPU_IDLE_THRESHOLD) triggered++;
    if (table->totalIoOperations > IO_THRESHOLD) triggered++;

    // Promedio de veces que procesos retornaron a Ready
    int totalReturns = 0, activeCount = 0;
    for (int i = 0; i < procesosEnEjecucion; i++) {
        Process* p = table->runningProcesses[i];
        if (p && p->bcp && p->bcp->state != ProcessStateFinished) {
            totalReturns += p->bcp->timesReturnedToReady;
            activeCount++;
        }
    }
    for (int i = 0; i < procesosEnEspera; i++) {
        Process* p = table->newRequests[i];
        if (p && p->bcp && p->bcp->state != ProcessStateFinished) {
            totalReturns += p->bcp->timesReturnedToReady;
            activeCount++;
        }
    }
    int avgReturns = (activeCount > 0) ? totalReturns / activeCount : 0;
    if (avgReturns > RETURNS_THRESHOLD) triggered++;

    // Promedio de desperdicio (waste / quantumAsignado)
    float totalWasteRatio = 0.0f;
    activeCount = 0;
    for (int i = 0; i < procesosEnEjecucion; i++) {
        Process* p = table->runningProcesses[i];
        if (p && p->bcp && p->bcp->quantumAssigned > 0) {
            totalWasteRatio += (float)p->bcp->wastedCpuCycles / p->bcp->quantumAssigned;
            activeCount++;
        }
    }
    for (int i = 0; i < procesosEnEspera; i++) {
        Process* p = table->newRequests[i];
        if (p && p->bcp && p->bcp->quantumAssigned > 0) {
            totalWasteRatio += (float)p->bcp->wastedCpuCycles / p->bcp->quantumAssigned;
            activeCount++;
        }
    }
    float avgWaste = (activeCount > 0) ? totalWasteRatio / activeCount : 0.0f;
    if (avgWaste > WASTE_RATIO_THRESHOLD) triggered++;

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