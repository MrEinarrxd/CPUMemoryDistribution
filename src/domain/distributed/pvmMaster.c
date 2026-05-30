#include "pvmMaster.h"
#include "messageProtocol.h"
#include "../process/bcp.h"
#include "../process/process.h"
#include "../process/processTable.h"
#include "../../utils/constants.h"
#include "../../utils/errorHandler.h"
#include "../../presentation/consoleIo.h"
#include <pvm3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

typedef struct {
    char processId[idProcesoLen];
    int primary;
    int secondary;
} RankingEntry;

static int getSnapshotProcesses(ProcessTable* table, Process** outArray, int maxSize) {
    int idx = 0;
    if (!table || !outArray) return 0;
    for (int i = 0; i < procesosEnEjecucion && idx < maxSize; i++)
        if (table->runningProcesses[i] && table->runningProcesses[i]->bcp)
            outArray[idx++] = table->runningProcesses[i];
    for (int i = 0; i < procesosEnEspera && idx < maxSize; i++)
        if (table->newRequests[i] && table->newRequests[i]->bcp)
            outArray[idx++] = table->newRequests[i];
    return idx;
}

static void bcpToBcpSummary(Bcp* bcp, BcpSummary* summary) {
    if (!bcp || !summary) return;
    memset(summary, 0, sizeof(*summary));
    summary->pid = bcp->pid;
    strncpy(summary->processId, bcp->processId, idProcesoLen - 1);
    summary->state = (int)bcp->state;
    summary->remainingCycles = bcp->remainingCycles;
    summary->totalCpuCycles = bcp->totalCpuCycles;
    summary->timeInExecution = bcp->timeInExecution;
    summary->timesInIo = bcp->timesInIo;
    summary->wastedCpuCycles = bcp->wastedCpuCycles;
}

static void bcpToRrProcessData(Bcp* bcp, RrProcessData* data) {
    if (!bcp || !data) return;
    memset(data, 0, sizeof(*data));
    data->pid = bcp->pid;
    strncpy(data->processId, bcp->processId, idProcesoLen - 1);
    data->remainingCycles = bcp->remainingCycles;
    data->totalCpuCycles = bcp->totalCpuCycles;
    data->timeInExecution = bcp->timeInExecution;
    data->quantumAssigned = bcp->quantumAssigned;
    data->quantumUsed = bcp->quantumUsed;
    data->timesReturnedToReady = bcp->timesReturnedToReady;
    data->wastedCpuCycles = bcp->wastedCpuCycles;
    data->cpuWasteRatio = bcp->cpuWasteRatio;
}

static int findSlaveIndex(PvmMaster* master, int tid) {
    if (!master) return -1;
    for (int i = 0; i < pvmNumEsclavos; i++)
        if (master->slaveTids[i] == tid) return i;
    return -1;
}

static int compareRankingEntries(const void* a, const void* b) {
    const RankingEntry* A = (const RankingEntry*)a;
    const RankingEntry* B = (const RankingEntry*)b;
    int diff = B->primary - A->primary;
    if (diff != 0) return diff;
    diff = B->secondary - A->secondary;
    return (diff != 0) ? diff : strcmp(A->processId, B->processId);
}

static void mergeStatsWasters(const DistributedStats* stats, int numSlaves,
                              char outIds[totalRankingProcesos][idProcesoLen],
                              int outWaste[totalRankingProcesos], int* outCount) {
    RankingEntry entries[pvmNumEsclavos * totalRankingProcesos];
    int total = 0;
    for (int i = 0; i < numSlaves; i++) {
        for (int j = 0; j < stats[i].topWastersCount && j < totalRankingProcesos; j++) {
            strncpy(entries[total].processId, stats[i].topWastersIds[j], idProcesoLen - 1);
            entries[total].processId[idProcesoLen - 1] = '\0';
            entries[total].primary = stats[i].topWastersCpuWaste[j];
            entries[total].secondary = 0;
            total++;
        }
    }
    qsort(entries, total, sizeof(entries[0]), compareRankingEntries);
    int copy = (total < totalRankingProcesos) ? total : totalRankingProcesos;
    for (int i = 0; i < copy; i++) {
        strncpy(outIds[i], entries[i].processId, idProcesoLen - 1);
        outIds[i][idProcesoLen - 1] = '\0';
        outWaste[i] = entries[i].primary;
    }
    *outCount = copy;
}

