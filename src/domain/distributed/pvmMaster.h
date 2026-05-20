// === src/domain/distributed/pvmMaster.h ===

#ifndef PVM_MASTER_H
#define PVM_MASTER_H

#include "../../utils/constants.h"
#include "messageProtocol.h"

struct ProcessTable;
struct RrScheduler;

// Propietario exclusivo de ProcessTable y RrScheduler
typedef struct PvmMaster {
    int masterTid;
    int slaveTids[pvmNumSlaves];
    struct ProcessTable* processTable;
    struct RrScheduler* rrScheduler;
    DistributedStats task1Results[pvmNumSlaves];
    AgingResults task2Results[pvmNumSlaves];
} PvmMaster;

PvmMaster* pvmMasterInit(void);

int pvmMasterSpawnSlaves(PvmMaster* master);

void pvmMasterTask1Stats(PvmMaster* master);

void pvmMasterTask2Aging(PvmMaster* master);

int pvmMasterSendData(PvmMaster* master, PvmMessage* message);

PvmMessage* pvmMasterReceiveData(PvmMaster* master);

void pvmMasterIntegrateResults(PvmMaster* master);

void pvmMasterPrintResults(PvmMaster* master);

void pvmMasterCleanup(PvmMaster* master);

#endif
