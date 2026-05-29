#include "pvmMaster.h"
#include "pvmSlave.h"
#include "messageProtocol.h"
#include "../process/bcp.h"
#include "../process/process.h"
#include "../process/processTable.h"
#include "../scheduler/rrScheduler.h"
#include "../../utils/constants.h"
#include "../../utils/errorHandler.h"
#include "../../presentation/consoleIo.h"
#include <pvm3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

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
    float cpuWasteRatio;
} RrProcessData;

static int getActiveProcesses(ProcessTable* table, Process** outArray, int maxSize) {
    int idx = 0;
    if (!table || !outArray) return 0;
    for (int i = 0; i < procesosEnEjecucion && idx < maxSize; i++)
        if (table->runningProcesses[i] && table->runningProcesses[i]->bcp && table->runningProcesses[i]->bcp->state != ProcessStateFinished)
            outArray[idx++] = table->runningProcesses[i];
    for (int i = 0; i < procesosEnEspera && idx < maxSize; i++)
        if (table->newRequests[i] && table->newRequests[i]->bcp && table->newRequests[i]->bcp->state != ProcessStateFinished)
            outArray[idx++] = table->newRequests[i];
    return idx;
}

static void bcpToBcpSummary(Bcp* bcp, BcpSummary* summary) {
    if (!bcp || !summary) return;
    summary->pid = bcp->pid;
    strncpy(summary->processId, bcp->processId, idProcesoLen - 1);
    summary->processId[idProcesoLen - 1] = '\0';
    summary->state = (int)bcp->state;
    summary->remainingCycles = bcp->remainingCycles;
    summary->totalCpuCycles = bcp->totalCpuCycles;
    summary->timesInIo = bcp->timesInIo;
    summary->wastedCpuCycles = bcp->wastedCpuCycles;
}

static void bcpToRrProcessData(Bcp* bcp, RrProcessData* data) {
    if (!bcp || !data) return;
    data->pid = bcp->pid;
    strncpy(data->processId, bcp->processId, idProcesoLen - 1);
    data->processId[idProcesoLen - 1] = '\0';
    data->quantumAssigned = bcp->quantumAssigned;
    data->quantumUsed = bcp->quantumUsed;
    data->timesReturnedToReady = bcp->timesReturnedToReady;
    data->cpuWasteRatio = bcp->cpuWasteRatio;
}

static int compareRankingEntries(const void* a, const void* b) {
    const struct { char processId[idProcesoLen]; int waste; } *A = a, *B = b;
    int diff = B->waste - A->waste;
    return (diff != 0) ? diff : strcmp(A->processId, B->processId);
}

static void mergeTopRankings(const AgingResults* slaveResults, int numSlaves,
                             char outIds[totalRankingProcesos][idProcesoLen],
                             int outWaste[totalRankingProcesos], int* outCount, int useAgedList) {
    struct { char processId[idProcesoLen]; int waste; } entries[pvmNumEsclavos * totalRankingProcesos];
    int totalEntries = 0;
    for (int i = 0; i < numSlaves; i++) {
        int cnt = useAgedList ? slaveResults[i].topAgedCount : slaveResults[i].topWastersCount;
        for (int j = 0; j < cnt && j < totalRankingProcesos; j++) {
            if (totalEntries >= pvmNumEsclavos * totalRankingProcesos) break;
            if (useAgedList) {
                memcpy(entries[totalEntries].processId, slaveResults[i].topAgedIds[j], idProcesoLen);
                entries[totalEntries].waste = slaveResults[i].topAgedCpuWaste[j];
            } else {
                memcpy(entries[totalEntries].processId, slaveResults[i].topWastersIds[j], idProcesoLen);
                entries[totalEntries].waste = slaveResults[i].topWastersCpuWaste[j];
            }
            totalEntries++;
        }
    }
    if (totalEntries == 0) { *outCount = 0; return; }
    qsort(entries, totalEntries, sizeof(entries[0]), compareRankingEntries);
    int copy = (totalEntries < totalRankingProcesos) ? totalEntries : totalRankingProcesos;
    for (int i = 0; i < copy; i++) {
        memcpy(outIds[i], entries[i].processId, idProcesoLen);
        outWaste[i] = entries[i].waste;
    }
    *outCount = copy;
}

