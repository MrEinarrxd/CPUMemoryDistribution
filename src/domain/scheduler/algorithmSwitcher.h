#ifndef ALGORITHM_SWITCHER_H
#define ALGORITHM_SWITCHER_H

#include "scheduler.h"

struct Scheduler;

typedef struct AlgorithmSwitcher {
    int algorithmChangeCount;
    int shouldSwitchAlgorithm;
    SchedulerAlgorithm nextAlgorithm;
    int switchThreshold;
    int tableThresholds[3];
    int pcbThresholds[5];
} AlgorithmSwitcher;

AlgorithmSwitcher* algorithmSwitcherCreate(int switchThreshold);
void algorithmSwitcherDestroy(AlgorithmSwitcher* switcher);
int algorithmSwitcherShouldSwitch(AlgorithmSwitcher* switcher, struct Scheduler* scheduler, struct ProcessTable* table);
void algorithmSwitcherSetNext(AlgorithmSwitcher* switcher, SchedulerAlgorithm nextAlgorithm);
SchedulerAlgorithm algorithmSwitcherGetNext(AlgorithmSwitcher* switcher);
void algorithmSwitcherApply(AlgorithmSwitcher* switcher, struct Scheduler* scheduler);
int algorithmSwitcherGetChangeCount(AlgorithmSwitcher* switcher);

#endif