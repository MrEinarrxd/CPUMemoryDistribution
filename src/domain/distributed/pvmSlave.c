// src/domain/distributed/pvmSlave.c
// Implementación del proceso esclavo PVM.
// Compila como ejecutable independiente: gcc pvmSlave.c -o pvmSlave -lpvm3

#include "pvmSlave.h"
#include "messageProtocol.h"
#include "../../utils/errorHandler.h"
#include "../../utils/constants.h"
#include <pvm3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ── Tipos locales ────────────────────────────────────────────
typedef struct {
    int pid;
    char processId[idProcesoLen];
    int state;
    int remainingCycles;
    int totalCpuCycles;
    int timesInIo;
    int wastedCpuCycles;
} BcpSummary;

typedef struct {
    int pid;
    char processId[idProcesoLen];
    int quantumAssigned;
    int quantumUsed;
    int timesReturnedToReady;
    int wastedCpuCycles;
    float cpuWasteRatio;
} RrProcessData;

// ── Ciclo de vida ─────────────────────────────────────────────
PvmSlave* pvmSlaveInit(int masterTid, int maxProcesses) {
    PvmSlave* slave = (PvmSlave*)calloc(1, sizeof(PvmSlave));
    if (!slave) return NULL;
    slave->masterTid  = masterTid;
    slave->slaveTid   = pvm_mytid();
    slave->maxProcesses = maxProcesses;
    return slave;
}

int pvmSlaveConnect(PvmSlave* slave) {
    if (!slave) return -1;
    return (slave->slaveTid >= 0) ? 0 : -1;
}

PvmMessage* pvmSlaveReceiveData(PvmSlave* slave) {
    if (!slave) return NULL;
    struct timeval timeout = {10, 0};
    int ret = pvm_trecv(-1, -1, &timeout);
    if (ret <= 0) return NULL;
    PvmMessage* msg = (PvmMessage*)malloc(sizeof(PvmMessage));
    if (!msg) return NULL;
    pvm_upkbyte((char*)msg, sizeof(PvmMessage), 1);
    slave->messagesReceived++;
    return msg;
}

int pvmSlaveSendData(PvmSlave* slave, PvmMessage* message) {
    if (!slave || !message) return -1;
    pvm_initsend(PvmDataDefault);
    pvm_pkbyte((char*)message, sizeof(PvmMessage), 1);
    int ret = pvm_send(slave->masterTid, message->messageType);
    if (ret >= 0) slave->messagesSent++;
    return ret;
}

void pvmSlaveAssignProcess(PvmSlave* slave, struct Process* process) { (void)slave; (void)process; }
int  pvmSlaveProcessCount(PvmSlave* slave) { return slave ? slave->processCount : 0; }

void pvmSlaveCleanup(PvmSlave* slave) {
    if (slave) { pvm_exit(); free(slave); }
}

// ── Tarea 1: estadísticas parciales ──────────────────────────
void pvmSlaveRunTask1(PvmSlave* slave) {
    if (!slave) return;

    PvmMessage* incoming = pvmSlaveReceiveData(slave);
    if (!incoming) {
        fprintf(stderr, "[Slave] Task1: no se recibio mensaje del master\n");
        return;
    }

    int count = incoming->payloadSize / (int)sizeof(BcpSummary);
    BcpSummary* summaries = (BcpSummary*)malloc(incoming->payloadSize);
    if (!summaries) { pvmMessageDestroy(incoming); return; }
    memcpy(summaries, incoming->payload, incoming->payloadSize);
    pvmMessageDestroy(incoming);

    DistributedStats stats = {0};
    long totalRemaining = 0;

    for (int i = 0; i < count; i++) {
        // state: 6 = ProcessStateFinished
        if (summaries[i].state == 6) {
            stats.totalProcessesFinished++;
        } else {
            // state: 3 = ProcessStateWaitingIo
            if (summaries[i].state == 3) stats.totalProcessesWaiting++;
            if (summaries[i].timesInIo > 0) stats.totalIoOperations++;
            totalRemaining += summaries[i].remainingCycles;
        }
    }

    int active = count - stats.totalProcessesFinished;
    stats.avgRemainingCycles = (active > 0) ? (int)(totalRemaining / active) : 0;

    // Aproximación de utilización: fracción de ciclos ejecutados
    long totalAssigned = 0;
    for (int i = 0; i < count; i++)
        totalAssigned += summaries[i].totalCpuCycles;
    long totalDone = totalAssigned - totalRemaining;
    stats.avgCpuUtilization = (totalAssigned > 0)
        ? (float)totalDone / (float)totalAssigned : 0.0f;

    free(summaries);

    PvmMessage* reply = packStatsMessage(0, slave->slaveTid, slave->masterTid, &stats);
    if (reply) {
        pvmSlaveSendData(slave, reply);
        pvmMessageDestroy(reply);
    }
}

