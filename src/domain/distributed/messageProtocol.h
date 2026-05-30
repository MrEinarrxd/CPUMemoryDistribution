#ifndef MESSAGE_PROTOCOL_H
#define MESSAGE_PROTOCOL_H

#include "../../utils/constants.h"

typedef enum {
    MessageBcpSnapshot,
    MessageStatistics,
    MessageAging,
    MessageFinish
} MessageType;

typedef enum {
    PvmTagStatsRequest = 100,
    PvmTagStatsResponse = 101,
    PvmTagAgingRequest = 200,
    PvmTagAgingResponse = 201,
    PvmTagFinish = 900
} PvmTag;

typedef struct BcpSummary {
    int pid;
    char processId[idProcesoLen];
    int state;
    int remainingCycles;
    int totalCpuCycles;
    int timeInExecution;
    int timesInIo;
    int wastedCpuCycles;
} BcpSummary;

typedef struct RrProcessData {
    int pid;
    char processId[idProcesoLen];
    int remainingCycles;
    int totalCpuCycles;
    int timeInExecution;
    int quantumAssigned;
    int quantumUsed;
    int timesReturnedToReady;
    int wastedCpuCycles;
    float cpuWasteRatio;
} RrProcessData;

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
    int processCount;
    int activeCount;
    int totalProcessesFinished;
    int totalProcessesWaiting;
    int avgRemainingCycles;
    long totalRemainingCycles;
    long totalAssignedCycles;
    long totalExecutedCycles;
    int totalIoOperations;
    float avgCpuUtilization;
    char topWastersIds[totalRankingProcesos][idProcesoLen];
    int topWastersCpuWaste[totalRankingProcesos];
    int topWastersCount;
} DistributedStats;

typedef struct AgingResults {
    char topAgedIds[totalRankingProcesos][idProcesoLen];
    int topAgedReturns[totalRankingProcesos];
    int topAgedRemainingCycles[totalRankingProcesos];
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
