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
    timeout.tv_sec = PvmReceiveTimeoutSeconds;
    timeout.tv_usec = 0;
    ret = pvm_trecv(-1, messageType, &timeout);
    if (ret <= 0) return -1;
    memset(packet, 0, sizeof(*packet));
    pvm_upkbyte((char*)packet, (int)sizeof(*packet), 1);
    return 0;
}

static int connectToPvm(void) {
    pvm_setopt(PvmAutoErr, 0);
    return pvm_mytid();
}

static void trimHost(char* text) {
    char* start;
    char* end;

    if (!text) return;
    start = text;
    while (*start == ' ' || *start == '\t') start++;
    if (start != text) memmove(text, start, strlen(start) + 1);

    end = text + strlen(text);
    while (end > text && (end[-1] == ' ' || end[-1] == '\t')) end--;
    *end = '\0';
}

static int hostNameMatches(const char* configured, const char* requested) {
    size_t requestedLen;

    if (!configured || !requested) return 0;
    if (strcmp(configured, requested) == 0) return 1;

    requestedLen = strlen(requested);
    return strncmp(configured, requested, requestedLen) == 0 &&
           configured[requestedLen] == '.';
}

static int configuredHostExists(const char* requested,
                                struct pvmhostinfo* hosts,
                                int hostCount) {
    if (!requested || !hosts || hostCount <= 0) return 0;
    for (int i = 0; i < hostCount; ++i) {
        if (hostNameMatches(hosts[i].hi_name, requested)) return 1;
    }
    return 0;
}

static int validateSpawnHosts(const char* slaveHosts) {
    char hostsText[256];
    char* token;
    struct pvmhostinfo* hosts = NULL;
    int hostCount = 0;
    int archCount = 0;
    int requestedCount = 0;

    if (!slaveHosts || slaveHosts[0] == '\0') return -1;
    if (pvm_config(&hostCount, &archCount, &hosts) < 0) return -1;
    if (!hosts || hostCount <= 0) return -1;

    strncpy(hostsText, slaveHosts, sizeof(hostsText) - 1);
    hostsText[sizeof(hostsText) - 1] = '\0';

    token = strtok(hostsText, ",");
    while (token) {
        trimHost(token);
        if (token[0] == '\0') return -1;
        if (!configuredHostExists(token, hosts, hostCount)) {
            return -1;
        }
        requestedCount++;
        token = strtok(NULL, ",");
    }

    return requestedCount > 0 ? 0 : -1;
}

