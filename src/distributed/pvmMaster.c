#include "pvmMaster.h"
#include "distributedTasks.h"
#include "../utils/constants.h"

#include <pvm3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

static int sendPacket(int tid, const PvmPacket* packet) {
    if (!packet || tid <= 0) return -1;
    pvm_initsend(PvmDataDefault);
    pvm_pkbyte((char*)packet, (int)sizeof(*packet), 1);
    return pvm_send(tid, packet->messageType);
}

static int receivePacket(int messageType, PvmPacket* packet) {
    struct timeval timeout;
    int ret;

    if (!packet) return -1;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    ret = pvm_trecv(-1, messageType, &timeout);
    if (ret <= 0) return -1;
    memset(packet, 0, sizeof(*packet));
    pvm_upkbyte((char*)packet, (int)sizeof(*packet), 1);
    return 0;
}

static void trimHost(char* text) {
    char* start;
    char* end;

    if (!text) return;
    start = text;
    while (*start == ' ' || *start == '\t') start++;
    if (start != text) memmove(text, start, strlen(start) + 1);
    end = text + strlen(text);
    while (end > text && (end[-1] == ' ' || end[-1] == '\t')) {
        end--;
    }
    *end = '\0';
}

static int spawnSlaves(const char* slaveExec, const char* slaveHosts, int tids[], int count) {
    char hosts[256];
    char* token;
    int spawned = 0;

    if (!slaveExec || !tids || count <= 0) return -1;
    if (!slaveHosts || slaveHosts[0] == '\0') {
        return pvm_spawn((char*)slaveExec, NULL, 0, "", count, tids);
    }

    strncpy(hosts, slaveHosts, sizeof(hosts) - 1);
    hosts[sizeof(hosts) - 1] = '\0';
    token = strtok(hosts, ",");
    while (token && spawned < count) {
        int result;
        trimHost(token);
        if (token[0] == '\0') return -1;
        result = pvm_spawn((char*)slaveExec, NULL, PvmTaskHost, token, 1, &tids[spawned]);
        if (result != 1) return -1;
        spawned++;
        token = strtok(NULL, ",");
    }
    return spawned == count ? spawned : -1;
}

static void sendFinishToSlaves(const int tids[], int count) {
    PvmPacket packet;

    if (!tids) return;
    for (int i = 0; i < count; ++i) {
        if (tids[i] <= 0) continue;
        protocolClearPacket(&packet, pvmMessageFinish, i);
        sendPacket(tids[i], &packet);
    }
}

static void fillStatsPacket(PvmPacket* packet, const ProcessStatsRow rows[], int start, int count, int worker) {
    protocolClearPacket(packet, pvmMessageStatsRequest, worker);
    packet->rowCount = count;
    for (int i = 0; i < count; ++i) packet->statsRows[i] = rows[start + i];
}

static void fillAgingPacket(PvmPacket* packet, const RrAnalysisRow rows[], int start, int count, int worker) {
    protocolClearPacket(packet, pvmMessageAgingRequest, worker);
    packet->rowCount = count;
    for (int i = 0; i < count; ++i) packet->rrRows[i] = rows[start + i];
}

int pvmMasterRunReal(const ProcessTable* table, DistributedReport* report) {
    int masterTid;
    int tids[2] = {0, 0};
    int spawned;
    const char* slaveExec;
    const char* slaveHosts;
    ProcessStatsRow statsRows[TotalProcesses];
    RrAnalysisRow rrRows[TotalProcesses];
    DistributedStatsResult statsPartials[2];
    DistributedAgingResult agingPartials[2];
    PvmPacket packet;
    int statsCount;
    int rrCount;
    int statsSplit;
    int rrSplit;

    if (!table || !report) return -1;
    memset(report, 0, sizeof(*report));
    memset(statsPartials, 0, sizeof(statsPartials));
    memset(agingPartials, 0, sizeof(agingPartials));

    masterTid = pvm_mytid();
    if (masterTid < 0) return -1;

    slaveExec = getenv("PVM_SLAVE_EXEC");
    if (!slaveExec || slaveExec[0] == '\0') slaveExec = DefaultPvmSlaveExec;
    slaveHosts = getenv("PVM_SLAVE_HOSTS");
    spawned = spawnSlaves(slaveExec, slaveHosts, tids, 2);
    if (spawned != 2) {
        pvm_exit();
        return -1;
    }

    statsCount = processTableExportStatsRows(table, statsRows, TotalProcesses);
    rrCount = processTableExportRrRows(table, rrRows, TotalProcesses);
    statsSplit = statsCount / 2;
    rrSplit = rrCount / 2;

    fillStatsPacket(&packet, statsRows, 0, statsSplit, 0);
    if (sendPacket(tids[0], &packet) < 0) {
        sendFinishToSlaves(tids, 2);
        pvm_exit();
        return -1;
    }
    fillStatsPacket(&packet, statsRows, statsSplit, statsCount - statsSplit, 1);
    if (sendPacket(tids[1], &packet) < 0) {
        sendFinishToSlaves(tids, 2);
        pvm_exit();
        return -1;
    }

    for (int i = 0; i < 2; ++i) {
        if (receivePacket(pvmMessageStatsResult, &packet) == 0 &&
            packet.workerIndex >= 0 && packet.workerIndex < 2) {
            statsPartials[packet.workerIndex] = packet.statsResult;
        } else {
            sendFinishToSlaves(tids, 2);
            pvm_exit();
            return -1;
        }
    }

    fillAgingPacket(&packet, rrRows, 0, rrSplit, 0);
    if (sendPacket(tids[0], &packet) < 0) {
        sendFinishToSlaves(tids, 2);
        pvm_exit();
        return -1;
    }
    fillAgingPacket(&packet, rrRows, rrSplit, rrCount - rrSplit, 1);
    if (sendPacket(tids[1], &packet) < 0) {
        sendFinishToSlaves(tids, 2);
        pvm_exit();
        return -1;
    }

    for (int i = 0; i < 2; ++i) {
        if (receivePacket(pvmMessageAgingResult, &packet) == 0 &&
            packet.workerIndex >= 0 && packet.workerIndex < 2) {
            agingPartials[packet.workerIndex] = packet.agingResult;
        } else {
            sendFinishToSlaves(tids, 2);
            pvm_exit();
            return -1;
        }
    }

    sendFinishToSlaves(tids, 2);

    distributedTasksIntegrateStats(statsPartials, 2, &report->stats);
    distributedTasksIntegrateAging(agingPartials, 2, &report->aging);
    pvm_exit();
    return 0;
}