static void mergeAgingRankings(const AgingResults* slaveResults, int numSlaves,
                               AgingResults* integrated) {
    RankingEntry aged[pvmNumEsclavos * totalRankingProcesos];
    RankingEntry wasters[pvmNumEsclavos * totalRankingProcesos];
    int agedCount = 0;
    int wasterCount = 0;

    for (int i = 0; i < numSlaves; i++) {
        for (int j = 0; j < slaveResults[i].topAgedCount && j < totalRankingProcesos; j++) {
            strncpy(aged[agedCount].processId, slaveResults[i].topAgedIds[j], idProcesoLen - 1);
            aged[agedCount].processId[idProcesoLen - 1] = '\0';
            aged[agedCount].primary = slaveResults[i].topAgedReturns[j];
            aged[agedCount].secondary = slaveResults[i].topAgedRemainingCycles[j];
            agedCount++;
        }
        for (int j = 0; j < slaveResults[i].topWastersCount && j < totalRankingProcesos; j++) {
            strncpy(wasters[wasterCount].processId, slaveResults[i].topWastersIds[j], idProcesoLen - 1);
            wasters[wasterCount].processId[idProcesoLen - 1] = '\0';
            wasters[wasterCount].primary = slaveResults[i].topWastersCpuWaste[j];
            wasters[wasterCount].secondary = 0;
            wasterCount++;
        }
    }

    qsort(aged, agedCount, sizeof(aged[0]), compareRankingEntries);
    qsort(wasters, wasterCount, sizeof(wasters[0]), compareRankingEntries);

    integrated->topAgedCount = (agedCount < totalRankingProcesos) ? agedCount : totalRankingProcesos;
    for (int i = 0; i < integrated->topAgedCount; i++) {
        strncpy(integrated->topAgedIds[i], aged[i].processId, idProcesoLen - 1);
        integrated->topAgedIds[i][idProcesoLen - 1] = '\0';
        integrated->topAgedReturns[i] = aged[i].primary;
        integrated->topAgedRemainingCycles[i] = aged[i].secondary;
    }

    integrated->topWastersCount = (wasterCount < totalRankingProcesos) ? wasterCount : totalRankingProcesos;
    for (int i = 0; i < integrated->topWastersCount; i++) {
        strncpy(integrated->topWastersIds[i], wasters[i].processId, idProcesoLen - 1);
        integrated->topWastersIds[i][idProcesoLen - 1] = '\0';
        integrated->topWastersCpuWaste[i] = wasters[i].primary;
    }
}

static int sendStatsRequest(PvmMaster* master, int slaveIdx, Process** processes, int start, int count) {
    if (!master || slaveIdx < 0 || slaveIdx >= pvmNumEsclavos) return -1;
    pvm_initsend(PvmDataDefault);
    pvm_pkint(&count, 1, 1);
    for (int i = 0; i < count; i++) {
        BcpSummary summary;
        bcpToBcpSummary(processes[start + i]->bcp, &summary);
        pvm_pkint(&summary.pid, 1, 1);
        pvm_pkstr(summary.processId);
        pvm_pkint(&summary.state, 1, 1);
        pvm_pkint(&summary.remainingCycles, 1, 1);
        pvm_pkint(&summary.totalCpuCycles, 1, 1);
        pvm_pkint(&summary.timeInExecution, 1, 1);
        pvm_pkint(&summary.timesInIo, 1, 1);
        pvm_pkint(&summary.wastedCpuCycles, 1, 1);
    }
    return pvm_send(master->slaveTids[slaveIdx], PvmTagStatsRequest);
}

static int sendAgingRequest(PvmMaster* master, int slaveIdx, Process** processes, int start, int count) {
    if (!master || slaveIdx < 0 || slaveIdx >= pvmNumEsclavos) return -1;
    pvm_initsend(PvmDataDefault);
    pvm_pkint(&count, 1, 1);
    for (int i = 0; i < count; i++) {
        RrProcessData data;
        bcpToRrProcessData(processes[start + i]->bcp, &data);
        pvm_pkint(&data.pid, 1, 1);
        pvm_pkstr(data.processId);
        pvm_pkint(&data.remainingCycles, 1, 1);
        pvm_pkint(&data.totalCpuCycles, 1, 1);
        pvm_pkint(&data.timeInExecution, 1, 1);
        pvm_pkint(&data.quantumAssigned, 1, 1);
        pvm_pkint(&data.quantumUsed, 1, 1);
        pvm_pkint(&data.timesReturnedToReady, 1, 1);
        pvm_pkint(&data.wastedCpuCycles, 1, 1);
        pvm_pkfloat(&data.cpuWasteRatio, 1, 1);
    }
    return pvm_send(master->slaveTids[slaveIdx], PvmTagAgingRequest);
}

