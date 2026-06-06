#ifndef CpuMemoryGuiControllerH
#define CpuMemoryGuiControllerH

#include "../domain/scheduler/scheduler.h"

typedef struct SimulationSnapshot {
    const char* modeName;
    const char* pvmStatus;
    const char* algorithmName;
    int currentQuantum;
    int currentTime;
    int cpuIterations;
    int activeCount;
    int newCount;
    int finishedCount;
    int readyCount;
    int ioCount;
    int totalContextSwitches;
    int totalIoOperations;
    int algorithmChanges;
    int memoryUsedFrames;
    int memoryFreeFrames;
    int memoryLargestFreeRun;
    int memoryFreeRunCount;
    int internalWaste;
    int externalWaste;
    int totalPageFaults;
    int totalSwapIns;
    int totalSwapOuts;
    float fragmentation;
    float avgWaitingTime;
    float avgExecutionTime;
    float avgFinishedPerTime;
    float cpuUtilization;
    float cpuWasteRatio;
    float utilizationHistory[HistoryBars];
    float wasteHistory[HistoryBars];
    int historyCount;
    RankingEntry topAged[TopRankingCount];
    RankingEntry topWasters[TopRankingCount];
    int topAgedCount;
    int topWastersCount;
    char eventLog[5][160];
} SimulationSnapshot;

int guiControllerShowMainMenu(void);
void guiControllerShowDashboard(const SimulationSnapshot* snapshot);
int guiControllerReadCommand(void);
int guiControllerAskAlgorithm(void);
int guiControllerAskQuantum(void);
int guiControllerAskProcessId(char* outProcessId, int maxLen);
void guiControllerShowRankings(const Scheduler* scheduler);
void guiControllerShowPauseMessage(void);
void guiControllerShowResumeMessage(void);

#endif
