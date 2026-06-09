#include "guiController.h"
#include "../presentation/consoleIo.h"
#include "../utils/constants.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ScreenWidth 78
#define LabelWidth 12
#define BarWidth 18
#define SmallBarWidth 12
#define DashboardBufferSize 20000

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

static int percentFromRatio(float ratio) {
    return (int)(clampRatio(ratio) * 100.0f + 0.5f);
}

static void appendText(char* buffer, size_t bufferSize, size_t* used, const char* format, ...) {
    va_list args;
    int written;

    if (!buffer || !used || *used >= bufferSize) return;
    va_start(args, format);
    written = vsnprintf(buffer + *used, bufferSize - *used, format, args);
    va_end(args);
    if (written <= 0) return;
    *used += (size_t)written;
    if (*used >= bufferSize) *used = bufferSize - 1;
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

static void appendSection(char* buffer, size_t bufferSize, size_t* used, const char* title) {
    appendRule(buffer, bufferSize, used, '-');
    appendText(buffer, bufferSize, used, "%s\n", title ? title : "");
}

static void makeBarWidth(char* out, size_t outSize, float ratio, int width) {
    int filled;
    int percent;
    size_t used = 0;

    if (!out || outSize == 0) return;
    if (width <= 0) width = BarWidth;
    ratio = clampRatio(ratio);
    filled = (int)(ratio * (float)width + 0.5f);
    percent = percentFromRatio(ratio);

    used += snprintf(out + used, outSize > used ? outSize - used : 0, "[");
    for (int i = 0; i < width; ++i) {
        used += snprintf(out + used, outSize > used ? outSize - used : 0, "%c",
                         i < filled ? '#' : '.');
    }
    snprintf(out + used, outSize > used ? outSize - used : 0, "] %3d%%", percent);
}

static void makeBar(char* out, size_t outSize, float ratio) {
    makeBarWidth(out, outSize, ratio, BarWidth);
}

static const char* algorithmKind(const SimulationSnapshot* snapshot) {
    if (!snapshot || !snapshot->algorithmName) return "--";
    return snapshot->algorithmName;
}

static int isRoundRobin(const SimulationSnapshot* snapshot) {
    const char* name = algorithmKind(snapshot);
    return strstr(name, "Round") != NULL || strstr(name, "Robin") != NULL ||
           strstr(name, "RR") != NULL;
}

static const char* rrState(const SimulationSnapshot* snapshot) {
    if (!snapshot) return "sin datos";
    if (isRoundRobin(snapshot)) return "activo";
    if (snapshot->rrProcessCount > 0 ||
        snapshot->topAgedCount > 0 ||
        snapshot->topWastersCount > 0) {
        return "datos acumulados";
    }
    return "sin datos";
}

static float lastHistoryOrCurrent(const SimulationSnapshot* snapshot, int reverseIndex, int useWaste) {
    int index;
    if (!snapshot) return 0.0f;
    if (snapshot->historyCount <= reverseIndex) {
        if (reverseIndex != 0) return 0.0f;
        return clampRatio(useWaste ? snapshot->cpuWasteRatio : snapshot->cpuUtilization);
    }
    index = snapshot->historyCount - 1 - reverseIndex;
    return clampRatio(useWaste ? snapshot->wasteHistory[index] : snapshot->utilizationHistory[index]);
}

static void appendMetric(char* buffer, size_t bufferSize, size_t* used,
                         const char* label, const char* value) {
    appendText(buffer, bufferSize, used, "  %-*s %s\n",
               LabelWidth, label ? label : "", value ? value : "");
}

static void appendPair(char* buffer, size_t bufferSize, size_t* used,
                       const char* leftLabel, const char* leftValue,
                       const char* rightLabel, const char* rightValue) {
    appendText(buffer, bufferSize, used, "  %-12s %-21.21s | %-12s %.24s\n",
               leftLabel ? leftLabel : "",
               leftValue ? leftValue : "",
               rightLabel ? rightLabel : "",
               rightValue ? rightValue : "");
}

static void appendHeader(const SimulationSnapshot* snapshot, char* buffer,
                         size_t bufferSize, size_t* used) {
    appendRule(buffer, bufferSize, used, '=');
    appendText(buffer, bufferSize, used,
               " CPU-MEM-DIST | Alg=%-12.12s | Q=%3d | t=%-8d | iter=%d\n",
               algorithmKind(snapshot),
               snapshot->currentQuantum,
               snapshot->currentTime,
               snapshot->cpuIterations);
    appendText(buffer, bufferSize, used, " Modo=%-24.24s | PVM=%.42s\n",
               snapshot->modeName ? snapshot->modeName : "--",
               snapshot->pvmStatus ? snapshot->pvmStatus : "--");
    appendRule(buffer, bufferSize, used, '=');
}

static void appendResumen(const SimulationSnapshot* snapshot, char* buffer,
                          size_t bufferSize, size_t* used) {
    char left[80];
    char right[80];
    char bar[80];

    appendSection(buffer, bufferSize, used, "RESUMEN");
    makeBar(bar, sizeof(bar), (float)snapshot->finishedCount / (float)TotalProcesses);
    appendMetric(buffer, bufferSize, used, "Avance", bar);

    snprintf(left, sizeof(left), "%d/%d fin | %d act",
             snapshot->finishedCount, TotalProcesses, snapshot->activeCount);
    snprintf(right, sizeof(right), "%d nuevas | %.4f/t",
             snapshot->newCount, snapshot->avgFinishedPerTime);
    appendPair(buffer, bufferSize, used, "Procesos", left, "Rend.", right);

    snprintf(left, sizeof(left), "%d listos | %d E/S",
             snapshot->readyCount, snapshot->ioCount);
    snprintf(right, sizeof(right), "D1=%d D2=%d D3=%d D4=%d",
             snapshot->ioDeviceCounts[0],
             snapshot->ioDeviceCounts[1],
             snapshot->ioDeviceCounts[2],
             snapshot->ioDeviceCounts[3]);
    appendPair(buffer, bufferSize, used, "Colas", left, "Dispositivos", right);

    snprintf(left, sizeof(left), "%d cambios", snapshot->algorithmChanges);
    snprintf(right, sizeof(right), "%d ctx | %d E/S",
             snapshot->totalContextSwitches, snapshot->totalIoOperations);
    appendPair(buffer, bufferSize, used, "Algoritmo", left, "Eventos", right);
}

static void appendCpu(const SimulationSnapshot* snapshot, char* buffer,
                      size_t bufferSize, size_t* used) {
    char value[120];
    char bar[80];
    char wasteBar[80];

    appendSection(buffer, bufferSize, used, "CPU / PLANIFICADOR");
    makeBar(bar, sizeof(bar), snapshot->cpuUtilization);
    makeBar(wasteBar, sizeof(wasteBar), snapshot->cpuWasteRatio);
    appendMetric(buffer, bufferSize, used, "Uso CPU", bar);
    appendMetric(buffer, bufferSize, used, "Desp. RR", wasteBar);

    snprintf(value, sizeof(value), "espera %.0f | ejecucion %.0f | Q=%d",
             snapshot->avgWaitingTime,
             snapshot->avgExecutionTime,
             snapshot->currentQuantum);
    appendMetric(buffer, bufferSize, used, "Promedios", value);

    appendLine(buffer, bufferSize, used, "  Historial CPU y desperdicio RR");
    for (int i = 0; i < HistoryBars; ++i) {
        char use[48];
        char waste[48];
        makeBarWidth(use, sizeof(use), lastHistoryOrCurrent(snapshot, i, 0), SmallBarWidth);
        makeBarWidth(waste, sizeof(waste), lastHistoryOrCurrent(snapshot, i, 1), SmallBarWidth);
        appendText(buffer, bufferSize, used, "  T-%d uso %-20.20s | desp %.20s\n", i, use, waste);
    }
}

static void appendMemoria(const SimulationSnapshot* snapshot, char* buffer,
                          size_t bufferSize, size_t* used) {
    char left[80];
    char right[80];
    char bar[80];
    int totalFrames = snapshot->memoryUsedFrames + snapshot->memoryFreeFrames;
    if (totalFrames <= 0) totalFrames = PhysicalFrameCount;

    appendSection(buffer, bufferSize, used, "MEMORIA / PAGINACION");
    makeBar(bar, sizeof(bar), (float)snapshot->memoryUsedFrames / (float)totalFrames);
    appendMetric(buffer, bufferSize, used, "Marcos", bar);

    snprintf(left, sizeof(left), "%d usados | %d libres",
             snapshot->memoryUsedFrames, snapshot->memoryFreeFrames);
    snprintf(right, sizeof(right), "mayor=%d | runs=%d",
             snapshot->memoryLargestFreeRun, snapshot->memoryFreeRunCount);
    appendPair(buffer, bufferSize, used, "Detalle", left, "Libres", right);

    snprintf(left, sizeof(left), "int=%d | ext=%d",
             snapshot->internalWaste, snapshot->externalWaste);
    snprintf(right, sizeof(right), "%.2f%% | resize=%d",
             snapshot->fragmentation * 100.0f, snapshot->resizeCount);
    appendPair(buffer, bufferSize, used, "Desperd.", left, "Frag.", right);

    snprintf(left, sizeof(left), "%d fallos", snapshot->totalPageFaults);
    snprintf(right, sizeof(right), "in=%d | out=%d",
             snapshot->totalSwapIns, snapshot->totalSwapOuts);
    appendPair(buffer, bufferSize, used, "Paginas", left, "Swap", right);
}

static void appendRankingLine(char* buffer, size_t bufferSize, size_t* used,
                              int number, const RankingEntry* aged,
                              const RankingEntry* waster) {
    char left[48];
    char right[48];

    if (aged) {
        snprintf(left, sizeof(left), "%s ret=%d pend=%d",
                 aged->processId, aged->primary, aged->secondary);
    } else {
        snprintf(left, sizeof(left), "--");
    }

    if (waster) {
        snprintf(right, sizeof(right), "%s waste=%d usoQ=%d",
                 waster->processId, waster->primary, waster->secondary);
    } else {
        snprintf(right, sizeof(right), "--");
    }

    appendText(buffer, bufferSize, used, "  %-2d %-27.27s | %.35s\n",
               number, left, right);
}

static void appendRoundRobin(const SimulationSnapshot* snapshot, char* buffer,
                             size_t bufferSize, size_t* used) {
    char left[80];
    char right[80];

    appendSection(buffer, bufferSize, used, "ROUND ROBIN");
    snprintf(left, sizeof(left), "%s | procesos=%d",
             rrState(snapshot), snapshot->rrProcessCount);
    snprintf(right, sizeof(right), "retornos=%d | Q=%d",
             snapshot->rrReturnsToReady, snapshot->currentQuantum);
    appendPair(buffer, bufferSize, used, "Estado", left, "Carga", right);

    snprintf(left, sizeof(left), "desp=%d%% local", percentFromRatio(snapshot->cpuWasteRatio));
    snprintf(right, sizeof(right), "uso dist=%d%%",
             percentFromRatio(snapshot->rrDistributedCpuUtilization));
    appendPair(buffer, bufferSize, used, "CPU RR", left, "PVM RR", right);

    appendLine(buffer, bufferSize, used, "  #  Envejecidos                 | Mayor desperdicio");
    for (int i = 0; i < TopRankingCount; ++i) {
        const RankingEntry* aged = i < snapshot->topAgedCount ? &snapshot->topAged[i] : NULL;
        const RankingEntry* waster = i < snapshot->topWastersCount ? &snapshot->topWasters[i] : NULL;
        appendRankingLine(buffer, bufferSize, used, i + 1, aged, waster);
    }
}

static void appendPvmLog(const SimulationSnapshot* snapshot, char* buffer,
                         size_t bufferSize, size_t* used) {
    char left[80];
    char right[80];

    appendSection(buffer, bufferSize, used, "PVM / EVENTOS");
    snprintf(left, sizeof(left), "fin=%d | espera=%d",
             snapshot->distributedFinishedCount, snapshot->distributedWaitingCount);
    snprintf(right, sizeof(right), "pend=%d | usoRR=%d%%",
             snapshot->distributedAvgRemainingCycles,
             percentFromRatio(snapshot->rrDistributedCpuUtilization));
    appendPair(buffer, bufferSize, used, "Dist.", left, "Resumen", right);

    for (int i = 0; i < 5; ++i) {
        if (snapshot->eventLog[i][0] != '\0') {
            appendText(buffer, bufferSize, used, "  %.74s\n", snapshot->eventLog[i]);
        }
    }

    if (snapshot->privilegedProcessActive) {
        appendText(buffer, bufferSize, used, "  Privilegiado RR: %.16s\n",
                   snapshot->privilegedProcessId);
    }
}

static void appendFooter(char* buffer, size_t bufferSize, size_t* used) {
    appendRule(buffer, bufferSize, used, '=');
    appendLine(buffer, bufferSize, used,
               " Controles: X algoritmo | A privilegiar RR | P pausa | Q salir");
    appendLine(buffer, bufferSize, used,
               " Defensa: CPU, memoria, RR y PVM visibles en una sola pantalla.");
    appendRule(buffer, bufferSize, used, '=');
}

static void buildDashboardBuffer(const SimulationSnapshot* snapshot,
                                 char* buffer, size_t bufferSize) {
    size_t used = 0;
    if (!snapshot || !buffer || bufferSize == 0) return;
    buffer[0] = '\0';

    appendHeader(snapshot, buffer, bufferSize, &used);
    appendResumen(snapshot, buffer, bufferSize, &used);
    appendCpu(snapshot, buffer, bufferSize, &used);
    appendMemoria(snapshot, buffer, bufferSize, &used);
    appendRoundRobin(snapshot, buffer, bufferSize, &used);
    appendPvmLog(snapshot, buffer, bufferSize, &used);
    appendFooter(buffer, bufferSize, &used);
}

void guiControllerShowDashboard(const SimulationSnapshot* snapshot) {
    char* buffer = (char*)malloc(DashboardBufferSize);
    if (!buffer) return;
    buildDashboardBuffer(snapshot, buffer, DashboardBufferSize);
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
    printf("======================\n\n");
    printf("  1. PVM real\n");
    printf("  2. Demo virtual\n");
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
    printf("Cambio de algoritmo\n");
    printf("===================\n\n");
    printf("  1. FCFS\n");
    printf("  2. Round Robin\n");
    printf("  3. Automatico\n");
    printf("  0. Cancelar\n\n");
    return readIntPrompt("Opcion: ");
}

int guiControllerAskQuantum(void) {
    return readIntPrompt("Ingrese quantum (10-120): ");
}

int guiControllerAskProcessId(char* outProcessId, int maxLen) {
    if (!outProcessId || maxLen <= 0) return -1;
    consoleIoSetNormalMode();
    printf("ID a privilegiar (Enter cancela): ");
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
    printf("ROUND ROBIN - TOP 5\n");
    printf("===================\n\n");

    printf("Mas envejecidos\n");
    if (scheduler->topAgedCount == 0) {
        printf("  Sin retornos RR registrados.\n");
    }
    for (int i = 0; i < scheduler->topAgedCount; ++i) {
        printf("  %d. %-8s retornos=%d pendientes=%d\n",
               i + 1,
               scheduler->topAged[i].processId,
               scheduler->topAged[i].primary,
               scheduler->topAged[i].secondary);
    }

    printf("\nMayor desperdicio CPU\n");
    if (scheduler->topWastersCount == 0) {
        printf("  Sin desperdicio RR registrado.\n");
    }
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
