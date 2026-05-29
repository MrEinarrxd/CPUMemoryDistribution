#include "statsCollector.h"
#include "../process/processTable.h"
#include <stdlib.h>

StatsCollector* statsCollectorCreate(void) {
    StatsCollector* sc = (StatsCollector*)calloc(1, sizeof(StatsCollector));
    return sc;
}
void statsCollectorDestroy(StatsCollector* collector) { free(collector); }

void statsCollectorCollect(StatsCollector* collector, ProcessTable* table) {
    if (!collector || !table) return;
    collector->totalCpuCyclesExecuted = table->totalCpuCyclesExecuted;
    collector->cpuUtilization = table->cpuUtilization;
    collector->totalContextSwitches = table->totalContextSwitches;
    collector->avgWaitingTime = table->avgWaitingTime;
    collector->avgTurnaroundTime = table->avgTurnaroundTime;
    collector->totalIoOperations = table->totalIoOperations;
    collector->algorithmChanges = table->algorithmChangeCount;
    collector->processesFinished = table->finishedProcesses;
    collector->internalWaste = table->internalWaste;
    collector->externalWaste = table->externalWaste;
    collector->totalPageFaults = table->totalPageFaults;
    collector->fragmentation = table->fragmentation;
    int cycles = table->currentCycle;
    if (cycles > 0) collector->avgProcessesFinishedPerCycle = (float)collector->processesFinished / cycles;
    int running = 0;
    for (int i = 0; i < procesosEnEjecucion; i++)
        if (table->runningProcesses[i] && table->runningProcesses[i]->bcp && table->runningProcesses[i]->bcp->state != ProcessStateFinished)
            running++;
    collector->processesRunning = running;
}

void statsCollectorRecordCpuCycle(StatsCollector* collector) { if (collector) collector->totalCpuCyclesExecuted++; }
void statsCollectorRecordPageFault(StatsCollector* collector) { if (collector) collector->totalPageFaults++; }
void statsCollectorRecordMemoryAllocation(StatsCollector* collector, int size) { (void)collector; (void)size; }
void statsCollectorUpdateStatistics(StatsCollector* collector, ProcessTable* table) { statsCollectorCollect(collector, table); }
float statsCollectorGetCpuUtilization(StatsCollector* collector) { return collector ? collector->cpuUtilization : 0.0f; }