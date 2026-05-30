#include "pvmSlave.h"
#include "messageProtocol.h"
#include "../process/bcp.h"
#include "../../utils/constants.h"
#include <pvm3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char processId[idProcesoLen];
    int primary;
    int secondary;
} RankingEntry;

static int compareRankingEntries(const void* a, const void* b) {
    const RankingEntry* A = (const RankingEntry*)a;
    const RankingEntry* B = (const RankingEntry*)b;
    int diff = B->primary - A->primary;
    if (diff != 0) return diff;
    diff = B->secondary - A->secondary;
    return (diff != 0) ? diff : strcmp(A->processId, B->processId);
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
    return (slave->slaveTid >= 0) ? 0 : -1;
}

PvmMessage* pvmSlaveReceiveData(PvmSlave* slave) {
    (void)slave;
    return NULL;
}

int pvmSlaveSendData(PvmSlave* slave, PvmMessage* message) {
    (void)slave;
    (void)message;
    return -1;
}

void pvmSlaveAssignProcess(PvmSlave* slave, struct Process* process) {
    (void)slave;
    (void)process;
}

int pvmSlaveProcessCount(PvmSlave* slave) {
    return slave ? slave->processCount : 0;
}

void pvmSlaveCleanup(PvmSlave* slave) {
    if (slave) {
        pvm_exit();
        free(slave);
    }
}

static BcpSummary* receiveStatsPayload(int* outCount) {
    int count = 0;
    pvm_upkint(&count, 1, 1);
    if (count < 0 || count > totalProcesos) count = 0;
    BcpSummary* rows = (BcpSummary*)calloc((count > 0) ? count : 1, sizeof(BcpSummary));
    if (!rows) {
        *outCount = 0;
        return NULL;
    }
    for (int i = 0; i < count; i++) {
        pvm_upkint(&rows[i].pid, 1, 1);
        pvm_upkstr(rows[i].processId);
        pvm_upkint(&rows[i].state, 1, 1);
        pvm_upkint(&rows[i].remainingCycles, 1, 1);
        pvm_upkint(&rows[i].totalCpuCycles, 1, 1);
        pvm_upkint(&rows[i].timeInExecution, 1, 1);
        pvm_upkint(&rows[i].timesInIo, 1, 1);
        pvm_upkint(&rows[i].wastedCpuCycles, 1, 1);
    }
    *outCount = count;
    return rows;
}

static RrProcessData* receiveAgingPayload(int* outCount) {
    int count = 0;
    pvm_upkint(&count, 1, 1);
    if (count < 0 || count > totalProcesos) count = 0;
    RrProcessData* rows = (RrProcessData*)calloc((count > 0) ? count : 1, sizeof(RrProcessData));
    if (!rows) {
        *outCount = 0;
        return NULL;
    }
    for (int i = 0; i < count; i++) {
        pvm_upkint(&rows[i].pid, 1, 1);
        pvm_upkstr(rows[i].processId);
        pvm_upkint(&rows[i].remainingCycles, 1, 1);
        pvm_upkint(&rows[i].totalCpuCycles, 1, 1);
        pvm_upkint(&rows[i].timeInExecution, 1, 1);
        pvm_upkint(&rows[i].quantumAssigned, 1, 1);
        pvm_upkint(&rows[i].quantumUsed, 1, 1);
        pvm_upkint(&rows[i].timesReturnedToReady, 1, 1);
        pvm_upkint(&rows[i].wastedCpuCycles, 1, 1);
        pvm_upkfloat(&rows[i].cpuWasteRatio, 1, 1);
    }
    *outCount = count;
    return rows;
}