PvmMaster* pvmMasterInit(void) {
    PvmMaster* master = (PvmMaster*)calloc(1, sizeof(PvmMaster));
    if (!master) { errorHandlerLog(ErrorCodeMemoryAllocationFailed, "pvmMasterInit"); return NULL; }
    int tid = pvm_mytid();
    if (tid < 0) { errorHandlerLog(ErrorCodeNodeConnectionFailed, "pvmMasterInit"); free(master); return NULL; }
    master->masterTid = tid;
    memset(master->slaveTids, 0, sizeof(master->slaveTids));
    memset(master->task1Results, 0, sizeof(master->task1Results));
    memset(master->task2Results, 0, sizeof(master->task2Results));
    return master;
}

int pvmMasterSpawnSlaves(PvmMaster* master) {
    if (!master) return -1;
    int num = pvm_spawn("pvmSlave", NULL, 0, "", pvmNumEsclavos, master->slaveTids);
    if (num < 0) { errorHandlerLog(ErrorCodeNodeConnectionFailed, "pvmMasterSpawnSlaves"); return -1; }
    return num;
}

void pvmMasterTask1Stats(PvmMaster* master) {
    if (!master || !master->processTable) return;
    Process* active[totalProcesos];
    int totalActive = getActiveProcesses(master->processTable, active, totalProcesos);
    if (totalActive == 0) { consoleIoPrintLine("No hay procesos activos para tarea 1."); return; }
    int split = totalActive / 2;
    for (int slaveIdx = 0; slaveIdx < pvmNumEsclavos; slaveIdx++) {
        int start = (slaveIdx == 0) ? 0 : split;
        int end = (slaveIdx == 0) ? split : totalActive;
        int count = end - start;
        if (count <= 0) continue;
        BcpSummary* summaries = (BcpSummary*)malloc(count * sizeof(BcpSummary));
        if (!summaries) continue;
        for (int i = 0; i < count; i++) {
            if (active[start + i] && active[start + i]->bcp)
                bcpToBcpSummary(active[start + i]->bcp, &summaries[i]);
        }
        PvmMessage* msg = packBcpMessage(slaveIdx, master->masterTid, master->slaveTids[slaveIdx],
                                        summaries, count * sizeof(BcpSummary));
        if (msg) {
            msg->messageType = MessageBcpSnapshot;
            pvmMasterSendData(master, msg);
            pvmMessageDestroy(msg);
        }
        free(summaries);
    }
    for (int slaveIdx = 0; slaveIdx < pvmNumEsclavos; slaveIdx++) {
        PvmMessage* resp = pvmMasterReceiveData(master);
        if (resp && resp->messageType == MessageStatistics) {
            unpackStatsMessage(resp, &master->task1Results[slaveIdx]);
            pvmMessageDestroy(resp);
        }
    }
}

void pvmMasterTask2Aging(PvmMaster* master) {
    if (!master || !master->processTable) return;
    Process* active[totalProcesos];
    int totalActive = getActiveProcesses(master->processTable, active, totalProcesos);
    if (totalActive == 0) { consoleIoPrintLine("No hay procesos activos para tarea 2."); return; }
    int split = totalActive / 2;
    for (int slaveIdx = 0; slaveIdx < pvmNumEsclavos; slaveIdx++) {
        int start = (slaveIdx == 0) ? 0 : split;
        int end = (slaveIdx == 0) ? split : totalActive;
        int count = end - start;
        if (count <= 0) continue;
        RrProcessData* rrData = (RrProcessData*)malloc(count * sizeof(RrProcessData));
        if (!rrData) continue;
        for (int i = 0; i < count; i++) {
            if (active[start + i] && active[start + i]->bcp)
                bcpToRrProcessData(active[start + i]->bcp, &rrData[i]);
        }
        PvmMessage* msg = packBcpMessage(slaveIdx, master->masterTid, master->slaveTids[slaveIdx],
                                        rrData, count * sizeof(RrProcessData));
        if (msg) {
            msg->messageType = MessageBcpSnapshot;
            pvmMasterSendData(master, msg);
            pvmMessageDestroy(msg);
        }
        free(rrData);
    }
    for (int slaveIdx = 0; slaveIdx < pvmNumEsclavos; slaveIdx++) {
        PvmMessage* resp = pvmMasterReceiveData(master);
        if (resp && resp->messageType == MessageAging) {
            unpackAgingMessage(resp, &master->task2Results[slaveIdx]);
            pvmMessageDestroy(resp);
        }
    }
}

