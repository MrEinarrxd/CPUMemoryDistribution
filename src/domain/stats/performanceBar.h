// === src/domain/stats/performanceBar.h ===

#ifndef PERFORMANCE_BAR_H
#define PERFORMANCE_BAR_H

#include "../../utils/constants.h"

struct StatsCollector;

typedef struct PerformanceBar {
    float values[barrasHistorial];
    int valueCount;
    float currentValue;
    float minValue;
    float maxValue;
} PerformanceBar;

PerformanceBar* performanceBarCreate(void);

void performanceBarDestroy(PerformanceBar* bar);

void performanceBarAddValue(PerformanceBar* bar, float value);

void performanceBarUpdate(PerformanceBar* bar, struct StatsCollector* collector);

float performanceBarGetCurrent(PerformanceBar* bar);

float performanceBarGetAverage(PerformanceBar* bar);

void performanceBarReset(PerformanceBar* bar);

#endif
