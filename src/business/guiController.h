#ifndef GUI_CONTROLLER_H
#define GUI_CONTROLLER_H

#include "../domain/core/performanceBar.h"
#include "../domain/core/statsCollector.h"
#include "../domain/core/processTable.h"
#include "../domain/core/scheduler.h"
#include "../utils/constants.h"

int guiControllerShowMainMenu(void);
void guiControllerShowMain(void);
void guiControllerShowAlgorithmOptions(void);
int guiControllerGetAlgorithmChoice(void);
int guiControllerAskQuantum(void);
void guiControllerShowQuantumPrompt(void);
void guiControllerShowBalanceAlert(float proportionReady, float proportionWaiting, int newQuantum);
void guiControllerShowTop5Aged(const char ids[][idProcesoLen], const int wasteValues[], int count);
void guiControllerShowTop5Wasters(const char ids[][idProcesoLen], const int wasteValues[], int count);
int guiControllerGetPrivilegedProcessId(char* outId, int maxLen);
void guiControllerShowPerformanceBars(const PerformanceBar* bar);
void guiControllerShowMemoryStats(const StatsCollector* collector);
void guiControllerShowDashboard(const char* modeName,
                                const ProcessTable* table,
                                const StatsCollector* collector,
                                const PerformanceBar* bar,
                                SchedulerAlgorithm algorithm,
                                int currentQuantum);
int guiControllerReadCommand(void);

#endif
