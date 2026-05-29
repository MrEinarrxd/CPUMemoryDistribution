#include "messageProtocol.h"
#include "../../utils/constants.h"
#include "../../utils/errorHandler.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

PvmMessage* packBcpMessage(int messageId, int sourceNodeId, int destinationNodeId,
                           const void* bcpData, int dataSize) {
    PvmMessage* msg = (PvmMessage*)malloc(sizeof(PvmMessage));
    if (!msg) {
        errorHandlerLog(ErrorCodeMemoryAllocationFailed, "packBcpMessage");
        return NULL;
    }
    memset(msg, 0, sizeof(PvmMessage));
    msg->messageId = messageId;
    msg->sourceNodeId = sourceNodeId;
    msg->destinationNodeId = destinationNodeId;
    msg->messageType = MessageBcpSnapshot;
    msg->timestamp = (long)time(NULL);
    if (bcpData && dataSize > 0) {
        if (dataSize > tamanoBufferMensaje) dataSize = tamanoBufferMensaje;
        memcpy(msg->payload, bcpData, dataSize);
        msg->payloadSize = dataSize;
    } else {
        msg->payloadSize = 0;
    }
    return msg;
}

void unpackBcpMessage(PvmMessage* message, void* outBcpData, int maxSize) {
    if (!message || !outBcpData) return;
    int copySize = message->payloadSize;
    if (copySize > maxSize) copySize = maxSize;
    memcpy(outBcpData, message->payload, copySize);
}

PvmMessage* packStatsMessage(int messageId, int sourceNodeId, int destinationNodeId,
                             DistributedStats* stats) {
    PvmMessage* msg = (PvmMessage*)malloc(sizeof(PvmMessage));
    if (!msg) return NULL;
    memset(msg, 0, sizeof(PvmMessage));
    msg->messageId = messageId;
    msg->sourceNodeId = sourceNodeId;
    msg->destinationNodeId = destinationNodeId;
    msg->messageType = MessageStatistics;
    msg->timestamp = (long)time(NULL);
    if (stats) {
        memcpy(msg->payload, stats, sizeof(DistributedStats));
        msg->payloadSize = sizeof(DistributedStats);
    } else {
        msg->payloadSize = 0;
    }
    return msg;
}

void unpackStatsMessage(PvmMessage* message, DistributedStats* outStats) {
    if (!message || !outStats) return;
    if (message->payloadSize >= sizeof(DistributedStats))
        memcpy(outStats, message->payload, sizeof(DistributedStats));
    else
        memset(outStats, 0, sizeof(DistributedStats));
}

PvmMessage* packAgingMessage(int messageId, int sourceNodeId, int destinationNodeId,
                             AgingResults* aging) {
    PvmMessage* msg = (PvmMessage*)malloc(sizeof(PvmMessage));
    if (!msg) return NULL;
    memset(msg, 0, sizeof(PvmMessage));
    msg->messageId = messageId;
    msg->sourceNodeId = sourceNodeId;
    msg->destinationNodeId = destinationNodeId;
    msg->messageType = MessageAging;
    msg->timestamp = (long)time(NULL);
    if (aging) {
        memcpy(msg->payload, aging, sizeof(AgingResults));
        msg->payloadSize = sizeof(AgingResults);
    } else {
        msg->payloadSize = 0;
    }
    return msg;
}

void unpackAgingMessage(PvmMessage* message, AgingResults* outAging) {
    if (!message || !outAging) return;
    if (message->payloadSize >= sizeof(AgingResults))
        memcpy(outAging, message->payload, sizeof(AgingResults));
    else
        memset(outAging, 0, sizeof(AgingResults));
}

void pvmMessageDestroy(PvmMessage* message) { if (message) free(message); }