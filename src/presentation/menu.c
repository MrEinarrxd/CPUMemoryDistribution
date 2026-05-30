#include "menu.h"
#include "consoleIo.h"
#include "../domain/stats/performanceBar.h"
#include "../domain/stats/statsCollector.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int menuGetExecutionMode(void) {
    consoleIoClear();
    consoleIoPrintLine("=== CPUMemoryDistribution Simulator ===");
    consoleIoPrintLine("Seleccione modo de ejecucion:");
    consoleIoPrintLine("1. LOCAL - sin PVM");
    consoleIoPrintLine("2. DISTRIBUIDO - con PVM");
    consoleIoPrint("Opcion: ");

    char buffer[16];
    if (!consoleIoReadLine(buffer, sizeof(buffer))) return 1;
    return (buffer[0] == '2') ? 2 : 1;
}

void menuShowMain(void) {
    consoleIoClear();
    consoleIoPrintLine("=== SIMULADOR DE PLANIFICACIÓN CPU - MEMORIA ===");
    consoleIoPrintLine("[X] Cambiar algoritmo (FCFS/RR)");
    consoleIoPrintLine("[A] Mostrar ranking envejecimiento (solo RR)");
    consoleIoPrintLine("[P] Pausar simulación");
    consoleIoPrintLine("[Q] Salir");
    consoleIoPrintSeparator();
}
void menuShowAlgorithmOptions(void) {
    consoleIoPrintLine("Seleccione algoritmo:");
    consoleIoPrintLine("1. FCFS");
    consoleIoPrintLine("2. Round Robin");
    consoleIoPrintLine("0. Cancelar");
}
int menuGetAlgorithmChoice(void) {
    char c = consoleIoGetChar();
    if (c == '1') return 1;
    if (c == '2') return 2;
    return 0;
}
int menuGetQuantumInput(void) {
    consoleIoPrint("Ingrese quantum: ");
    char buffer[32];
    if (!consoleIoReadLine(buffer, sizeof(buffer))) return 0;
    return atoi(buffer);
}
void menuShowQuantumPrompt(void) { consoleIoPrintLine("Ingrese el quantum para RR:"); }
void menuShowBalanceAlert(float proportionReady, float proportionWaiting, int newQuantum) {
    consoleIoPrintSeparator();
    consoleIoPrintLine("*** ALERTA DE BALANCEO ***");
    consoleIoPrintFloat("Proporción Listos: ", proportionReady);
    consoleIoPrintFloat("Proporción Espera: ", proportionWaiting);
    consoleIoPrintInt("Nuevo Quantum: ", newQuantum);
    consoleIoPrintSeparator();
}
void menuShowTop5Aged(const char ids[][idProcesoLen], const int wasteValues[], int count) {
    consoleIoPrintLine("=== TOP 5 PROCESOS MÁS ENVEJECIDOS ===");
    for (int i = 0; i < count && i < totalRankingProcesos; i++)
        consoleIoPrintInt(ids[i], wasteValues[i]);
}
void menuShowTop5Wasters(const char ids[][idProcesoLen], const int wasteValues[], int count) {
    consoleIoPrintLine("=== TOP 5 PROCESOS CON MAYOR DESPERDICIO ===");
    for (int i = 0; i < count && i < totalRankingProcesos; i++)
        consoleIoPrintInt(ids[i], wasteValues[i]);
}
int menuGetPrivilegedProcessId(char* outId, int maxLen) {
    consoleIoPrint("Ingrese ID del proceso a privilegiar: ");
    if (consoleIoReadLine(outId, maxLen)) {
        size_t len = strlen(outId);
        if (len > 0 && outId[len-1] == '\n') outId[len-1] = '\0';
        return 1;
    }
    return 0;
}
void menuShowPvmResults(void) { consoleIoPrintLine("Resultados PVM no disponibles aún."); }
void menuShowPerformanceBars(const struct PerformanceBar* bar) {
    if (!bar) return;
    consoleIoPrintLine("=== APROVECHAMIENTO DE CPU (últimas 5 mediciones) ===");
    for (int i = 0; i < bar->valueCount; i++) {
        int percent = (int)(bar->values[i] * 100);
        consoleIoPrintInt("Aprovech.%: ", percent);
        printf("[");
        for (int p = 0; p < percent / 5; p++) printf("#");
        printf("]\n");
    }
    consoleIoPrintLine("=== DESPERDICIO DE CPU (últimas 5 mediciones) ===");
    for (int i = 0; i < bar->valueCount; i++) {
        int percent = (int)(bar->wasteValues[i] * 100);
        consoleIoPrintInt("Desperdicio%: ", percent);
        printf("[");
        for (int p = 0; p < percent / 5; p++) printf("~");
        printf("]\n");
    }
}
void menuShowMemoryStats(const struct StatsCollector* collector) {
    if (!collector) return;
    consoleIoPrintLine("=== ESTADÍSTICAS DE MEMORIA ===");
    consoleIoPrintInt("Marcos usados: ", collector->memoryUsedBlocks);
    consoleIoPrintInt("Marcos libres: ", collector->memoryFreeBlocks);
    consoleIoPrintInt("Mayor hueco libre: ", collector->largestFreeRun);
    consoleIoPrintInt("Huecos libres: ", collector->freeRunCount);
    consoleIoPrintInt("Desperdicio interno: ", collector->internalWaste);
    consoleIoPrintInt("Desperdicio externo: ", collector->externalWaste);
    consoleIoPrintFloat("Fragmentación externa %: ", collector->fragmentation * 100.0f);
    consoleIoPrintInt("Fallos de página: ", collector->totalPageFaults);
    consoleIoPrintInt("Procesos en ejecución: ", collector->processesRunning);
    consoleIoPrintFloat("Finalizados/ciclo: ", collector->avgProcessesFinishedPerCycle);
    consoleIoPrintFloat("Tiempo prom. ejecución: ", collector->avgTimeInExecution);
}

//Parte 4 – Dominio: Procesos
