// === src/domain/process/processTable.h ===

#ifndef PROCESS_TABLE_H
#define PROCESS_TABLE_H

#include "../../utils/constants.h"
#include "process.h"

struct ReadyQueue;
struct IoQueue;

// Propietario exclusivo de Process arrays
// Solo referencia a ReadyQueue e IoQueue
typedef struct ProcessTable {
    Process* runningProcesses[procesosEnEjecucion];
    Process* newRequests[procesosEnEspera];
    int totalProcesses;
    int finishedProcesses;
    struct ReadyQueue* readyQueue;
    struct IoQueue* ioQueue;
    float avgWaitingTime;
    float avgTurnaroundTime;
    int currentCycle;
    int simulationStartTime;
    int totalCpuCyclesExecuted;                         // Total de ciclos CPU ejecutados por todos los procesos
    int totalContextSwitches;                           // Total de cambios de contexto realizados
    int algorithmChangeCount;                           // Cantidad de veces que se cambió el algoritmo
    int totalIoOperations;                              // Total de operaciones de E/S realizadas
    float cpuUtilization;                               // Porcentaje de utilización de CPU
    int internalWaste;                                  // Fragmentación interna
    int externalWaste;                                  // Fragmentación externa
    int totalPageFaults;                                // Total de fallos de página ocurridos
    float fragmentation;                                // Nivel actual de fragmentación
    int quantumCurrent;                                 // Valor actual de quantum
    int quantumHistory[historialAlgoritmoMaximo];   // Historial de valores de Quantum
    float proportionReady;                              // Proporción actual lista/total
    float proportionWaiting;                            // Proporción actual en espera/total
    int iterationsSinceBalance;                         // Iteraciones desde el último balance
    int shouldSwitchAlgorithm;                          // Indicador: debe cambiar algoritmo
} ProcessTable;

ProcessTable* processTableCreate(void);

void processTableDestroy(ProcessTable* table);

int processTableAddRunning(ProcessTable* table, Process* process);

int processTableAddNew(ProcessTable* table, Process* process);

Process* processTableGetRunning(ProcessTable* table, int index);

Process* processTableGetNew(ProcessTable* table, int index);

void processTableRemoveRunning(ProcessTable* table, int index);

int processTableGetTotalProcesses(ProcessTable* table);

int processTableGetFinishedProcesses(ProcessTable* table);

void processTableIncrementFinished(ProcessTable* table);

void processTableUpdateAverages(ProcessTable* table);

#endif
