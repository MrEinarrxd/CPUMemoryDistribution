#ifndef CpuMemoryProcessTableH
#define CpuMemoryProcessTableH

#include "bcp.h"
#include "ioQueue.h"
#include "readyQueue.h"
#include "../../distributed/protocol.h"
#include "../../utils/logger.h"

typedef struct ProcessTable {
    Bcp processes[TotalProcesses];
    int activeSlots[ActiveProcessCount];
    int newSlots[NewRequestCount];
    int activeCount;
    int newCount;
    int finishedCount;
    int currentTime;
    int cpuIterations;
    int totalCpuCyclesExecuted;
    int totalCpuWasteCycles;
    int totalContextSwitches;
    int totalContextSwitchTime;
    int totalIoOperations;
    int totalPageFaults;
    int totalSwapIns;
    int totalSwapOuts;
    int memoryUsedFrames;
    int memoryFreeFrames;
    int memoryLargestFreeRun;
    int memoryFreeRunCount;
    int internalWaste;
    int externalWaste;
    float fragmentation;
    float avgWaitingTime;
    float avgExecutionTime;
    float avgFinishedPerTime;
    float cpuUtilization;
    float readyProportion;
    float waitingProportion;
    int currentQuantum;
    int algorithmChanges;
    int resizeCount;
    ReadyQueue readyQueue;
    IoQueue ioQueue;
} ProcessTable;

void processTableInit(ProcessTable* table);
void processTablePromoteNew(ProcessTable* table);
void processTableFinishProcess(ProcessTable* table, int processIndex);
void processTableUpdateQueueMetrics(ProcessTable* table);
void processTableUpdateAverages(ProcessTable* table);
void processTableLogSnapshot(ProcessTable* table, Logger* logger);
void processTableLogBcps(ProcessTable* table, Logger* logger);
int processTableExportStatsRows(const ProcessTable* table, ProcessStatsRow rows[], int maxRows);
int processTableExportRrRows(const ProcessTable* table, RrAnalysisRow rows[], int maxRows);
int processTableFindById(ProcessTable* table, const char* processId);

#endif
