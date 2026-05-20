
#ifndef RR_SCHEDULER_H
#define RR_SCHEDULER_H

#include "../../utils/constants.h"

struct ProcessTable;
struct Process;

// Propietario exclusivo de ranking data
typedef struct AgingRanking {
    char processIds[totalRankingProcesos][idProcesoLen];
    int wasteValues[totalRankingProcesos];
    int count;
} AgingRanking;

typedef struct RrScheduler {
    int currentQuantum;
    int quantumHistory[maxAlgorithmHistory];
    int historyIndex;
    float proportionReady;
    float proportionWaiting;
    int iterationsSinceBalance;
    AgingRanking agingRanking;
} RrScheduler;

RrScheduler* rrSchedulerCreate(int quantum);

void rrSchedulerDestroy(RrScheduler* scheduler);

struct Process* rrSchedulerSelectNext(RrScheduler* scheduler, struct ProcessTable* table);

void rrSchedulerOnQuantumExpired(RrScheduler* scheduler);

void rrSchedulerUpdateProportions(RrScheduler* scheduler, struct ProcessTable* table);

void rrSchedulerUpdateAgingRanking(RrScheduler* scheduler, struct ProcessTable* table);

void rrSchedulerPrioritizeProcess(RrScheduler* scheduler, const char* processId);

int rrSchedulerGetCurrentQuantum(RrScheduler* scheduler);

#endif