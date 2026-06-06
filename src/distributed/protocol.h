#ifndef CpuMemoryDistributedProtocolH
#define CpuMemoryDistributedProtocolH

#include "../utils/constants.h"

typedef enum DistributedMessageType {
    pvmMessageStatsRequest = 1,
    pvmMessageStatsResult = 2,
    pvmMessageAgingRequest = 3,
    pvmMessageAgingResult = 4,
    pvmMessageFinish = 5
} DistributedMessageType;

typedef struct ProcessStatsRow {
    int pid;
    char processId[ProcessIdLen];
    int state;
    int remainingCycles;
    int totalCpuCycles;
    int executedCycles;
    int timesInIo;
    int wastedCpuCycles;
    int timesReturnedToReady;
} ProcessStatsRow;

typedef struct RrAnalysisRow {
    int pid;
    char processId[ProcessIdLen];
    int remainingCycles;
    int totalCpuCycles;
    int executedCycles;
    int quantumAssigned;
    int quantumUsed;
    int rrExecutionCount;
    int rrQuantumAssignedTotal;
    int rrQuantumUsedTotal;
    int timesReturnedToReady;
    int wastedCpuCycles;
    float cpuWasteRatio;
} RrAnalysisRow;

typedef struct DistributedStatsResult {
    int processCount;
    int activeCount;
    int finishedCount;
    int waitingCount;
    long totalRemainingCycles;
    long totalAssignedCycles;
    long totalExecutedCycles;
    int avgRemainingCycles;
    int ioOperations;
    float avgCpuUtilization;
    char topWastersIds[TopRankingCount][ProcessIdLen];
    int topWastersWaste[TopRankingCount];
    int topWastersCount;
} DistributedStatsResult;

typedef struct DistributedAgingResult {
    int processCount;
    char topAgedIds[TopRankingCount][ProcessIdLen];
    int topAgedReturns[TopRankingCount];
    int topAgedRemaining[TopRankingCount];
    int topAgedCount;
    char topWastersIds[TopRankingCount][ProcessIdLen];
    int topWastersWaste[TopRankingCount];
    int topWastersCount;
    float avgCpuUtilization;
    int totalReturnsToReady;
} DistributedAgingResult;

typedef struct DistributedReport {
    DistributedStatsResult stats;
    DistributedAgingResult aging;
} DistributedReport;

typedef struct PvmPacket {
    int messageType;
    int workerIndex;
    int rowCount;
    ProcessStatsRow statsRows[TotalProcesses];
    RrAnalysisRow rrRows[TotalProcesses];
    DistributedStatsResult statsResult;
    DistributedAgingResult agingResult;
} PvmPacket;

void protocolClearPacket(PvmPacket* packet, DistributedMessageType type, int workerIndex);

#endif
