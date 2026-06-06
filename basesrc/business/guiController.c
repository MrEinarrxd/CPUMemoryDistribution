#include "guiController.h"
#include "../presentation/consoleIo.h"
#include "../presentation/uiTexts.h"
#include "../domain/core/ioQueue.h"
#include "../domain/core/readyQueue.h"
#include <stdio.h>
#include <string.h>

#define ColorReset "\033[0m"
#define ColorBold "\033[1m"
#define ColorDim "\033[2m"
#define ColorCyan "\033[36m"
#define ColorGreen "\033[32m"
#define ColorYellow "\033[33m"
#define ColorRed "\033[31m"
#define ColorBlue "\033[34m"

static void printLine(const char* text) {
    consoleIoPrintLine(text);
}

static void printBar(const char* label, float ratio, const char* color) {
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;
    int width = 24;
    int filled = (int)(ratio * width);
    int percent = (int)(ratio * 100.0f);

    printf("%-24s %s[", label, color);
    for (int i = 0; i < width; i++) printf("%c", i < filled ? '#' : '.');
    printf("]%s %3d%%\n", ColorReset, percent);
    fflush(stdout);
}

static void printMetric(const char* label, long value) {
    printf("  %-28s %ld\n", label, value);
    fflush(stdout);
}

static void printMetricFloat(const char* label, float value) {
    printf("  %-28s %.2f\n", label, value);
    fflush(stdout);
}

static int getIoWaitingCount(const ProcessTable* table) {
    if (!table || !table->ioQueue) return 0;
    int total = 0;
    for (int i = 0; i < numColasEs; i++) total += table->ioQueue->devices[i].size;
    return total;
}

static int readInt(void) {
    int value = 0;
    consoleIoSetNormalMode();
    if (scanf("%d", &value) != 1) {
        consoleIoSetRawMode();
        return 0;
    }
    int ch = 0;
    while ((ch = getchar()) != '\n' && ch != EOF) {}
    consoleIoSetRawMode();
    return value;
}

int guiControllerShowMainMenu(void) {
    consoleIoClear();
    consoleIoPrintLine(ColorBold ColorCyan "╔════════════════════════════════════════════════════╗" ColorReset);
    consoleIoPrintLine(ColorBold ColorCyan "║        Simulador CPU - Memoria - PVM              ║" ColorReset);
    consoleIoPrintLine(ColorBold ColorCyan "╚════════════════════════════════════════════════════╝" ColorReset);
    consoleIoPrintLine("");
    consoleIoPrintLine("  1. Simulación completa con PVM real");
    consoleIoPrintLine("  2. Simulación con PVM simulado");
    consoleIoPrintLine("  3. Prueba PVM real");
    consoleIoPrintLine("");
    consoleIoPrint(UiSeleccioneOpcion);
    return readInt();
}

void guiControllerShowMain(void) {
    consoleIoPrintLine(ColorBold ColorBlue "Controles" ColorReset);
    consoleIoPrintLine("  X Cambiar algoritmo   A Ranking RR   P Pausar   Q Salir");
    consoleIoPrintSeparator();
}

void guiControllerShowAlgorithmOptions(void) {
    consoleIoPrintLine("Seleccione algoritmo:");
    consoleIoPrintLine("1. FCFS");
    consoleIoPrintLine("2. Round Robin");
    consoleIoPrintLine("0. Cancelar");
}

int guiControllerGetAlgorithmChoice(void) {
    consoleIoPrint("Opción: ");
    return readInt();
}

int guiControllerAskQuantum(void) {
    consoleIoPrint("Ingrese quantum: ");
    return readInt();
}

void guiControllerShowQuantumPrompt(void) {
    consoleIoPrintLine("Ingrese el quantum para RR:");
}

void guiControllerShowBalanceAlert(float proportionReady, float proportionWaiting, int newQuantum) {
    consoleIoPrintSeparator();
    consoleIoPrintLine("*** ALERTA DE BALANCEO ***");
    consoleIoPrintFloat("Proporción Listos: ", proportionReady);
    consoleIoPrintFloat("Proporción Espera: ", proportionWaiting);
    consoleIoPrintInt("Nuevo Quantum: ", newQuantum);
    consoleIoPrintSeparator();
}

void guiControllerShowTop5Aged(const char ids[][idProcesoLen], const int wasteValues[], int count) {
    consoleIoPrintLine("=== TOP 5 PROCESOS MÁS ENVEJECIDOS ===");
    for (int i = 0; i < count; i++) {
        consoleIoPrintInt(ids[i], wasteValues[i]);
    }
}

void guiControllerShowTop5Wasters(const char ids[][idProcesoLen], const int wasteValues[], int count) {
    consoleIoPrintLine("=== TOP 5 PROCESOS CON MAYOR DESPERDICIO ===");
    for (int i = 0; i < count; i++) {
        consoleIoPrintInt(ids[i], wasteValues[i]);
    }
}

int guiControllerGetPrivilegedProcessId(char* outId, int maxLen) {
    if (!outId || maxLen <= 0) return -1;
    consoleIoPrint("Ingrese ID del proceso a privilegiar: ");
    consoleIoSetNormalMode();
    if (!fgets(outId, maxLen, stdin)) {
        consoleIoSetRawMode();
        return -1;
    }
    consoleIoSetRawMode();
    outId[strcspn(outId, "\n")] = '\0';
    return (int)strlen(outId);
}

