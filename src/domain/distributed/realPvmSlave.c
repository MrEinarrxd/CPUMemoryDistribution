#include "realPvmSlave.h"
#include "distributedTasks.h"
#include "../../utils/constants.h"
#include <pvm3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

static PvmMessage* pvmSlaveReceiveMessage(PvmSlave* slave, int timeoutSeconds) {
    if (!slave) return NULL;
    struct timeval timeout = { timeoutSeconds, 0 };
    int ret = pvm_trecv(-1, -1, &timeout);
    if (ret <= 0) return NULL;

    PvmMessage* msg = (PvmMessage*)malloc(sizeof(PvmMessage));
    if (!msg) return NULL;
    memset(msg, 0, sizeof(PvmMessage));
    pvm_upkbyte((char*)msg, sizeof(PvmMessage), 1);
    slave->messagesReceived++;
    return msg;
}

static int pvmSlaveSendMessage(PvmSlave* slave, PvmMessage* message) {
    if (!slave || !message) return -1;
    pvm_initsend(PvmDataDefault);
    pvm_pkbyte((char*)message, sizeof(PvmMessage), 1);
    int ret = pvm_send(slave->masterTid, message->messageType);
    if (ret >= 0) slave->messagesSent++;
    return ret;
}

PvmSlave* pvmSlaveInit(int masterTid, int maxProcesses) {
    PvmSlave* slave = (PvmSlave*)calloc(1, sizeof(PvmSlave));
    if (!slave) return NULL;
    slave->masterTid = masterTid;
    slave->slaveTid = pvm_mytid();
    slave->maxProcesses = maxProcesses;
    return slave;
}

int pvmSlaveConnect(PvmSlave* slave) {
    if (!slave) return -1;
    return slave->slaveTid >= 0 ? 0 : -1;
}

void pvmSlaveCleanup(PvmSlave* slave) {
    if (!slave) return;
    pvm_exit();
    free(slave);
}

static void pvmSlaveHandleStatsMessage(PvmSlave* slave, PvmMessage* incoming) {
    if (!slave || !incoming) return;
    if (incoming->payloadSize <= 0 ||
        incoming->payloadSize % (int)sizeof(BcpSummary) != 0) {
        fprintf(stderr, "[Slave] Payload de estadisticas invalido\n");
        return;
    }

    int count = incoming->payloadSize / (int)sizeof(BcpSummary);
    BcpSummary* rows = (BcpSummary*)malloc((size_t)incoming->payloadSize);
    if (!rows) return;
    memcpy(rows, incoming->payload, (size_t)incoming->payloadSize);

    DistributedStats stats;
    distributedTasksCalculateStats(rows, count, &stats);
    free(rows);

    PvmMessage* reply = packStatsMessage(0, slave->slaveTid, slave->masterTid, &stats);
    if (reply) {
        pvmSlaveSendMessage(slave, reply);
        pvmMessageDestroy(reply);
    }
}

static void pvmSlaveHandleAgingMessage(PvmSlave* slave, PvmMessage* incoming) {
    if (!slave || !incoming) return;
    if (incoming->payloadSize <= 0 ||
        incoming->payloadSize % (int)sizeof(RrProcessData) != 0) {
        fprintf(stderr, "[Slave] Payload de envejecimiento invalido\n");
        return;
    }

    int count = incoming->payloadSize / (int)sizeof(RrProcessData);
    RrProcessData* rows = (RrProcessData*)malloc((size_t)incoming->payloadSize);
    if (!rows) return;
    memcpy(rows, incoming->payload, (size_t)incoming->payloadSize);

    AgingResults aging;
    distributedTasksCalculateAging(rows, count, &aging);
    free(rows);

    PvmMessage* reply = packAgingMessage(0, slave->slaveTid, slave->masterTid, &aging);
    if (reply) {
        pvmSlaveSendMessage(slave, reply);
        pvmMessageDestroy(reply);
    }
}

void pvmSlaveRunTask1(PvmSlave* slave) {
    PvmMessage* incoming = pvmSlaveReceiveMessage(slave, 10);
    if (!incoming) {
        fprintf(stderr, "[Slave] Task1: no se recibio mensaje del master\n");
        return;
    }
    pvmSlaveHandleStatsMessage(slave, incoming);
    pvmMessageDestroy(incoming);
}

void pvmSlaveRunTask2(PvmSlave* slave) {
    PvmMessage* incoming = pvmSlaveReceiveMessage(slave, 10);
    if (!incoming) {
        fprintf(stderr, "[Slave] Task2: no se recibio mensaje del master\n");
        return;
    }
    pvmSlaveHandleAgingMessage(slave, incoming);
    pvmMessageDestroy(incoming);
}

int main(void) {
    int masterTid = pvm_parent();
    if (masterTid < 0) {
        fprintf(stderr, "[Slave] No se pudo obtener TID del master\n");
        pvm_exit();
        return 1;
    }

    PvmSlave* slave = pvmSlaveInit(masterTid, totalProcesos);
    if (!slave || pvmSlaveConnect(slave) != 0) {
        pvmSlaveCleanup(slave);
        return 1;
    }

    fprintf(stderr, "[Slave TID=%d] Listo, esperando tareas...\n", slave->slaveTid);

    for (int task = 0; task < 2; task++) {
        PvmMessage* incoming = pvmSlaveReceiveMessage(slave, 15);
        if (!incoming) {
            fprintf(stderr, "[Slave] Timeout esperando tarea %d\n", task + 1);
            break;
        }

        if (task == 0) {
            pvmSlaveHandleStatsMessage(slave, incoming);
        } else {
            pvmSlaveHandleAgingMessage(slave, incoming);
        }
        pvmMessageDestroy(incoming);
    }

    pvmSlaveCleanup(slave);
    return 0;
}