static DistributedStats calculateStats(const BcpSummary* rows, int count) {
    DistributedStats stats;
    memset(&stats, 0, sizeof(stats));
    RankingEntry wasters[totalProcesos];
    int wasterCount = 0;

    stats.processCount = count;
    for (int i = 0; i < count; i++) {
        stats.totalAssignedCycles += rows[i].totalCpuCycles;
        stats.totalExecutedCycles += rows[i].timeInExecution;
        stats.totalIoOperations += rows[i].timesInIo;

        if (rows[i].state == ProcessStateFinished) {
            stats.totalProcessesFinished++;
        } else {
            stats.activeCount++;
            stats.totalRemainingCycles += rows[i].remainingCycles;
            if (rows[i].state == ProcessStateWaitingIo) stats.totalProcessesWaiting++;
        }

        strncpy(wasters[wasterCount].processId, rows[i].processId, idProcesoLen - 1);
        wasters[wasterCount].processId[idProcesoLen - 1] = '\0';
        wasters[wasterCount].primary = rows[i].wastedCpuCycles;
        wasters[wasterCount].secondary = 0;
        wasterCount++;
    }

    stats.avgRemainingCycles = (stats.activeCount > 0)
        ? (int)(stats.totalRemainingCycles / stats.activeCount) : 0;
    stats.avgCpuUtilization = (stats.totalAssignedCycles > 0)
        ? (float)stats.totalExecutedCycles / (float)stats.totalAssignedCycles : 0.0f;

    qsort(wasters, wasterCount, sizeof(wasters[0]), compareRankingEntries);
    stats.topWastersCount = (wasterCount < totalRankingProcesos) ? wasterCount : totalRankingProcesos;
    for (int i = 0; i < stats.topWastersCount; i++) {
        strncpy(stats.topWastersIds[i], wasters[i].processId, idProcesoLen - 1);
        stats.topWastersIds[i][idProcesoLen - 1] = '\0';
        stats.topWastersCpuWaste[i] = wasters[i].primary;
    }
    return stats;
}

static AgingResults calculateAging(const RrProcessData* rows, int count) {
    AgingResults aging;
    memset(&aging, 0, sizeof(aging));
    RankingEntry aged[totalProcesos];
    RankingEntry wasters[totalProcesos];
    float totalUtil = 0.0f;

    for (int i = 0; i < count; i++) {
        strncpy(aged[i].processId, rows[i].processId, idProcesoLen - 1);
        aged[i].processId[idProcesoLen - 1] = '\0';
        aged[i].primary = rows[i].timesReturnedToReady;
        aged[i].secondary = rows[i].remainingCycles;

        strncpy(wasters[i].processId, rows[i].processId, idProcesoLen - 1);
        wasters[i].processId[idProcesoLen - 1] = '\0';
        wasters[i].primary = rows[i].wastedCpuCycles;
        wasters[i].secondary = 0;

        aging.totalReturnsToReady += rows[i].timesReturnedToReady;
        float utilization = 1.0f - rows[i].cpuWasteRatio;
        if (utilization < 0.0f) utilization = 0.0f;
        if (utilization > 1.0f) utilization = 1.0f;
        totalUtil += utilization;
    }

    qsort(aged, count, sizeof(aged[0]), compareRankingEntries);
    qsort(wasters, count, sizeof(wasters[0]), compareRankingEntries);

    aging.topAgedCount = (count < totalRankingProcesos) ? count : totalRankingProcesos;
    for (int i = 0; i < aging.topAgedCount; i++) {
        strncpy(aging.topAgedIds[i], aged[i].processId, idProcesoLen - 1);
        aging.topAgedIds[i][idProcesoLen - 1] = '\0';
        aging.topAgedReturns[i] = aged[i].primary;
        aging.topAgedRemainingCycles[i] = aged[i].secondary;
    }

    aging.topWastersCount = (count < totalRankingProcesos) ? count : totalRankingProcesos;
    for (int i = 0; i < aging.topWastersCount; i++) {
        strncpy(aging.topWastersIds[i], wasters[i].processId, idProcesoLen - 1);
        aging.topWastersIds[i][idProcesoLen - 1] = '\0';
        aging.topWastersCpuWaste[i] = wasters[i].primary;
    }

    aging.avgCpuUtilizationPerSlave = (count > 0) ? totalUtil / count : 0.0f;
    return aging;
}