void guiControllerShowPerformanceBars(const PerformanceBar* bar) {
    if (!bar) return;
    consoleIoPrintLine(ColorBold ColorBlue "Historial CPU" ColorReset);
    for (int i = 0; i < bar->valueCount; i++) {
        char label[40];
        snprintf(label, sizeof(label), "Aprovechamiento #%d", i + 1);
        printBar(label, bar->values[i], ColorGreen);
    }
    for (int i = 0; i < bar->valueCount; i++) {
        char label[40];
        snprintf(label, sizeof(label), "Desperdicio #%d", i + 1);
        printBar(label, bar->wasteValues[i], ColorYellow);
    }
}

void guiControllerShowMemoryStats(const StatsCollector* collector) {
    if (!collector) return;
    consoleIoPrintLine(ColorBold ColorBlue "Memoria" ColorReset);
    float usedRatio = maxMarcos > 0 ? (float)collector->memoryUsedBlocks / (float)maxMarcos : 0.0f;
    printBar("Uso de marcos", usedRatio, ColorCyan);
    printMetric("Marcos usados", collector->memoryUsedBlocks);
    printMetric("Marcos libres", collector->memoryFreeBlocks);
    printMetric("Mayor hueco libre", collector->largestFreeRun);
    printMetric("Huecos libres", collector->freeRunCount);
    printMetric("Desperdicio interno", collector->internalWaste);
    printMetric("Desperdicio externo", collector->externalWaste);
    printMetric("Fallos de página", collector->totalPageFaults);
    printMetricFloat("Fragmentación externa %", collector->fragmentation * 100.0f);
}

void guiControllerShowDashboard(const char* modeName,
                                const ProcessTable* table,
                                const StatsCollector* collector,
                                const PerformanceBar* bar,
                                SchedulerAlgorithm algorithm,
                                int currentQuantum) {
    if (!table || !collector) return;

    const char* algorithmName = algorithm == SchedulerAlgorithmRr ? "Round Robin" : "FCFS";
    int readyCount = table->readyQueue ? readyQueueGetCount(table->readyQueue) : 0;
    int ioWaiting = getIoWaitingCount(table);
    int nextPvmCycle = 20 - (table->currentCycle % 20);
    float finishedRatio = table->totalProcesses > 0
        ? (float)table->finishedProcesses / (float)table->totalProcesses : 0.0f;

    consoleIoClear();
    printf(ColorBold ColorCyan "╔════════════════════════════════════════════════════════════════════╗\n" ColorReset);
    printf(ColorBold ColorCyan "║              CPUMemoryDistribution Simulator                     ║\n" ColorReset);
    printf(ColorBold ColorCyan "╚════════════════════════════════════════════════════════════════════╝\n" ColorReset);
    printf("%sModo:%s %s   %sAlgoritmo:%s %s   %sQuantum:%s %d\n",
           ColorBold, ColorReset, modeName ? modeName : "Simulación",
           ColorBold, ColorReset, algorithmName,
           ColorBold, ColorReset, currentQuantum);
    printf("%sPVM:%s análisis distribuido cada 20 ciclos | próximo en %d ciclo(s)\n",
           ColorBold, ColorReset, nextPvmCycle);
    printf("%sControles:%s X cambiar algoritmo | A ranking RR | P pausar | Q salir\n\n",
           ColorBold, ColorReset);

    consoleIoPrintLine(ColorBold ColorBlue "Procesos y CPU" ColorReset);
    printBar("Progreso finalizados", finishedRatio, ColorGreen);
    printBar("Aprovechamiento CPU", collector->cpuUtilization, ColorGreen);
    printBar("Desperdicio CPU", collector->cpuWasteRatio, collector->cpuWasteRatio > 0.45f ? ColorRed : ColorYellow);
    printMetric("Ciclo actual", table->currentCycle);
    printMetric("Procesos creados", table->totalProcesses);
    printMetric("Procesos finalizados", table->finishedProcesses);
    printMetric("Procesos en ejecución", collector->processesRunning);
    printMetric("Cola de listos", readyCount);
    printMetric("Procesos en E/S", ioWaiting);
    printMetric("Operaciones E/S", collector->totalIoOperations);
    printMetric("Cambios de contexto", collector->totalContextSwitches);
    printMetric("Cambios de algoritmo", collector->algorithmChanges);
    printMetricFloat("Espera promedio", collector->avgWaitingTime);
    printMetricFloat("Ejecución promedio", collector->avgTimeInExecution);

    consoleIoPrintLine("");
    guiControllerShowMemoryStats(collector);

    if (bar && bar->valueCount > 0) {
        consoleIoPrintLine("");
        guiControllerShowPerformanceBars(bar);
    }

    consoleIoPrintLine("");
    printLine(ColorDim "La pantalla se actualiza automáticamente. Use los controles de una tecla sin presionar Enter." ColorReset);
}

int guiControllerReadCommand(void) {
    return consoleIoKbhit() ? consoleIoGetChar() : 0;
}
