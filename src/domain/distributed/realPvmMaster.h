#ifndef REAL_PVM_MASTER_H
#define REAL_PVM_MASTER_H

#include "../../utils/constants.h"
#include "messageProtocol.h"

struct ProcessTable;
struct RrScheduler;

typedef struct PvmMaster {
    int masterTid;
    int slaveTids[pvmNumEsclavos];
    struct ProcessTable* processTable;
    struct RrScheduler* rrScheduler;
    DistributedStats task1Results[pvmNumEsclavos];
    AgingResults task2Results[pvmNumEsclavos];
} PvmMaster;

PvmMaster* pvmMasterInit(void);
int pvmMasterSpawnSlaves(PvmMaster* master);
void pvmMasterTask1Stats(PvmMaster* master);
void pvmMasterTask2Aging(PvmMaster* master);
void pvmMasterIntegrateResults(PvmMaster* master);
void pvmMasterPrintResults(PvmMaster* master);
void pvmMasterCleanup(PvmMaster* master);

#endif