int pvmMasterSendData(PvmMaster* master, PvmMessage* message) {
    if (!master || !message) return -1;
    int destTid = master->slaveTids[message->destinationNodeId];
    if (destTid <= 0) return -1;
    pvm_initsend(PvmDataDefault);
    pvm_pkbyte((char*)message, sizeof(PvmMessage), 1);
    return pvm_send(destTid, message->messageType);
}

PvmMessage* pvmMasterReceiveData(PvmMaster* master) {
    if (!master) return NULL;
    pvm_setopt(PvmRoute, PvmRouteDirect);
    struct timeval timeout = {5, 0};
    int ret = pvm_trecv(-1, -1, &timeout);
    if (ret <= 0) return NULL;
    PvmMessage* msg = (PvmMessage*)malloc(sizeof(PvmMessage));
    if (!msg) return NULL;
    pvm_upkbyte((char*)msg, sizeof(PvmMessage), 1);
    return msg;
}

void pvmMasterIntegrateResults(PvmMaster* master) {
    if (!master) return;
    DistributedStats integrated1 = {0};
    for (int i = 0; i < pvmNumEsclavos; i++) {
        integrated1.totalProcessesFinished += master->task1Results[i].totalProcessesFinished;
        integrated1.totalProcessesWaiting += master->task1Results[i].totalProcessesWaiting;
        integrated1.avgRemainingCycles += master->task1Results[i].avgRemainingCycles;
        integrated1.totalIoOperations += master->task1Results[i].totalIoOperations;
        integrated1.avgCpuUtilization += master->task1Results[i].avgCpuUtilization;
    }
    integrated1.avgRemainingCycles /= pvmNumEsclavos;
    integrated1.avgCpuUtilization /= pvmNumEsclavos;
    AgingResults integrated2 = {0};
    for (int i = 0; i < pvmNumEsclavos; i++) {
        integrated2.totalReturnsToReady += master->task2Results[i].totalReturnsToReady;
        integrated2.avgCpuUtilizationPerSlave += master->task2Results[i].avgCpuUtilizationPerSlave;
    }
    integrated2.avgCpuUtilizationPerSlave /= pvmNumEsclavos;
    mergeTopRankings(master->task2Results, pvmNumEsclavos,
                     integrated2.topAgedIds, integrated2.topAgedCpuWaste, &integrated2.topAgedCount, 1);
    mergeTopRankings(master->task2Results, pvmNumEsclavos,
                     integrated2.topWastersIds, integrated2.topWastersCpuWaste, &integrated2.topWastersCount, 0);
    master->task1Results[0] = integrated1;
    master->task2Results[0] = integrated2;
}

void pvmMasterPrintResults(PvmMaster* master) {
    if (!master) return;
    consoleIoClear();
    consoleIoPrintSeparator();
    consoleIoPrintLine("=== RESULTADOS TAREA DISTRIBUIDA 1 ===");
    consoleIoPrintInt("Procesos finalizados: ", master->task1Results[0].totalProcessesFinished);
    consoleIoPrintInt("Procesos en espera: ", master->task1Results[0].totalProcessesWaiting);
    consoleIoPrintInt("Promedio ciclos pendientes: ", master->task1Results[0].avgRemainingCycles);
    consoleIoPrintInt("Operaciones E/S totales: ", master->task1Results[0].totalIoOperations);
    consoleIoPrintFloat("Utilización CPU promedio: ", master->task1Results[0].avgCpuUtilization);
    consoleIoPrintSeparator();
    consoleIoPrintLine("=== RESULTADOS TAREA DISTRIBUIDA 2 ===");
    for (int i = 0; i < master->task2Results[0].topAgedCount && i < totalRankingProcesos; i++) {
        char buf[256];
        snprintf(buf, sizeof(buf), "%d. %s (desperdicio: %d)", i+1, master->task2Results[0].topAgedIds[i], master->task2Results[0].topAgedCpuWaste[i]);
        consoleIoPrintLine(buf);
    }
    consoleIoPrintFloat("Promedio aprovechamiento CPU: ", master->task2Results[0].avgCpuUtilizationPerSlave);
    consoleIoPrintInt("Veces que retornaron a Listos: ", master->task2Results[0].totalReturnsToReady);
    consoleIoPrintSeparator();
}

void pvmMasterCleanup(PvmMaster* master) {
    if (master) { pvm_exit(); free(master); }
}
