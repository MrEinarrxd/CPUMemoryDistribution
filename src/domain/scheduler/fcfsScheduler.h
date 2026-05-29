#ifndef FCFS_SCHEDULER_H
#define FCFS_SCHEDULER_H

struct ProcessTable;
struct Process;

typedef struct FcfsScheduler {
    int totalScheduled;
    int contextSwitchCount;
} FcfsScheduler;

FcfsScheduler* fcfsSchedulerCreate(void);
void fcfsSchedulerDestroy(FcfsScheduler* scheduler);
struct Process* fcfsSchedulerSelectNext(FcfsScheduler* scheduler, struct ProcessTable* table);
void fcfsSchedulerOnContextSwitch(FcfsScheduler* scheduler);

#endif