static int receiveStatsResponse(PvmMaster* master) {
    struct timeval timeout = {10, 0};
    int buf = pvm_trecv(-1, PvmTagStatsResponse, &timeout);
    if (buf <= 0) {
        errorHandlerLog(ErrorCodeNodeConnectionFailed, "pvmMaster: timeout esperando estadisticas de slave");
        return -1;
    }

    int bytes = 0, tag = 0, tid = 0;
    pvm_bufinfo(buf, &bytes, &tag, &tid);
    int idx = findSlaveIndex(master, tid);
    if (idx < 0) return -1;

    DistributedStats* stats = &master->task1Results[idx];
    memset(stats, 0, sizeof(*stats));
    pvm_upkint(&stats->processCount, 1, 1);
    pvm_upkint(&stats->activeCount, 1, 1);
    pvm_upkint(&stats->totalProcessesFinished, 1, 1);
    pvm_upkint(&stats->totalProcessesWaiting, 1, 1);
    pvm_upkint(&stats->avgRemainingCycles, 1, 1);
    pvm_upklong(&stats->totalRemainingCycles, 1, 1);
    pvm_upklong(&stats->totalAssignedCycles, 1, 1);
    pvm_upklong(&stats->totalExecutedCycles, 1, 1);
    pvm_upkint(&stats->totalIoOperations, 1, 1);
    pvm_upkfloat(&stats->avgCpuUtilization, 1, 1);
    pvm_upkint(&stats->topWastersCount, 1, 1);
    for (int i = 0; i < stats->topWastersCount && i < totalRankingProcesos; i++) {
        pvm_upkstr(stats->topWastersIds[i]);
        pvm_upkint(&stats->topWastersCpuWaste[i], 1, 1);
    }
    return 0;
}

static int receiveAgingResponse(PvmMaster* master) {
    struct timeval timeout = {10, 0};
    int buf = pvm_trecv(-1, PvmTagAgingResponse, &timeout);
    if (buf <= 0) {
        errorHandlerLog(ErrorCodeNodeConnectionFailed, "pvmMaster: timeout esperando analisis RR de slave");
        return -1;
    }

    int bytes = 0, tag = 0, tid = 0;
    pvm_bufinfo(buf, &bytes, &tag, &tid);
    int idx = findSlaveIndex(master, tid);
    if (idx < 0) return -1;

    AgingResults* aging = &master->task2Results[idx];
    memset(aging, 0, sizeof(*aging));
    pvm_upkint(&aging->topAgedCount, 1, 1);
    for (int i = 0; i < aging->topAgedCount && i < totalRankingProcesos; i++) {
        pvm_upkstr(aging->topAgedIds[i]);
        pvm_upkint(&aging->topAgedReturns[i], 1, 1);
        pvm_upkint(&aging->topAgedRemainingCycles[i], 1, 1);
    }
    pvm_upkint(&aging->topWastersCount, 1, 1);
    for (int i = 0; i < aging->topWastersCount && i < totalRankingProcesos; i++) {
        pvm_upkstr(aging->topWastersIds[i]);
        pvm_upkint(&aging->topWastersCpuWaste[i], 1, 1);
    }
    pvm_upkfloat(&aging->avgCpuUtilizationPerSlave, 1, 1);
    pvm_upkint(&aging->totalReturnsToReady, 1, 1);
    return 0;
}

PvmMaster* pvmMasterInit(void) {
    PvmMaster* master = (PvmMaster*)calloc(1, sizeof(PvmMaster));
    if (!master) { errorHandlerLog(ErrorCodeMemoryAllocationFailed, "pvmMasterInit"); return NULL; }
    int tid = pvm_mytid();
    if (tid < 0) {
        errorHandlerLog(ErrorCodeNodeConnectionFailed, "pvmMasterInit: pvmd no esta corriendo o no responde");
        free(master);
        return NULL;
    }
    master->masterTid = tid;
    return master;
}

