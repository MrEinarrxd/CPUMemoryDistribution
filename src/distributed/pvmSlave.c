#include "distributedTasks.h"
#include "protocol.h"

#include <pvm3.h>
#include <stdio.h>
#include <string.h>

static int receivePacket(int masterTid, PvmPacket* packet) {
    int ret;
    if (!packet) return -1;
    ret = pvm_recv(masterTid, -1);
    if (ret < 0) return -1;
    memset(packet, 0, sizeof(*packet));
    pvm_upkbyte((char*)packet, (int)sizeof(*packet), 1);
    return 0;
}

static int sendPacket(int masterTid, const PvmPacket* packet) {
    if (!packet || masterTid <= 0) return -1;
    pvm_initsend(PvmDataDefault);
    pvm_pkbyte((char*)packet, (int)sizeof(*packet), 1);
    return pvm_send(masterTid, packet->messageType);
}

int main(void) {
    int masterTid = pvm_parent();
    PvmPacket packet;
    PvmPacket reply;

    if (masterTid < 0) {
        pvm_exit();
        return 1;
    }

    while (1) {
        if (receivePacket(masterTid, &packet) != 0) {
            fprintf(stderr, "simSlave: no se pudo recibir mensaje del master\n");
            break;
        }

        if (packet.messageType == pvmMessageFinish) break;

        if (packet.messageType == pvmMessageStatsRequest) {
            protocolClearPacket(&reply, pvmMessageStatsResult, packet.workerIndex);
            distributedTasksCalculateStats(packet.statsRows, packet.rowCount, &reply.statsResult);
            sendPacket(masterTid, &reply);
        } else if (packet.messageType == pvmMessageAgingRequest) {
            protocolClearPacket(&reply, pvmMessageAgingResult, packet.workerIndex);
            distributedTasksCalculateAging(packet.rrRows, packet.rowCount, &reply.agingResult);
            sendPacket(masterTid, &reply);
        }
    }

    pvm_exit();
    return 0;
}
