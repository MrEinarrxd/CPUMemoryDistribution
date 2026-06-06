// === src/domain/scheduler/scheduler.h ===

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "../../utils/constants.h"

struct ProcessTable;
struct Process;
struct FcfsScheduler;
struct RrScheduler;

typedef enum {
    SchedulerAlgorithmFcfs,
    SchedulerAlgorithmRr
} SchedulerAlgorithm;

// Solo referencia, no destruye FcfsScheduler y RrScheduler
typedef struct Scheduler {
    SchedulerAlgorithm currentAlgorithm;
    struct FcfsScheduler* fcfsScheduler;
    struct RrScheduler* rrScheduler;
    int totalContextSwitches;
    int cycleCount;
} Scheduler;

Scheduler* schedulerCreate(SchedulerAlgorithm algorithm);

void schedulerDestroy(Scheduler* scheduler);

struct Process* schedulerSelectNext(Scheduler* scheduler, struct ProcessTable* table);

void schedulerOnContextSwitch(Scheduler* scheduler);

void schedulerSetAlgorithm(Scheduler* scheduler, SchedulerAlgorithm algorithm);

void schedulerSetFcfs(Scheduler* scheduler, struct FcfsScheduler* fcfs);
// Asigna el planificador FCFS al scheduler

void schedulerSetRr(Scheduler* scheduler, struct RrScheduler* rr);
// Asigna el planificador RR al scheduler

SchedulerAlgorithm schedulerGetAlgorithm(Scheduler* scheduler);

int schedulerGetTotalContextSwitches(Scheduler* scheduler);

#endif
