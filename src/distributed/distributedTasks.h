#ifndef CpuMemoryDistributedTasksH
#define CpuMemoryDistributedTasksH

#include "protocol.h"

void distributedTasksCalculateStats(const ProcessStatsRow rows[], int count, DistributedStatsResult* out);
void distributedTasksCalculateAging(const RrAnalysisRow rows[], int count, DistributedAgingResult* out);
void distributedTasksIntegrateStats(const DistributedStatsResult partials[], int count, DistributedStatsResult* out);
void distributedTasksIntegrateAging(const DistributedAgingResult partials[], int count, DistributedAgingResult* out);

#endif
