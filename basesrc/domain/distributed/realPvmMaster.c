#include "realPvmMaster.h"
#include "messageProtocol.h"
#include "../core/bcp.h"
#include "../core/process.h"
#include "../core/processTable.h"
#include "../../utils/constants.h"
#include "../../utils/errorHandler.h"
#include "../../presentation/consoleIo.h"
#include <pvm3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

static int pvmMasterSendData(PvmMaster* master, PvmMessage* message);
static PvmMessage* pvmMasterReceiveData(PvmMaster* master);

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
    summary->timeInExecution = bcp->timeInExecution;
    summary->timesInIo = bcp->timesInIo;
    summary->wastedCpuCycles = bcp->wastedCpuCycles;
}

static void bcpToRrProcessData(Bcp* bcp, RrProcessData* data) {
    if (!bcp || !data) return;
    data->pid = bcp->pid;
    strncpy(data->processId, bcp->processId, idProcesoLen - 1);
    data->processId[idProcesoLen - 1] = '\0';
    data->remainingCycles = bcp->remainingCycles;
    data->totalCpuCycles = bcp->totalCpuCycles;
    data->timeInExecution = bcp->timeInExecution;
    data->quantumAssigned = bcp->quantumAssigned;
    data->quantumUsed = bcp->quantumUsed;
    data->timesReturnedToReady = bcp->timesReturnedToReady;
    data->wastedCpuCycles = bcp->wastedCpuCycles;
    data->cpuWasteRatio = bcp->cpuWasteRatio;
}

static int compareRankingEntries(const void* a, const void* b) {
    const struct { char processId[idProcesoLen]; int primary; int secondary; } *A = a, *B = b;
    int diff = B->primary - A->primary;
    if (diff == 0) diff = B->secondary - A->secondary;
    return (diff != 0) ? diff : strcmp(A->processId, B->processId);
}

