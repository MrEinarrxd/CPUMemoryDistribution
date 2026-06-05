#ifndef DISTRIBUTED_TASKS_H
#define DISTRIBUTED_TASKS_H

#include "messageProtocol.h"

void distributedTasksCalculateStats(const BcpSummary* rows, int count, DistributedStats* outStats);
void distributedTasksCalculateAging(const RrProcessData* rows, int count, AgingResults* outAging);

#endif
