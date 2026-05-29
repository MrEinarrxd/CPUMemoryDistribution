#include "performanceBar.h"
#include "statsCollector.h"
#include <stdlib.h>

PerformanceBar* performanceBarCreate(void) {
    PerformanceBar* pb = (PerformanceBar*)calloc(1, sizeof(PerformanceBar));
    if (pb) {
        pb->valueCount = 0;
        pb->currentValue = 0.0f;
        pb->minValue = 0.0f;
        pb->maxValue = 0.0f;
    }
    return pb;
}
void performanceBarDestroy(PerformanceBar* bar) { free(bar); }
void performanceBarAddValue(PerformanceBar* bar, float value) {
    if (!bar) return;
    if (bar->valueCount < barrasHistorial) {
        bar->values[bar->valueCount] = value;
        bar->valueCount++;
    } else {
        for (int i = 1; i < barrasHistorial; i++)
            bar->values[i-1] = bar->values[i];
        bar->values[barrasHistorial-1] = value;
    }
    bar->currentValue = value;
}
void performanceBarUpdate(PerformanceBar* bar, StatsCollector* collector) {
    if (!bar || !collector) return;
    float utilization = statsCollectorGetCpuUtilization(collector);
    float waste = collector->cpuWasteRatio;
    if (utilization < 0.0f) utilization = 0.0f;
    if (utilization > 1.0f) utilization = 1.0f;
    if (waste < 0.0f) waste = 0.0f;
    if (waste > 1.0f) waste = 1.0f;
    // Actualizar values y wasteValues sincrónicamente con un solo contador
    if (bar->valueCount < barrasHistorial) {
        bar->values[bar->valueCount] = utilization;
        bar->wasteValues[bar->valueCount] = waste;
        bar->valueCount++;
    } else {
        for (int i = 1; i < barrasHistorial; i++) {
            bar->values[i-1] = bar->values[i];
            bar->wasteValues[i-1] = bar->wasteValues[i];
        }
        bar->values[barrasHistorial-1] = utilization;
        bar->wasteValues[barrasHistorial-1] = waste;
    }
    bar->currentValue = utilization;
}
float performanceBarGetCurrent(PerformanceBar* bar) { return bar ? bar->currentValue : 0.0f; }
float performanceBarGetAverage(PerformanceBar* bar) {
    if (!bar || bar->valueCount == 0) return 0.0f;
    float sum = 0;
    for (int i = 0; i < bar->valueCount; i++) sum += bar->values[i];
    return sum / bar->valueCount;
}
void performanceBarReset(PerformanceBar* bar) { if (bar) bar->valueCount = 0; }