static void mergeTopRankings(const AgingResults* slaveResults, int numSlaves,
                             char outIds[totalRankingProcesos][idProcesoLen],
                             int outPrimary[totalRankingProcesos],
                             int outSecondary[totalRankingProcesos],
                             int* outCount, int useAgedList) {
    struct { char processId[idProcesoLen]; int primary; int secondary; } entries[pvmNumEsclavos * totalRankingProcesos];
    int totalEntries = 0;
    for (int i = 0; i < numSlaves; i++) {
        int cnt = useAgedList ? slaveResults[i].topAgedCount : slaveResults[i].topWastersCount;
        for (int j = 0; j < cnt && j < totalRankingProcesos; j++) {
            if (totalEntries >= pvmNumEsclavos * totalRankingProcesos) break;
            if (useAgedList) {
                memcpy(entries[totalEntries].processId, slaveResults[i].topAgedIds[j], idProcesoLen);
                entries[totalEntries].primary = slaveResults[i].topAgedReturns[j];
                entries[totalEntries].secondary = slaveResults[i].topAgedRemainingCycles[j];
            } else {
                memcpy(entries[totalEntries].processId, slaveResults[i].topWastersIds[j], idProcesoLen);
                entries[totalEntries].primary = slaveResults[i].topWastersCpuWaste[j];
                entries[totalEntries].secondary = 0;
            }
            totalEntries++;
        }
    }
    if (totalEntries == 0) { *outCount = 0; return; }
    qsort(entries, totalEntries, sizeof(entries[0]), compareRankingEntries);
    int copy = (totalEntries < totalRankingProcesos) ? totalEntries : totalRankingProcesos;
    for (int i = 0; i < copy; i++) {
        memcpy(outIds[i], entries[i].processId, idProcesoLen);
        outPrimary[i] = entries[i].primary;
        if (outSecondary) outSecondary[i] = entries[i].secondary;
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
    const char* slaveExec = getenv("PVM_SLAVE_EXEC");
    if (!slaveExec || slaveExec[0] == '\0') slaveExec = "simSlave";

    const char* hosts = getenv(pvmSlaveHostsEnvVar);
    if (!hosts || hosts[0] == '\0') {
        int num = pvm_spawn((char*)slaveExec, NULL, 0, "", pvmNumEsclavos, master->slaveTids);
        if (num < 0) { errorHandlerLog(ErrorCodeNodeConnectionFailed, "pvmMasterSpawnSlaves"); return -1; }
        return num;
    }

    char hostsCopy[longitudMaximaCadena];
    strncpy(hostsCopy, hosts, sizeof(hostsCopy) - 1);
    hostsCopy[sizeof(hostsCopy) - 1] = '\0';

    int spawned = 0;
    char* host = strtok(hostsCopy, ",");
    while (host && spawned < pvmNumEsclavos) {
        while (*host == ' ' || *host == '\t') host++;
        int tid = 0;
        int num = pvm_spawn((char*)slaveExec, NULL, PvmTaskHost, host, 1, &tid);
        if (num == 1) master->slaveTids[spawned++] = tid;
        host = strtok(NULL, ",");
    }

    if (spawned != pvmNumEsclavos)
        errorHandlerLog(ErrorCodeNodeConnectionFailed, "pvmMasterSpawnSlaves: no se pudieron lanzar todos los hosts configurados");
    return spawned;
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
        PvmMessage* msg = packBcpMessage(slaveIdx, master->masterTid, slaveIdx,
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
        PvmMessage* msg = packBcpMessage(slaveIdx, master->masterTid, slaveIdx,
                                        rrData, count * sizeof(RrProcessData));
        if (msg) {
            msg->messageType = MessageAging;
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

static int pvmMasterSendData(PvmMaster* master, PvmMessage* message) {
    if (!master || !message) return -1;
    int destTid = master->slaveTids[message->destinationNodeId];
    if (destTid <= 0) return -1;
    pvm_initsend(PvmDataDefault);
    pvm_pkbyte((char*)message, sizeof(PvmMessage), 1);
    return pvm_send(destTid, message->messageType);
}

static PvmMessage* pvmMasterReceiveData(PvmMaster* master) {
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
        integrated1.processCount += master->task1Results[i].processCount;
        integrated1.activeCount += master->task1Results[i].activeCount;
        integrated1.totalProcessesFinished += master->task1Results[i].totalProcessesFinished;
        integrated1.totalProcessesWaiting += master->task1Results[i].totalProcessesWaiting;
        integrated1.totalRemainingCycles += master->task1Results[i].totalRemainingCycles;
        integrated1.totalAssignedCycles += master->task1Results[i].totalAssignedCycles;
        integrated1.totalExecutedCycles += master->task1Results[i].totalExecutedCycles;
        integrated1.totalIoOperations += master->task1Results[i].totalIoOperations;
    }
    integrated1.avgRemainingCycles = integrated1.activeCount > 0
        ? (int)(integrated1.totalRemainingCycles / integrated1.activeCount)
        : 0;
    integrated1.avgCpuUtilization = integrated1.totalAssignedCycles > 0
        ? (float)integrated1.totalExecutedCycles / (float)integrated1.totalAssignedCycles
        : 0.0f;
    AgingResults integrated2 = {0};
    for (int i = 0; i < pvmNumEsclavos; i++) {
        integrated2.totalReturnsToReady += master->task2Results[i].totalReturnsToReady;
        integrated2.avgCpuUtilizationPerSlave += master->task2Results[i].avgCpuUtilizationPerSlave;
    }
    integrated2.avgCpuUtilizationPerSlave /= pvmNumEsclavos;
    mergeTopRankings(master->task2Results, pvmNumEsclavos,
                     integrated2.topAgedIds, integrated2.topAgedReturns,
                     integrated2.topAgedRemainingCycles, &integrated2.topAgedCount, 1);
    mergeTopRankings(master->task2Results, pvmNumEsclavos,
                     integrated2.topWastersIds, integrated2.topWastersCpuWaste,
                     NULL, &integrated2.topWastersCount, 0);
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
        snprintf(buf, sizeof(buf), "%d. %s (retornos: %d, pendientes: %d)",
                 i + 1,
                 master->task2Results[0].topAgedIds[i],
                 master->task2Results[0].topAgedReturns[i],
                 master->task2Results[0].topAgedRemainingCycles[i]);
        consoleIoPrintLine(buf);
    }
    consoleIoPrintFloat("Promedio aprovechamiento CPU: ", master->task2Results[0].avgCpuUtilizationPerSlave);
    consoleIoPrintInt("Veces que retornaron a Listos: ", master->task2Results[0].totalReturnsToReady);
    consoleIoPrintSeparator();
}

void pvmMasterCleanup(PvmMaster* master) {
    if (master) { pvm_exit(); free(master); }
}
