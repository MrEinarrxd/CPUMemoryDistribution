#include "guiController.h"
#include "../presentation/consoleIo.h"
#include "../utils/constants.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#define ScreenWidth 86
#define LabelWidth 18
#define BarWidth 24

static int readIntPrompt(const char* prompt) {
    int value = 0;
    int ch;
    consoleIoSetNormalMode();
    printf("%s", prompt ? prompt : "");
    fflush(stdout);
    if (scanf("%d", &value) != 1) value = 0;
    while ((ch = getchar()) != '\n' && ch != EOF) {}
    consoleIoSetRawMode();
    return value;
}

static float clampRatio(float value) {
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

static void appendText(char* buffer, size_t bufferSize, size_t* used, const char* format, ...) {
    va_list args;
    int written;

    if (!buffer || !used || *used >= bufferSize) return;
    va_start(args, format);
    written = vsnprintf(buffer + *used, bufferSize - *used, format, args);
    va_end(args);
    if (written > 0) *used += (size_t)written;
}

static void appendLine(char* buffer, size_t bufferSize, size_t* used, const char* line) {
    appendText(buffer, bufferSize, used, "%s\n", line ? line : "");
}

static void appendRule(char* buffer, size_t bufferSize, size_t* used, char character) {
    char line[ScreenWidth + 1];
    for (int i = 0; i < ScreenWidth; ++i) line[i] = character;
    line[ScreenWidth] = '\0';
    appendLine(buffer, bufferSize, used, line);
}

static void makeBar(char* out, size_t outSize, float ratio) {
    int filled;
    int percent;
    size_t used = 0;

    if (!out || outSize == 0) return;
    ratio = clampRatio(ratio);
    filled = (int)(ratio * (float)BarWidth + 0.5f);
    percent = (int)(ratio * 100.0f + 0.5f);

    used += snprintf(out + used, outSize > used ? outSize - used : 0, "[");
    for (int i = 0; i < BarWidth; ++i) {
        used += snprintf(out + used, outSize > used ? outSize - used : 0, "%c", i < filled ? '#' : '.');
    }
    snprintf(out + used, outSize > used ? outSize - used : 0, "] %3d%%", percent);
}

static int safePercent(int value, int total) {
    if (total <= 0) return 0;
    return (value * 100) / total;
}

static const char* algorithmKind(const SimulationSnapshot* snapshot) {
    if (!snapshot || !snapshot->algorithmName) return "--";
    return snapshot->algorithmName;
}

static int isRoundRobin(const SimulationSnapshot* snapshot) {
    const char* name = algorithmKind(snapshot);
    return strstr(name, "Round") != NULL || strstr(name, "Robin") != NULL || strstr(name, "RR") != NULL;
}

static float lastHistoryOrCurrent(const SimulationSnapshot* snapshot, int reverseIndex) {
    int index;
    if (!snapshot) return 0.0f;
    if (snapshot->historyCount <= reverseIndex) {
        return reverseIndex == 0 ? clampRatio(snapshot->cpuUtilization) : 0.0f;
    }
    index = snapshot->historyCount - 1 - reverseIndex;
    return clampRatio(snapshot->utilizationHistory[index]);
}

static void appendMetric(char* buffer, size_t bufferSize, size_t* used,
                         const char* label, const char* value) {
    appendText(buffer, bufferSize, used, "  %-*s %s\n", LabelWidth, label ? label : "", value ? value : "");
}

static void appendHeader(const SimulationSnapshot* snapshot, char* buffer, size_t bufferSize, size_t* used) {
    appendRule(buffer, bufferSize, used, '=');
    appendText(buffer, bufferSize, used,
               " CPU-MEM-DIST | %-13s | Q=%-3d | t=%-7d | iter=%d\n",
               algorithmKind(snapshot),
               snapshot->currentQuantum,
               snapshot->currentTime,
               snapshot->cpuIterations);
    appendRule(buffer, bufferSize, used, '=');
}

static void appendResumen(const SimulationSnapshot* snapshot, char* buffer, size_t bufferSize, size_t* used) {
    char value[160];
    char bar[80];
    int totalKnown;

    appendLine(buffer, bufferSize, used, "RESUMEN GENERAL");
    totalKnown = snapshot->finishedCount + snapshot->activeCount + snapshot->newCount;
    if (totalKnown <= 0) totalKnown = TotalProcesses;

    snprintf(value, sizeof(value), "%d/%d terminados | %d activos | %d nuevos",
             snapshot->finishedCount, TotalProcesses, snapshot->activeCount, snapshot->newCount);
    appendMetric(buffer, bufferSize, used, "Procesos", value);

    snprintf(value, sizeof(value), "%d listos | %d en E/S", snapshot->readyCount, snapshot->ioCount);
    appendMetric(buffer, bufferSize, used, "Colas", value);

    snprintf(value, sizeof(value), "%s | %s", snapshot->modeName ? snapshot->modeName : "--",
             snapshot->pvmStatus ? snapshot->pvmStatus : "--");
    appendMetric(buffer, bufferSize, used, "PVM", value);

    makeBar(bar, sizeof(bar), (float)snapshot->finishedCount / (float)TotalProcesses);
    appendMetric(buffer, bufferSize, used, "Avance", bar);
}

static void appendCpu(const SimulationSnapshot* snapshot, char* buffer, size_t bufferSize, size_t* used) {
    char value[160];
    char bar[80];

    appendRule(buffer, bufferSize, used, '-');
    appendLine(buffer, bufferSize, used, "CPU");

    makeBar(bar, sizeof(bar), snapshot->cpuUtilization);
    appendMetric(buffer, bufferSize, used, "Uso CPU", bar);

    if (isRoundRobin(snapshot)) {
        makeBar(bar, sizeof(bar), snapshot->cpuWasteRatio);
        appendMetric(buffer, bufferSize, used, "Desperdicio RR", bar);
    } else {
        appendMetric(buffer, bufferSize, used, "Desperdicio RR", "no aplica en FCFS");
    }

    snprintf(value, sizeof(value), "espera %.0f ciclos | ejecucion %.0f ciclos",
             snapshot->avgWaitingTime, snapshot->avgExecutionTime);
    appendMetric(buffer, bufferSize, used, "Promedios", value);

    appendText(buffer, bufferSize, used, "  %-*s ", LabelWidth, "Historial");
    for (int i = 0; i < HistoryBars; ++i) {
        appendText(buffer, bufferSize, used, "T-%d:%02d%% ", i,
                   (int)(lastHistoryOrCurrent(snapshot, i) * 100.0f + 0.5f));
    }
    appendText(buffer, bufferSize, used, "\n");
}

static void appendMemoria(const SimulationSnapshot* snapshot, char* buffer, size_t bufferSize, size_t* used) {
    char value[160];
    char bar[80];
    int totalFrames = snapshot->memoryUsedFrames + snapshot->memoryFreeFrames;
    if (totalFrames <= 0) totalFrames = PhysicalFrameCount;

    appendRule(buffer, bufferSize, used, '-');
    appendLine(buffer, bufferSize, used, "MEMORIA");

    makeBar(bar, sizeof(bar), (float)snapshot->memoryUsedFrames / (float)totalFrames);
    appendMetric(buffer, bufferSize, used, "Marcos usados", bar);

    snprintf(value, sizeof(value), "%d usados | %d libres | bloque libre mayor: %d",
             snapshot->memoryUsedFrames, snapshot->memoryFreeFrames, snapshot->memoryLargestFreeRun);
    appendMetric(buffer, bufferSize, used, "Detalle", value);

    snprintf(value, sizeof(value), "interno %d | externo %d | fragmentacion %.2f%%",
             snapshot->internalWaste, snapshot->externalWaste, snapshot->fragmentation);
    appendMetric(buffer, bufferSize, used, "Desperdicio", value);

    snprintf(value, sizeof(value), "%d fallos | swap in %d | swap out %d",
             snapshot->totalPageFaults, snapshot->totalSwapIns, snapshot->totalSwapOuts);
    appendMetric(buffer, bufferSize, used, "Paginacion", value);
}

static void appendRankingLine(char* buffer, size_t bufferSize, size_t* used,
                              int number, const RankingEntry* aged, const RankingEntry* waster,
                              const SimulationSnapshot* snapshot) {
    char left[42];
    char right[42];

    if (aged) {
        snprintf(left, sizeof(left), "%d) %-8s pendientes=%d", number, aged->processId, aged->secondary);
    } else {
        snprintf(left, sizeof(left), "%d) --", number);
    }

    if (waster) {
        int usedPercent = snapshot->currentQuantum > 0
            ? safePercent(waster->secondary, snapshot->currentQuantum)
            : 0;
        if (usedPercent > 100) usedPercent = 100;
        snprintf(right, sizeof(right), "%d) %-8s usa=%d%%", number, waster->processId, usedPercent);
    } else {
        snprintf(right, sizeof(right), "%d) --", number);
    }

    appendText(buffer, bufferSize, used, "  %-40s | %-40s\n", left, right);
}

static void appendRoundRobin(const SimulationSnapshot* snapshot, char* buffer, size_t bufferSize, size_t* used) {
    appendRule(buffer, bufferSize, used, '-');
    appendLine(buffer, bufferSize, used, "ROUND ROBIN");

    if (!isRoundRobin(snapshot)) {
        appendLine(buffer, bufferSize, used,
                   "  FCFS esta activo. Los rankings RR se ocultan para no mezclar datos que no aplican.");
        appendLine(buffer, bufferSize, used,
                   "  Presione X y seleccione Round Robin para ver envejecimiento y desperdicio real.");
        return;
    }

    appendLine(buffer, bufferSize, used,
               "  Mas envejecidos                          | Mayor desperdicio");
    for (int i = 0; i < TopRankingCount; ++i) {
        const RankingEntry* aged = i < snapshot->topAgedCount ? &snapshot->topAged[i] : NULL;
        const RankingEntry* waster = i < snapshot->topWastersCount ? &snapshot->topWasters[i] : NULL;
        appendRankingLine(buffer, bufferSize, used, i + 1, aged, waster, snapshot);
    }
}

static void appendPvmLog(const SimulationSnapshot* snapshot, char* buffer, size_t bufferSize, size_t* used) {
    appendRule(buffer, bufferSize, used, '-');
    appendLine(buffer, bufferSize, used, "PVM / EVENTOS");

    if (snapshot->eventLog[0][0] != '\0') appendText(buffer, bufferSize, used, "  %s\n", snapshot->eventLog[0]);
    if (snapshot->eventLog[1][0] != '\0') appendText(buffer, bufferSize, used, "  %s\n", snapshot->eventLog[1]);
    if (snapshot->eventLog[2][0] != '\0') appendText(buffer, bufferSize, used, "  %s\n", snapshot->eventLog[2]);
    if (snapshot->eventLog[3][0] != '\0') appendText(buffer, bufferSize, used, "  %s\n", snapshot->eventLog[3]);
    if (snapshot->eventLog[4][0] != '\0') appendText(buffer, bufferSize, used, "  %s\n", snapshot->eventLog[4]);
}

static void buildDashboardBuffer(const SimulationSnapshot* snapshot, char* buffer, size_t bufferSize) {
    size_t used = 0;
    if (!snapshot || !buffer || bufferSize == 0) return;
    buffer[0] = '\0';

    appendHeader(snapshot, buffer, bufferSize, &used);
    appendResumen(snapshot, buffer, bufferSize, &used);
    appendCpu(snapshot, buffer, bufferSize, &used);
    appendMemoria(snapshot, buffer, bufferSize, &used);
    appendRoundRobin(snapshot, buffer, bufferSize, &used);
    appendPvmLog(snapshot, buffer, bufferSize, &used);
    appendRule(buffer, bufferSize, &used, '=');
    appendLine(buffer, bufferSize, &used,
               "CONTROLES: X cambiar algoritmo | A privilegiar proceso RR | P pausa | Q salir");
    appendLine(buffer, bufferSize, &used,
               "Lectura sugerida: RESUMEN -> CPU -> MEMORIA. RR solo aparece cuando RR esta activo.");
    appendRule(buffer, bufferSize, &used, '=');
}

void guiControllerShowDashboard(const SimulationSnapshot* snapshot) {
    char* buffer = (char*)malloc(16384);
    if (!buffer) return;
    buildDashboardBuffer(snapshot, buffer, 16384);
    consoleIoClear();
    fputs(buffer, stdout);
    fflush(stdout);
    free(buffer);
}

int guiControllerShowMainMenu(void) {
    int option = 0;
    int ch;
    consoleIoClear();
    printf("CPU-MEM-DIST Scheduler\n");
    printf("----------------------\n\n");
    printf("  1. Simulacion con PVM real\n");
    printf("  2. Simulacion con PVM local\n");
    printf("  3. Simulacion con PVM desactivado\n");
    printf("  4. Probar comunicacion PVM\n");
    printf("  0. Salir\n\n");
    printf("Seleccione opcion: ");
    fflush(stdout);
    if (scanf("%d", &option) != 1) option = 0;
    while ((ch = getchar()) != '\n' && ch != EOF) {}
    return option;
}

int guiControllerReadCommand(void) {
    return consoleIoKbhit() ? (int)consoleIoGetChar() : 0;
}

int guiControllerAskAlgorithm(void) {
    consoleIoClear();
    printf("Seleccione algoritmo:\n\n");
    printf("  1. FCFS\n");
    printf("  2. Round Robin\n");
    printf("  0. Cancelar\n\n");
    return readIntPrompt("Opcion: ");
}

int guiControllerAskQuantum(void) {
    return readIntPrompt("Ingrese quantum: ");
}

int guiControllerAskProcessId(char* outProcessId, int maxLen) {
    if (!outProcessId || maxLen <= 0) return -1;
    consoleIoSetNormalMode();
    printf("Ingrese ID del proceso a privilegiar: ");
    fflush(stdout);
    if (!fgets(outProcessId, maxLen, stdin)) {
        outProcessId[0] = '\0';
        consoleIoSetRawMode();
        return -1;
    }
    outProcessId[strcspn(outProcessId, "\n")] = '\0';
    consoleIoSetRawMode();
    return (int)strlen(outProcessId);
}

void guiControllerShowRankings(const Scheduler* scheduler) {
    if (!scheduler) return;
    consoleIoClear();
    printf("TOP 5 ROUND ROBIN\n");
    printf("-----------------\n\n");
    printf("Procesos mas envejecidos:\n");
    for (int i = 0; i < scheduler->topAgedCount; ++i) {
        printf("  %d. %-8s retornos=%d pendientes=%d\n",
               i + 1,
               scheduler->topAged[i].processId,
               scheduler->topAged[i].primary,
               scheduler->topAged[i].secondary);
    }
    printf("\nProcesos con mayor desperdicio:\n");
    for (int i = 0; i < scheduler->topWastersCount; ++i) {
        printf("  %d. %-8s desperdicio=%d usoQuantum=%d\n",
               i + 1,
               scheduler->topWasters[i].processId,
               scheduler->topWasters[i].primary,
               scheduler->topWasters[i].secondary);
    }
    printf("\n");
}

void guiControllerShowPauseMessage(void) {
    consoleIoPrintLine("[PAUSADO] Presione P para reanudar...");
}

void guiControllerShowResumeMessage(void) {
    consoleIoPrintLine("[REANUDADO]");
}