static int sendStatsResponse(PvmSlave* slave, const DistributedStats* stats) {
    pvm_initsend(PvmDataDefault);
    pvm_pkint((int*)&stats->processCount, 1, 1);
    pvm_pkint((int*)&stats->activeCount, 1, 1);
    pvm_pkint((int*)&stats->totalProcessesFinished, 1, 1);
    pvm_pkint((int*)&stats->totalProcessesWaiting, 1, 1);
    pvm_pkint((int*)&stats->avgRemainingCycles, 1, 1);
    pvm_pklong((long*)&stats->totalRemainingCycles, 1, 1);
    pvm_pklong((long*)&stats->totalAssignedCycles, 1, 1);
    pvm_pklong((long*)&stats->totalExecutedCycles, 1, 1);
    pvm_pkint((int*)&stats->totalIoOperations, 1, 1);
    pvm_pkfloat((float*)&stats->avgCpuUtilization, 1, 1);
    pvm_pkint((int*)&stats->topWastersCount, 1, 1);
    for (int i = 0; i < stats->topWastersCount && i < totalRankingProcesos; i++) {
        pvm_pkstr((char*)stats->topWastersIds[i]);
        pvm_pkint((int*)&stats->topWastersCpuWaste[i], 1, 1);
    }
    return pvm_send(slave->masterTid, PvmTagStatsResponse);
}

static int sendAgingResponse(PvmSlave* slave, const AgingResults* aging) {
    pvm_initsend(PvmDataDefault);
    pvm_pkint((int*)&aging->topAgedCount, 1, 1);
    for (int i = 0; i < aging->topAgedCount && i < totalRankingProcesos; i++) {
        pvm_pkstr((char*)aging->topAgedIds[i]);
        pvm_pkint((int*)&aging->topAgedReturns[i], 1, 1);
        pvm_pkint((int*)&aging->topAgedRemainingCycles[i], 1, 1);
    }
    pvm_pkint((int*)&aging->topWastersCount, 1, 1);
    for (int i = 0; i < aging->topWastersCount && i < totalRankingProcesos; i++) {
        pvm_pkstr((char*)aging->topWastersIds[i]);
        pvm_pkint((int*)&aging->topWastersCpuWaste[i], 1, 1);
    }
    pvm_pkfloat((float*)&aging->avgCpuUtilizationPerSlave, 1, 1);
    pvm_pkint((int*)&aging->totalReturnsToReady, 1, 1);
    return pvm_send(slave->masterTid, PvmTagAgingResponse);
}

void pvmSlaveRunTask1(PvmSlave* slave) {
    int count = 0;
    BcpSummary* rows = receiveStatsPayload(&count);
    if (!rows) return;
    DistributedStats stats = calculateStats(rows, count);
    free(rows);
    sendStatsResponse(slave, &stats);
}

void pvmSlaveRunTask2(PvmSlave* slave) {
    int count = 0;
    RrProcessData* rows = receiveAgingPayload(&count);
    if (!rows) return;
    AgingResults aging = calculateAging(rows, count);
    free(rows);
    sendAgingResponse(slave, &aging);
}

int main(void) {
    int masterTid = pvm_parent();
    if (masterTid < 0) {
        fprintf(stderr, "[Slave] No se pudo obtener TID del master\n");
        pvm_exit();
        return 1;
    }

    PvmSlave* slave = pvmSlaveInit(masterTid, totalProcesos);
    if (!slave) {
        pvm_exit();
        return 1;
    }

    fprintf(stderr, "[Slave TID=%d] Listo, esperando tareas...\n", slave->slaveTid);

    int running = 1;
    while (running) {
        int buf = pvm_recv(masterTid, -1);
        if (buf <= 0) break;
        int bytes = 0, tag = 0, tid = 0;
        pvm_bufinfo(buf, &bytes, &tag, &tid);
        switch (tag) {
            case PvmTagStatsRequest:
                pvmSlaveRunTask1(slave);
                break;
            case PvmTagAgingRequest:
                pvmSlaveRunTask2(slave);
                break;
            case PvmTagFinish:
                running = 0;
                break;
            default:
                fprintf(stderr, "[Slave] Tag desconocido: %d\n", tag);
                break;
        }
    }

    pvmSlaveCleanup(slave);
    return 0;
}
