
#ifndef GUI_H
#define GUI_H

#include "../domain/distributed/messageProtocol.h"
#include "../domain/stats/performanceBar.h"

void guiShowMainMenu(void);
void guiShowStatistics(const DistributedStats* stats, const PerformanceBar* bar);
void guiShowAgingResults(const AgingResults* results);
void guiShowMessage(const char* message);
int guiGetChar(void);

#endif