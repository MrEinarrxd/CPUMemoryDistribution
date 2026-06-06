// === src/domain/distributed/pvmSlave.h ===

#ifndef PVM_SLAVE_H
#define PVM_SLAVE_H

#include "../../utils/constants.h"
#include "messageProtocol.h"

struct Process;

// Propietario exclusivo de Process array
typedef struct PvmSlave {
    int slaveTid;
    int masterTid;
    struct Process** assignedProcesses;
    int processCount;
    int maxProcesses;
} PvmSlave;

PvmSlave* pvmSlaveInit(int masterTid, int maxProcesses);
// El slaveTid se obtiene internamente con pvm_mytid()

int pvmSlaveConnect(PvmSlave* slave);

PvmMessage* pvmSlaveReceiveData(PvmSlave* slave);

int pvmSlaveSendData(PvmSlave* slave, PvmMessage* message);

void pvmSlaveRunTask1(PvmSlave* slave);
// Tarea 1: calcula estadísticas parciales del subconjunto asignado

void pvmSlaveRunTask2(PvmSlave* slave);
// Tarea 2: analiza envejecimiento y desperdicio de CPU en RR

void pvmSlaveAssignProcess(PvmSlave* slave, struct Process* process);

int pvmSlaveProcessCount(PvmSlave* slave);

void pvmSlaveCleanup(PvmSlave* slave);

#endif