int pvmMasterSpawnSlaves(PvmMaster* master) {
    if (!master) return -1;
    char defaultSlaveExec[longitudMaximaCadena];
    const char* slaveExec = getenv("PVM_SLAVE_EXEC");
    if (!slaveExec || slaveExec[0] == '\0') {
        if (getcwd(defaultSlaveExec, sizeof(defaultSlaveExec))) {
            size_t len = strlen(defaultSlaveExec);
            snprintf(defaultSlaveExec + len, sizeof(defaultSlaveExec) - len, "/pvmSlave");
            slaveExec = defaultSlaveExec;
        } else {
            slaveExec = "pvmSlave";
        }
    }

    const char* hosts = getenv(pvmSlaveHostsEnvVar);
    if (!hosts || hosts[0] == '\0') {
        errorHandlerLog(ErrorCodeNodeConnectionFailed, "pvmMasterSpawnSlaves: falta export PVM_SLAVE_HOSTS");
        return -1;
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
        if (num == 1) {
            master->slaveTids[spawned++] = tid;
        } else {
            char detail[longitudMaximaCadena];
            snprintf(detail, sizeof(detail), "pvm_spawn fallo en host %s usando %s", host, slaveExec);
            errorHandlerLog(ErrorCodeNodeConnectionFailed, detail);
            pvm_perror("pvm_spawn");
        }
        host = strtok(NULL, ",");
    }

    if (spawned != pvmNumEsclavos)
        errorHandlerLog(ErrorCodeNodeConnectionFailed, "pvmMasterSpawnSlaves: no se pudieron lanzar todos los esclavos");
    return spawned;
}

void pvmMasterTask1Stats(PvmMaster* master) {
    if (!master || !master->processTable) return;
    Process* snapshot[totalProcesos];
    int total = getSnapshotProcesses(master->processTable, snapshot, totalProcesos);
    if (total == 0) { consoleIoPrintLine("No hay procesos para tarea distribuida 1."); return; }

    int base = total / pvmNumEsclavos;
    int rem = total % pvmNumEsclavos;
    int start = 0;
    int requests = 0;
    memset(master->task1Results, 0, sizeof(master->task1Results));

    for (int slaveIdx = 0; slaveIdx < pvmNumEsclavos; slaveIdx++) {
        int count = base + (slaveIdx < rem ? 1 : 0);
        if (sendStatsRequest(master, slaveIdx, snapshot, start, count) == 0) requests++;
        start += count;
    }
    for (int i = 0; i < requests; i++) receiveStatsResponse(master);
}

void pvmMasterTask2Aging(PvmMaster* master) {
    if (!master || !master->processTable) return;
    Process* snapshot[totalProcesos];
    int total = getSnapshotProcesses(master->processTable, snapshot, totalProcesos);
    if (total == 0) { consoleIoPrintLine("No hay procesos para tarea distribuida 2."); return; }

    int base = total / pvmNumEsclavos;
    int rem = total % pvmNumEsclavos;
    int start = 0;
    int requests = 0;
    memset(master->task2Results, 0, sizeof(master->task2Results));

    for (int slaveIdx = 0; slaveIdx < pvmNumEsclavos; slaveIdx++) {
        int count = base + (slaveIdx < rem ? 1 : 0);
        if (sendAgingRequest(master, slaveIdx, snapshot, start, count) == 0) requests++;
        start += count;
    }
    for (int i = 0; i < requests; i++) receiveAgingResponse(master);
}

int pvmMasterSendData(PvmMaster* master, PvmMessage* message) {
    (void)master;
    (void)message;
    return -1;
}

