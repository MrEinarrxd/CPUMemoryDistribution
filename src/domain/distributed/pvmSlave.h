#ifndef PVM_SLAVE_H
#define PVM_SLAVE_H

#include "../../utils/constants.h"
#include "messageProtocol.h"

struct Process;

typedef struct PvmSlave {
    int slaveTid;
    int masterTid;
    struct Process** assignedProcesses;
    int processCount;
    int maxProcesses;
} PvmSlave;

PvmSlave* pvmSlaveInit(int masterTid, int maxProcesses);
int pvmSlaveConnect(PvmSlave* slave);
PvmMessage* pvmSlaveReceiveData(PvmSlave* slave);
int pvmSlaveSendData(PvmSlave* slave, PvmMessage* message);
void pvmSlaveRunTask1(PvmSlave* slave);
void pvmSlaveRunTask2(PvmSlave* slave);
void pvmSlaveAssignProcess(PvmSlave* slave, struct Process* process);
int pvmSlaveProcessCount(PvmSlave* slave);
void pvmSlaveCleanup(PvmSlave* slave);

#endif
