#ifndef REAL_PVM_SLAVE_H
#define REAL_PVM_SLAVE_H

#include "../../utils/constants.h"
#include "messageProtocol.h"

struct Process;

typedef struct PvmSlave {
    int slaveTid;
    int masterTid;
    struct Process** assignedProcesses;
    int processCount;
    int maxProcesses;
    int messagesReceived;
    int messagesSent;
} PvmSlave;

PvmSlave* pvmSlaveInit(int masterTid, int maxProcesses);
int pvmSlaveConnect(PvmSlave* slave);
void pvmSlaveRunTask1(PvmSlave* slave);
void pvmSlaveRunTask2(PvmSlave* slave);
void pvmSlaveCleanup(PvmSlave* slave);

#endif
