// === src/domain/stats/statsCollector.h ===

#ifndef STATS_COLLECTOR_H
#define STATS_COLLECTOR_H

#include "../../utils/constants.h"

struct ProcessTable;

typedef struct StatsCollector {
    long totalCpuCyclesExecuted;
    float cpuUtilization;
    long totalMemoryAllocated;
    int internalWaste;
    int externalWaste;
    int totalPageFaults;
    float fragmentation;
    int totalContextSwitches;                           // Total de cambios de contexto
    float avgWaitingTime;                               // Tiempo promedio de espera
    float avgTurnaroundTime;                            // Tiempo promedio de respuesta
    int totalIoOperations;                              // Total de operaciones de E/S
    int algorithmChanges;                               // Número de cambios de algoritmo
    int processesFinished;                              // Procesos que finalizaron
    float avgProcessesFinishedPerCycle;                 // Promedio de procesos finalizados por ciclo
    int processesRunning;              // Cantidad de procesos en ejecución actualmente
    float avgTimeInExecution;          // Tiempo promedio en ejecución por proceso
} StatsCollector;

StatsCollector* statsCollectorCreate(void);

void statsCollectorDestroy(StatsCollector* collector);

void statsCollectorCollect(StatsCollector* collector, struct ProcessTable* table);

void statsCollectorRecordCpuCycle(StatsCollector* collector);

void statsCollectorRecordPageFault(StatsCollector* collector);

void statsCollectorRecordMemoryAllocation(StatsCollector* collector, int size);

void statsCollectorUpdateStatistics(StatsCollector* collector, struct ProcessTable* table);

float statsCollectorGetCpuUtilization(StatsCollector* collector);

#endif