// ── Tarea 2: envejecimiento y desperdicio ─────────────────────
void pvmSlaveRunTask2(PvmSlave* slave) {
    if (!slave) return;

    PvmMessage* incoming = pvmSlaveReceiveData(slave);
    if (!incoming) {
        fprintf(stderr, "[Slave] Task2: no se recibio mensaje del master\n");
        return;
    }

    int count = incoming->payloadSize / (int)sizeof(RrProcessData);
    RrProcessData* rrData = (RrProcessData*)malloc(incoming->payloadSize);
    if (!rrData) { pvmMessageDestroy(incoming); return; }
    memcpy(rrData, incoming->payload, incoming->payloadSize);
    pvmMessageDestroy(incoming);

    AgingResults aging = {0};
    float totalUtil = 0.0f;

    // Ordenar por desperdicio descendente para obtener top-5
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (rrData[j].wastedCpuCycles > rrData[i].wastedCpuCycles) {
                RrProcessData tmp = rrData[i];
                rrData[i] = rrData[j];
                rrData[j] = tmp;
            }
        }
    }

    int top = (count < totalRankingProcesos) ? count : totalRankingProcesos;
    for (int i = 0; i < top; i++) {
        memcpy(aging.topAgedIds[i], rrData[i].processId, idProcesoLen);
        aging.topAgedCpuWaste[i] = rrData[i].wastedCpuCycles;
        memcpy(aging.topWastersIds[i], rrData[i].processId, idProcesoLen);
        aging.topWastersCpuWaste[i] = rrData[i].wastedCpuCycles;
        aging.totalReturnsToReady += rrData[i].timesReturnedToReady;
        totalUtil += rrData[i].cpuWasteRatio;
    }
    aging.topAgedCount   = top;
    aging.topWastersCount = top;
    aging.avgCpuUtilizationPerSlave = (top > 0) ? totalUtil / top : 0.0f;

    free(rrData);

    PvmMessage* reply = packAgingMessage(0, slave->slaveTid, slave->masterTid, &aging);
    if (reply) {
        pvmSlaveSendData(slave, reply);
        pvmMessageDestroy(reply);
    }
}

