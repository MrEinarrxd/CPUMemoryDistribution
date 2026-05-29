#include "menu.h"
#include "consoleIo.h"
#include "../domain/stats/performanceBar.h"
#include "../domain/stats/statsCollector.h"
#include <stdio.h>
#include <string.h>

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
    char c = getchar(); while (getchar() != '\n');
    if (c == '1') return 1;
    if (c == '2') return 2;
    return 0;
}
int menuGetQuantumInput(void) {
    consoleIoPrint("Ingrese quantum: ");
    int q; scanf("%d", &q); while (getchar() != '\n');
    return q;
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
    if (fgets(outId, maxLen, stdin)) {
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
    consoleIoPrintInt("Desperdicio interno: ", collector->internalWaste);
    consoleIoPrintInt("Desperdicio externo: ", collector->externalWaste);
    consoleIoPrintFloat("Fragmentación: ", collector->fragmentation);
    consoleIoPrintInt("Fallos de página: ", collector->totalPageFaults);
}

//Parte 4 – Dominio: Procesos