PvmMessage* pvmMasterReceiveData(PvmMaster* master) {
    (void)master;
    return NULL;
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
    integrated1.avgRemainingCycles = (integrated1.activeCount > 0)
        ? (int)(integrated1.totalRemainingCycles / integrated1.activeCount) : 0;
    integrated1.avgCpuUtilization = (integrated1.totalAssignedCycles > 0)
        ? (float)integrated1.totalExecutedCycles / (float)integrated1.totalAssignedCycles : 0.0f;
    if (master->processTable) {
        if (master->processTable->finishedProcesses > integrated1.totalProcessesFinished)
            integrated1.totalProcessesFinished = master->processTable->finishedProcesses;
        if (master->processTable->totalIoOperations > integrated1.totalIoOperations)
            integrated1.totalIoOperations = master->processTable->totalIoOperations;
    }
    mergeStatsWasters(master->task1Results, pvmNumEsclavos,
                      integrated1.topWastersIds, integrated1.topWastersCpuWaste,
                      &integrated1.topWastersCount);

    AgingResults integrated2 = {0};
    int utilSamples = 0;
    for (int i = 0; i < pvmNumEsclavos; i++) {
        integrated2.totalReturnsToReady += master->task2Results[i].totalReturnsToReady;
        if (master->task2Results[i].topAgedCount > 0 || master->task2Results[i].topWastersCount > 0) {
            integrated2.avgCpuUtilizationPerSlave += master->task2Results[i].avgCpuUtilizationPerSlave;
            utilSamples++;
        }
    }
    integrated2.avgCpuUtilizationPerSlave = (utilSamples > 0)
        ? integrated2.avgCpuUtilizationPerSlave / utilSamples : 0.0f;
    mergeAgingRankings(master->task2Results, pvmNumEsclavos, &integrated2);

    master->task1Results[0] = integrated1;
    master->task2Results[0] = integrated2;
}

void pvmMasterPrintResults(PvmMaster* master) {
    if (!master) return;
    consoleIoClear();
    consoleIoPrintSeparator();
    consoleIoPrintLine("=== RESULTADOS TAREA DISTRIBUIDA 1 ===");
    consoleIoPrintInt("Procesos analizados: ", master->task1Results[0].processCount);
    consoleIoPrintInt("Procesos finalizados: ", master->task1Results[0].totalProcessesFinished);
    consoleIoPrintInt("Procesos en espera/E/S: ", master->task1Results[0].totalProcessesWaiting);
    consoleIoPrintInt("Promedio ciclos pendientes: ", master->task1Results[0].avgRemainingCycles);
    consoleIoPrintInt("Envios a E/S totales: ", master->task1Results[0].totalIoOperations);
    consoleIoPrintFloat("Utilizacion CPU promedio: ", master->task1Results[0].avgCpuUtilization);
    consoleIoPrintLine("Top desperdicio CPU:");
    for (int i = 0; i < master->task1Results[0].topWastersCount; i++) {
        char buf[256];
        snprintf(buf, sizeof(buf), "%d. %s (desperdicio: %d)", i + 1,
                 master->task1Results[0].topWastersIds[i],
                 master->task1Results[0].topWastersCpuWaste[i]);
        consoleIoPrintLine(buf);
    }

    consoleIoPrintSeparator();
    consoleIoPrintLine("=== RESULTADOS TAREA DISTRIBUIDA 2 ===");
    consoleIoPrintLine("Top procesos envejecidos/perjudicados:");
    for (int i = 0; i < master->task2Results[0].topAgedCount; i++) {
        char buf[256];
        snprintf(buf, sizeof(buf), "%d. %s (retornos: %d, pendientes: %d)", i + 1,
                 master->task2Results[0].topAgedIds[i],
                 master->task2Results[0].topAgedReturns[i],
                 master->task2Results[0].topAgedRemainingCycles[i]);
        consoleIoPrintLine(buf);
    }
    consoleIoPrintLine("Top procesos con mayor desperdicio:");
    for (int i = 0; i < master->task2Results[0].topWastersCount; i++) {
        char buf[256];
        snprintf(buf, sizeof(buf), "%d. %s (desperdicio: %d)", i + 1,
                 master->task2Results[0].topWastersIds[i],
                 master->task2Results[0].topWastersCpuWaste[i]);
        consoleIoPrintLine(buf);
    }
    consoleIoPrintFloat("Promedio aprovechamiento CPU: ", master->task2Results[0].avgCpuUtilizationPerSlave);
    consoleIoPrintInt("Veces que retornaron a Listos: ", master->task2Results[0].totalReturnsToReady);
    consoleIoPrintSeparator();
}

void pvmMasterCleanup(PvmMaster* master) {
    if (!master) return;
    for (int i = 0; i < pvmNumEsclavos; i++) {
        if (master->slaveTids[i] > 0) {
            pvm_initsend(PvmDataDefault);
            pvm_send(master->slaveTids[i], PvmTagFinish);
        }
    }
    pvm_exit();
    free(master);
}
