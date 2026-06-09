#ifndef CpuMemorySchedulerH
#define CpuMemorySchedulerH

#include "../process/processTable.h"

typedef enum SchedulerAlgorithm {
    schedulerFcfs = 0,
    schedulerRr = 1
} SchedulerAlgorithm;

typedef struct RankingEntry {
    char processId[ProcessIdLen];
    int primary;
    int secondary;
} RankingEntry;

typedef struct Scheduler {
    SchedulerAlgorithm algorithm;
    int quantum;
    int quantumHistory[HistoryBars];
    int historyCount;
    int iterationsSinceRebalance;
    RankingEntry topAged[TopRankingCount];
    RankingEntry topWasters[TopRankingCount];
    int topAgedCount;
    int topWastersCount;
    int hasPrivilegedProcess;
    int manualAlgorithmOverride;
    int lastAutoSwitchIteration;
    char privilegedProcessId[ProcessIdLen];
} Scheduler;

void schedulerInit(Scheduler* scheduler, SchedulerAlgorithm algorithm, int quantum);
const char* schedulerAlgorithmName(SchedulerAlgorithm algorithm);
int schedulerSelectNext(Scheduler* scheduler, ProcessTable* table);
void schedulerRecordQuantum(Scheduler* scheduler);
void schedulerRebalanceQuantum(Scheduler* scheduler, ProcessTable* table);
int schedulerAutoSwitchIfNeeded(Scheduler* scheduler, ProcessTable* table);
void schedulerUpdateRankings(Scheduler* scheduler, ProcessTable* table);
void schedulerPrivilegeProcess(Scheduler* scheduler, ProcessTable* table, const char* processId);
void schedulerClearPrivilegedProcess(Scheduler* scheduler, ProcessTable* table);

#endif
