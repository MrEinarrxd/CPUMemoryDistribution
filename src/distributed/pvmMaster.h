#ifndef CpuMemoryPvmMasterH
#define CpuMemoryPvmMasterH

#include "../domain/process/processTable.h"
#include "protocol.h"

enum {
    PvmMasterWorkerCount = 2
};

typedef struct PvmMasterSession {
    int masterTid;
    int tids[PvmMasterWorkerCount];
    int workerCount;
    int active;
} PvmMasterSession;

void pvmMasterSessionInit(PvmMasterSession* session);
int pvmMasterSessionStart(PvmMasterSession* session);
int pvmMasterSessionAnalyze(PvmMasterSession* session, const ProcessTable* table,
                            DistributedReport* report);
void pvmMasterSessionStop(PvmMasterSession* session);
int pvmMasterRunReal(const ProcessTable* table, DistributedReport* report);

#endif
