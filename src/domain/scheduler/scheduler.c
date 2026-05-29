#include "scheduler.h"
#include "fcfsScheduler.h"
#include "rrScheduler.h"
#include "../process/processTable.h"
#include <stdlib.h>

Scheduler* schedulerCreate(SchedulerAlgorithm algorithm) {
    Scheduler* s = (Scheduler*)calloc(1, sizeof(Scheduler));
    if (s) s->currentAlgorithm = algorithm;
    return s;
}
void schedulerDestroy(Scheduler* scheduler) { free(scheduler); }

Process* schedulerSelectNext(Scheduler* scheduler, ProcessTable* table) {
    if (!scheduler || !table) return NULL;
    if (scheduler->currentAlgorithm == SchedulerAlgorithmFcfs && scheduler->fcfsScheduler)
        return fcfsSchedulerSelectNext(scheduler->fcfsScheduler, table);
    else if (scheduler->currentAlgorithm == SchedulerAlgorithmRr && scheduler->rrScheduler)
        return rrSchedulerSelectNext(scheduler->rrScheduler, table);
    return NULL;
}

void schedulerOnContextSwitch(Scheduler* scheduler) { if (scheduler) scheduler->totalContextSwitches++; }
void schedulerSetAlgorithm(Scheduler* scheduler, SchedulerAlgorithm algorithm) { if (scheduler) scheduler->currentAlgorithm = algorithm; }
void schedulerSetFcfs(Scheduler* scheduler, FcfsScheduler* fcfs) { if (scheduler) scheduler->fcfsScheduler = fcfs; }
void schedulerSetRr(Scheduler* scheduler, RrScheduler* rr) { if (scheduler) scheduler->rrScheduler = rr; }
SchedulerAlgorithm schedulerGetAlgorithm(Scheduler* scheduler) { return scheduler ? scheduler->currentAlgorithm : SchedulerAlgorithmFcfs; }
int schedulerGetTotalContextSwitches(Scheduler* scheduler) { return scheduler ? scheduler->totalContextSwitches : 0; }
