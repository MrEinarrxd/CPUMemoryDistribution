#ifndef DISTRIBUTED_TASKS_H
#define DISTRIBUTED_TASKS_H

#include "protocol.h"

void distributed_calculate_stats(const BcpSummary* rows, int count, DistributedStats* out_stats);
void distributed_calculate_aging(const RrProcessData* rows, int count, AgingResults* out_aging);
void distributed_integrate_stats(const DistributedStats* partials, int count, DistributedStats* out_stats);
void distributed_integrate_aging(const AgingResults* partials, int count, AgingResults* out_aging);

#endif