static int spawnSlaves(const char* slaveExec, const char* slaveHosts, int tids[], int count) {
    char hosts[256];
    char* token;
    int spawned = 0;

    if (!slaveExec || !slaveHosts || !tids || count <= 0) return -1;

    strncpy(hosts, slaveHosts, sizeof(hosts) - 1);
    hosts[sizeof(hosts) - 1] = '\0';

    token = strtok(hosts, ",");
    while (token && spawned < count) {
        int result;
        trimHost(token);
        if (token[0] == '\0') return -1;

        result = pvm_spawn((char*)slaveExec, NULL, PvmTaskHost, token, 1, &tids[spawned]);
        if (result != 1) return spawned;

        spawned++;
        token = strtok(NULL, ",");
    }

    return spawned;
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

static int analyzeWithWorkers(const int tids[], int workerCount, const ProcessTable* table,
                              DistributedReport* report) {
    ProcessStatsRow statsRows[TotalProcesses];
    RrAnalysisRow rrRows[TotalProcesses];
    DistributedStatsResult statsPartials[PvmMasterWorkerCount];
    DistributedAgingResult agingPartials[PvmMasterWorkerCount];
    PvmPacket packet;
    int statsCount;
    int rrCount;

    if (!tids || !table || !report) return -1;
    if (workerCount <= 0 || workerCount > PvmMasterWorkerCount) return -1;
    memset(report, 0, sizeof(*report));
    memset(statsPartials, 0, sizeof(statsPartials));
    memset(agingPartials, 0, sizeof(agingPartials));

    statsCount = processTableExportStatsRows(table, statsRows, TotalProcesses);
    rrCount = processTableExportRrRows(table, rrRows, TotalProcesses);

    for (int worker = 0; worker < workerCount; ++worker) {
        int start = (statsCount * worker) / workerCount;
        int end = (statsCount * (worker + 1)) / workerCount;
        fillStatsPacket(&packet, statsRows, start, end - start, worker);
        if (sendPacket(tids[worker], &packet) < 0) return -1;
    }

    for (int i = 0; i < workerCount; ++i) {
        if (receivePacket(pvmMessageStatsResult, &packet) == 0 &&
            packet.workerIndex >= 0 && packet.workerIndex < workerCount) {
            statsPartials[packet.workerIndex] = packet.statsResult;
        } else {
            return -1;
        }
    }

    for (int worker = 0; worker < workerCount; ++worker) {
        int start = (rrCount * worker) / workerCount;
        int end = (rrCount * (worker + 1)) / workerCount;
        fillAgingPacket(&packet, rrRows, start, end - start, worker);
        if (sendPacket(tids[worker], &packet) < 0) return -1;
    }

    for (int i = 0; i < workerCount; ++i) {
        if (receivePacket(pvmMessageAgingResult, &packet) == 0 &&
            packet.workerIndex >= 0 && packet.workerIndex < workerCount) {
            agingPartials[packet.workerIndex] = packet.agingResult;
        } else {
            return -1;
        }
    }

    distributedTasksIntegrateStats(statsPartials, workerCount, &report->stats);
    distributedTasksIntegrateAging(agingPartials, workerCount, &report->aging);
    return 0;
}

void pvmMasterSessionInit(PvmMasterSession* session) {
    if (!session) return;
    memset(session, 0, sizeof(*session));
    session->masterTid = -1;
}

int pvmMasterSessionStart(PvmMasterSession* session) {
    int spawned;
    const char* slaveExec;
    const char* slaveHosts;

    if (!session) return -1;
    if (session->active) return 0;

    pvmMasterSessionInit(session);
    session->masterTid = connectToPvm();
    if (session->masterTid < 0) {
        pvmMasterSessionInit(session);
        return -1;
    }

    slaveExec = getenv("PVM_SLAVE_EXEC");
    if (!slaveExec || slaveExec[0] == '\0') slaveExec = DefaultPvmSlaveExec;
    slaveHosts = getenv("PVM_SLAVE_HOSTS");
    if (!slaveHosts || slaveHosts[0] == '\0') slaveHosts = DefaultPvmSlaveHosts;

    if (validateSpawnHosts(slaveHosts) != 0) {
        pvm_exit();
        pvmMasterSessionInit(session);
        return -1;
    }

    spawned = spawnSlaves(slaveExec, slaveHosts, session->tids, PvmMasterWorkerCount);
    if (spawned != PvmMasterWorkerCount) {
        if (spawned > 0) sendFinishToSlaves(session->tids, spawned);
        pvm_exit();
        pvmMasterSessionInit(session);
        return -1;
    }

    session->workerCount = PvmMasterWorkerCount;
    session->active = 1;
    return 0;
}

int pvmMasterSessionAnalyze(PvmMasterSession* session, const ProcessTable* table,
                            DistributedReport* report) {
    if (!session || !session->active) return -1;
    return analyzeWithWorkers(session->tids, session->workerCount, table, report);
}

void pvmMasterSessionStop(PvmMasterSession* session) {
    if (!session) return;
    if (session->active) {
        sendFinishToSlaves(session->tids, session->workerCount);
        pvm_exit();
    }
    pvmMasterSessionInit(session);
}

int pvmMasterRunReal(const ProcessTable* table, DistributedReport* report) {
    int result;
    PvmMasterSession session;

    pvmMasterSessionInit(&session);
    if (pvmMasterSessionStart(&session) != 0) return -1;
    result = pvmMasterSessionAnalyze(&session, table, report);
    pvmMasterSessionStop(&session);
    if (result != 0) return -1;
    return 0;
}
