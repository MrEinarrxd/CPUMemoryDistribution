#ifndef MENU_H
#define MENU_H

#include "../utils/constants.h"

struct PerformanceBar;
struct StatsCollector;

void menuShowMain(void);
int menuGetExecutionMode(void);
void menuShowAlgorithmOptions(void);
int menuGetAlgorithmChoice(void);
int menuGetQuantumInput(void);
void menuShowQuantumPrompt(void);
void menuShowBalanceAlert(float proportionReady, float proportionWaiting, int newQuantum);
void menuShowTop5Aged(const char ids[][idProcesoLen], const int wasteValues[], int count);
void menuShowTop5Wasters(const char ids[][idProcesoLen], const int wasteValues[], int count);
int menuGetPrivilegedProcessId(char* outId, int maxLen);
void menuShowPvmResults(void);
void menuShowPerformanceBars(const struct PerformanceBar* bar);
void menuShowMemoryStats(const struct StatsCollector* collector);

#endif
