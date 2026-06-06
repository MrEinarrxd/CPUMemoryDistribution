#ifndef CpuMemoryPvmMasterH
#define CpuMemoryPvmMasterH

#include "../domain/process/processTable.h"
#include "protocol.h"

int pvmMasterRunReal(const ProcessTable* table, DistributedReport* report);

#endif