// ── main del esclavo (ejecutable independiente) ───────────────
int main(void) {
    int masterTid = pvm_parent();
    if (masterTid < 0) {
        fprintf(stderr, "[Slave] No se pudo obtener TID del master\n");
        pvm_exit();
        return 1;
    }

    PvmSlave* slave = pvmSlaveInit(masterTid, totalProcesos);
    if (!slave) { pvm_exit(); return 1; }

    fprintf(stderr, "[Slave TID=%d] Listo, esperando tareas...\n", slave->slaveTid);

    // El master envía dos mensajes: uno para Tarea 1, otro para Tarea 2.
    // Se procesan en orden de llegada usando el tipo del mensaje.
    for (int task = 0; task < 2; task++) {
        struct timeval timeout = {15, 0};
        int ret = pvm_trecv(-1, -1, &timeout);
        if (ret <= 0) {
            fprintf(stderr, "[Slave] Timeout esperando tarea %d\n", task + 1);
            break;
        }
        PvmMessage* msg = (PvmMessage*)malloc(sizeof(PvmMessage));
        if (!msg) break;
        pvm_upkbyte((char*)msg, sizeof(PvmMessage), 1);

        // Reinyectar en el PvmSlave para que pvmSlaveRunTaskX lo lea
        // Usamos un mensaje dummy como "prefetch" y lo procesamos aquí directamente
        if (msg->messageType == MessageBcpSnapshot && task == 0) {
            // Reutilizar la lógica de Task1 inlineada
            int count = msg->payloadSize / (int)sizeof(BcpSummary);
            BcpSummary* summaries = (BcpSummary*)malloc(msg->payloadSize);
            if (summaries) {
                memcpy(summaries, msg->payload, msg->payloadSize);
                DistributedStats stats = {0};
                long totalRemaining = 0;
                for (int i = 0; i < count; i++) {
                    if (summaries[i].state == 6) { stats.totalProcessesFinished++; continue; }
                    if (summaries[i].state == 3) stats.totalProcessesWaiting++;
                    if (summaries[i].timesInIo > 0) stats.totalIoOperations++;
                    totalRemaining += summaries[i].remainingCycles;
                }
                int active = count - stats.totalProcessesFinished;
                stats.avgRemainingCycles = (active > 0) ? (int)(totalRemaining / active) : 0;
                long totalAssigned = 0;
                for (int i = 0; i < count; i++) totalAssigned += summaries[i].totalCpuCycles;
                long done = totalAssigned - totalRemaining;
                stats.avgCpuUtilization = (totalAssigned > 0) ? (float)done / totalAssigned : 0.0f;
                free(summaries);
                PvmMessage* reply = packStatsMessage(0, slave->slaveTid, masterTid, &stats);
                if (reply) { pvmSlaveSendData(slave, reply); pvmMessageDestroy(reply); }
            }
        } else if (msg->messageType == MessageBcpSnapshot && task == 1) {
            int count = msg->payloadSize / (int)sizeof(RrProcessData);
            RrProcessData* rrData = (RrProcessData*)malloc(msg->payloadSize);
            if (rrData) {
                memcpy(rrData, msg->payload, msg->payloadSize);
                AgingResults aging = {0};
                float totalUtil = 0.0f;
                for (int i = 0; i < count - 1; i++)
                    for (int j = i+1; j < count; j++)
                        if (rrData[j].wastedCpuCycles > rrData[i].wastedCpuCycles) {
                            RrProcessData tmp = rrData[i]; rrData[i] = rrData[j]; rrData[j] = tmp;
                        }
                int top = (count < totalRankingProcesos) ? count : totalRankingProcesos;
                for (int i = 0; i < top; i++) {
                    memcpy(aging.topAgedIds[i], rrData[i].processId, idProcesoLen);
                    aging.topAgedCpuWaste[i] = rrData[i].wastedCpuCycles;
                    memcpy(aging.topWastersIds[i], rrData[i].processId, idProcesoLen);
                    aging.topWastersCpuWaste[i] = rrData[i].wastedCpuCycles;
                    aging.totalReturnsToReady += rrData[i].timesReturnedToReady;
                    totalUtil += rrData[i].cpuWasteRatio;
                }
                aging.topAgedCount = aging.topWastersCount = top;
                aging.avgCpuUtilizationPerSlave = (top > 0) ? totalUtil / top : 0.0f;
                free(rrData);
                PvmMessage* reply = packAgingMessage(0, slave->slaveTid, masterTid, &aging);
                if (reply) { pvmSlaveSendData(slave, reply); pvmMessageDestroy(reply); }
            }
        }
        pvmMessageDestroy(msg);
    }

    pvmSlaveCleanup(slave);
    return 0;
}
