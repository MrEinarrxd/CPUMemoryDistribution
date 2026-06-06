#ifndef CpuMemoryLocalRunnerH
#define CpuMemoryLocalRunnerH

#include "../domain/process/processTable.h"
#include "protocol.h"

void localRunnerRun(const ProcessTable* table, DistributedReport* report);

#endif
