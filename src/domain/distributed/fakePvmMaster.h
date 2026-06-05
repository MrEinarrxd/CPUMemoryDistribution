#ifndef FAKE_PVM_MASTER_H
#define FAKE_PVM_MASTER_H

#include "messageProtocol.h"

struct ProcessTable;

typedef struct FakePvmMaster {
    DistributedStats task1Results[pvmNumEsclavos];
    AgingResults task2Results[pvmNumEsclavos];
} FakePvmMaster;

FakePvmMaster* fakePvmMasterCreate(void);
int fakePvmMasterRunStatsTask(FakePvmMaster* master, struct ProcessTable* table);
int fakePvmMasterRunAgingTask(FakePvmMaster* master, struct ProcessTable* table);
void fakePvmMasterIntegrateResults(FakePvmMaster* master);
void fakePvmMasterPrintResults(FakePvmMaster* master);
void fakePvmMasterDestroy(FakePvmMaster* master);

#endif
