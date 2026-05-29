#ifndef MESSAGE_PROTOCOL_H
#define MESSAGE_PROTOCOL_H

#include "../../utils/constants.h"

typedef enum {
    MessageBcpSnapshot,
    MessageStatistics,
    MessageAging,
    MessageFinish
} MessageType;

typedef struct PvmMessage {
    int messageId;
    int sourceNodeId;
    int destinationNodeId;
    MessageType messageType;
    char payload[tamanoBufferMensaje];
    int payloadSize;
    long timestamp;
} PvmMessage;

typedef struct DistributedStats {
    int totalProcessesFinished;
    int totalProcessesWaiting;
    int avgRemainingCycles;
    int totalIoOperations;
    float avgCpuUtilization;
} DistributedStats;

typedef struct AgingResults {
    char topAgedIds[totalRankingProcesos][idProcesoLen];
    int topAgedCpuWaste[totalRankingProcesos];
    int topAgedCount;
    char topWastersIds[totalRankingProcesos][idProcesoLen];
    int topWastersCpuWaste[totalRankingProcesos];
    int topWastersCount;
    float avgCpuUtilizationPerSlave;
    int totalReturnsToReady;
} AgingResults;

PvmMessage* packBcpMessage(int messageId, int sourceNodeId, int destinationNodeId, const void* bcpData, int dataSize);
void unpackBcpMessage(PvmMessage* message, void* outBcpData, int maxSize);
PvmMessage* packStatsMessage(int messageId, int sourceNodeId, int destinationNodeId, DistributedStats* stats);
void unpackStatsMessage(PvmMessage* message, DistributedStats* outStats);
PvmMessage* packAgingMessage(int messageId, int sourceNodeId, int destinationNodeId, AgingResults* aging);
void unpackAgingMessage(PvmMessage* message, AgingResults* outAging);
void pvmMessageDestroy(PvmMessage* message);

#endif
