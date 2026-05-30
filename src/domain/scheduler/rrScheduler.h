#ifndef RR_SCHEDULER_H
#define RR_SCHEDULER_H

#include "../../utils/constants.h"

struct ProcessTable;
struct Process;

typedef struct AgingRanking {
    char processIds[totalRankingProcesos][idProcesoLen];
    int wasteValues[totalRankingProcesos];
    char wasterProcessIds[totalRankingProcesos][idProcesoLen];
    int wasterWasteValues[totalRankingProcesos];
    int count;
    int wasterCount;
} AgingRanking;

typedef struct RrScheduler {
    int currentQuantum;
    int quantumHistory[historialAlgoritmoMaximo];
    int historyIndex;
    float proportionReady;
    float proportionWaiting;
    int iterationsSinceBalance;
    AgingRanking agingRanking;
    char privilegedProcessId[idProcesoLen];
    int hasPrivilegedProcess;
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
