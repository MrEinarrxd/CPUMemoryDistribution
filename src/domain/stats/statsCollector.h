// === src/domain/stats/statsCollector.h ===

#ifndef STATS_COLLECTOR_H
#define STATS_COLLECTOR_H

#include "../../utils/constants.h"

struct ProcessTable;

typedef struct StatsCollector {
    long totalCpuCyclesExecuted;
    float cpuUtilization;
    long totalMemoryAllocated;
    int internalWaste;
    int externalWaste;
    int totalPageFaults;
    float fragmentation;
} StatsCollector;

StatsCollector* statsCollectorCreate(void);

void statsCollectorDestroy(StatsCollector* collector);

void statsCollectorCollect(StatsCollector* collector, struct ProcessTable* table);

void statsCollectorRecordCpuCycle(StatsCollector* collector);

void statsCollectorRecordPageFault(StatsCollector* collector);

void statsCollectorRecordMemoryAllocation(StatsCollector* collector, int size);

void statsCollectorUpdateStatistics(StatsCollector* collector, struct ProcessTable* table);

float statsCollectorGetCpuUtilization(StatsCollector* collector);

#endif
