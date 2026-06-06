#include "simulationController.h"
#include "guiController.h"
#include "../data/textLoader.h"
#include "../domain/memory/paging.h"
#include "../domain/process/processTable.h"
#include "../domain/scheduler/scheduler.h"
#include "../presentation/consoleIo.h"
#include "../utils/constants.h"
#include "../utils/logger.h"
#include "../utils/randomUtils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

struct SimulationController {
    ProcessTable table;
    Scheduler scheduler;
    PagingSystem* paging;
    TextRepository textRepository;
    Logger logger;
    PvmController pvmController;
    PvmMode pvmMode;
    int running;
    int initialized;
    int lastLogIteration;
    int lastResizeIteration;
    int lastDashboardIteration;
    float utilizationHistory[HistoryBars];
    float wasteHistory[HistoryBars];
    int historyCount;
};

enum {
    DashboardRefreshIterations = 100,
    InteractiveDelayMilliseconds = 5
};

static void delayMilliseconds(int milliseconds) {
    struct timespec delay;
    delay.tv_sec = milliseconds / 1000;
    delay.tv_nsec = (long)(milliseconds % 1000) * 1000000L;
    nanosleep(&delay, NULL);
}

static void loggerWriteHeaders(Logger* logger) {
    loggerTableLine(logger,
        "time,iteration,active,new,finished,ready,io,cpuExecuted,cpuWaste,contextSwitches,"
        "contextTime,ioOperations,pageFaults,usedFrames,freeFrames,largestFreeRun,"
        "fragmentation,avgWaiting,avgExecution,cpuUtilization,quantum,algorithmChanges");
    loggerBcpLine(logger,
        "processId,pid,state,priority,active,arrival,creationDelay,start,finish,totalCpu,"
        "remaining,currentInstance,contextSwitch,lastContextSwitch,totalContextSwitchTime,"
        "executed,timesExecuted,waiting,turnaround,ioDevice,ioRemaining,timesInIo,"
        "memoryRequested,totalMemoryAllocated,pageCount,pageFaults,swapIns,swapOuts,"
        "cpuWasteRatio,agingCounter,returnsToReady,cpuWasteCycles");
}

static void updateMemoryMetrics(SimulationController* controller) {
    int internalWaste = 0;
    ProcessTable* table;
    PagingSystem* paging;

    if (!controller || !controller->paging) return;
    table = &controller->table;
    paging = controller->paging;
    bitmapMemoryUpdateMetrics(&paging->bitmap);

    for (int i = 0; i < TotalProcesses; ++i) {
        int usedWords = table->processes[i].totalMemoryAllocated;
        int remainder = usedWords % WordsPerPage;
        if (usedWords > 0 && remainder != 0) {
            internalWaste += WordsPerPage - remainder;
        }
    }

    table->memoryUsedFrames = paging->bitmap.usedFrames;
    table->memoryFreeFrames = paging->bitmap.freeFrames;
    table->memoryLargestFreeRun = paging->bitmap.largestFreeRun;
    table->memoryFreeRunCount = paging->bitmap.freeRunCount;
    table->internalWaste = internalWaste;
    table->externalWaste = paging->bitmap.externalWaste;
    table->fragmentation = paging->bitmap.fragmentation;
    table->totalPageFaults = paging->pageFaults;
    table->totalSwapIns = paging->swapIns;
    table->totalSwapOuts = paging->swapOuts;
}

static float currentWasteRatio(const ProcessTable* table) {
    long denominator;
    if (!table) return 0.0f;
    denominator = (long)table->totalCpuCyclesExecuted + table->totalCpuWasteCycles;
    return denominator > 0 ? (float)table->totalCpuWasteCycles / (float)denominator : 0.0f;
}

static void recordHistory(SimulationController* controller) {
    float utilization;
    float waste;
    if (!controller) return;
    utilization = controller->table.cpuUtilization;
    waste = currentWasteRatio(&controller->table);
    if (utilization < 0.0f) utilization = 0.0f;
    if (utilization > 1.0f) utilization = 1.0f;
    if (waste < 0.0f) waste = 0.0f;
    if (waste > 1.0f) waste = 1.0f;

    if (controller->historyCount < HistoryBars) {
        controller->utilizationHistory[controller->historyCount] = utilization;
        controller->wasteHistory[controller->historyCount] = waste;
        controller->historyCount++;
        return;
    }

    for (int i = 1; i < HistoryBars; ++i) {
        controller->utilizationHistory[i - 1] = controller->utilizationHistory[i];
        controller->wasteHistory[i - 1] = controller->wasteHistory[i];
    }
    controller->utilizationHistory[HistoryBars - 1] = utilization;
    controller->wasteHistory[HistoryBars - 1] = waste;
}

static void fillSnapshot(SimulationController* controller, SimulationSnapshot* snapshot) {
    if (!controller || !snapshot) return;
    memset(snapshot, 0, sizeof(*snapshot));
    if (controller->pvmMode == pvmModeReal) {
        snapshot->modeName = "Simulacion con PVM real";
    } else if (controller->pvmMode == pvmModeDisabled) {
        snapshot->modeName = "Simulacion sin PVM";
    } else {
        snapshot->modeName = "Simulacion con PVM local";
    }
    snapshot->pvmStatus = controller->pvmController.statusText;
    snapshot->algorithmName = schedulerAlgorithmName(controller->scheduler.algorithm);
    snapshot->currentQuantum = controller->scheduler.quantum;
    snapshot->currentTime = controller->table.currentTime;
    snapshot->cpuIterations = controller->table.cpuIterations;
    snapshot->activeCount = controller->table.activeCount;
    snapshot->newCount = controller->table.newCount;
    snapshot->finishedCount = controller->table.finishedCount;
    snapshot->readyCount = controller->table.readyQueue.count;
    snapshot->ioCount = ioQueueTotalCount(&controller->table.ioQueue);
    snapshot->totalContextSwitches = controller->table.totalContextSwitches;
    snapshot->totalIoOperations = controller->table.totalIoOperations;
    snapshot->algorithmChanges = controller->table.algorithmChanges;
    snapshot->memoryUsedFrames = controller->table.memoryUsedFrames;
    snapshot->memoryFreeFrames = controller->table.memoryFreeFrames;
    snapshot->memoryLargestFreeRun = controller->table.memoryLargestFreeRun;
    snapshot->memoryFreeRunCount = controller->table.memoryFreeRunCount;
    snapshot->internalWaste = controller->table.internalWaste;
    snapshot->externalWaste = controller->table.externalWaste;
    snapshot->totalPageFaults = controller->table.totalPageFaults;
    snapshot->totalSwapIns = controller->table.totalSwapIns;
    snapshot->totalSwapOuts = controller->table.totalSwapOuts;
    snapshot->fragmentation = controller->table.fragmentation;
    snapshot->avgWaitingTime = controller->table.avgWaitingTime;
    snapshot->avgExecutionTime = controller->table.avgExecutionTime;
    snapshot->avgFinishedPerTime = controller->table.avgFinishedPerTime;
    snapshot->cpuUtilization = controller->table.cpuUtilization;
    snapshot->cpuWasteRatio = currentWasteRatio(&controller->table);
    snapshot->historyCount = controller->historyCount;
    for (int i = 0; i < controller->historyCount; ++i) {
        snapshot->utilizationHistory[i] = controller->utilizationHistory[i];
        snapshot->wasteHistory[i] = controller->wasteHistory[i];
    }
    snapshot->topAgedCount = controller->scheduler.topAgedCount;
    snapshot->topWastersCount = controller->scheduler.topWastersCount;
    for (int i = 0; i < snapshot->topAgedCount; ++i) {
        snapshot->topAged[i] = controller->scheduler.topAged[i];
    }
    for (int i = 0; i < snapshot->topWastersCount; ++i) {
        snapshot->topWasters[i] = controller->scheduler.topWasters[i];
    }

    if (controller->pvmMode == pvmModeDisabled) {
        snprintf(snapshot->eventLog[0], sizeof(snapshot->eventLog[0]),
                 "[PVM DESACTIVADO] Analisis distribuido omitido.");
        snprintf(snapshot->eventLog[1], sizeof(snapshot->eventLog[1]),
                 "[STATS] Sin reporte distribuido.");
    } else if (controller->pvmMode == pvmModeReal) {
        snprintf(snapshot->eventLog[0], sizeof(snapshot->eventLog[0]),
                 "[PVM REAL] Resultado integrado por slaves reales. %s",
                 controller->pvmController.statusText);
        snprintf(snapshot->eventLog[1], sizeof(snapshot->eventLog[1]),
                 "[STATS] Finalizados=%d | En E/S=%d | Prom. pendientes=%d.",
                 controller->pvmController.lastReport.stats.finishedCount,
                 controller->pvmController.lastReport.stats.waitingCount,
                 controller->pvmController.lastReport.stats.avgRemainingCycles);
        snprintf(snapshot->eventLog[2], sizeof(snapshot->eventLog[2]),
                 "[RR] Procesos RR=%d | retornos=%d | uso promedio=%.0f%%.",
                 controller->pvmController.lastReport.aging.processCount,
                 controller->pvmController.lastReport.aging.totalReturnsToReady,
                 controller->pvmController.lastReport.aging.avgCpuUtilization * 100.0f);
    } else {
        snprintf(snapshot->eventLog[0], sizeof(snapshot->eventLog[0]),
                 "[PVM LOCAL] Analisis local equivalente. %s",
                 controller->pvmController.statusText);
        snprintf(snapshot->eventLog[1], sizeof(snapshot->eventLog[1]),
                 "[STATS] Finalizados=%d | En E/S=%d | Prom. pendientes=%d.",
                 controller->pvmController.lastReport.stats.finishedCount,
                 controller->pvmController.lastReport.stats.waitingCount,
                 controller->pvmController.lastReport.stats.avgRemainingCycles);
        snprintf(snapshot->eventLog[2], sizeof(snapshot->eventLog[2]),
                 "[RR] Procesos RR=%d | retornos=%d | uso promedio=%.0f%%.",
                 controller->pvmController.lastReport.aging.processCount,
                 controller->pvmController.lastReport.aging.totalReturnsToReady,
                 controller->pvmController.lastReport.aging.avgCpuUtilization * 100.0f);
    }
    snprintf(snapshot->eventLog[3], sizeof(snapshot->eventLog[3]),
             "[COLAS] Listos=%d | E/S=%d | cambios algoritmo=%d.",
             snapshot->readyCount, snapshot->ioCount, snapshot->algorithmChanges);
    if (controller->scheduler.hasPrivilegedProcess && controller->scheduler.algorithm == schedulerRr) {
        snprintf(snapshot->eventLog[4], sizeof(snapshot->eventLog[4]),
                 "[SISTEMA] Proceso RR privilegiado activo: %s.",
                 controller->scheduler.privilegedProcessId);
    } else {
        snprintf(snapshot->eventLog[4], sizeof(snapshot->eventLog[4]),
                 "[SISTEMA] Sin proceso RR privilegiado activo.");
    }
}

static void showDashboard(SimulationController* controller) {
    SimulationSnapshot snapshot;
    fillSnapshot(controller, &snapshot);
    guiControllerShowDashboard(&snapshot);
}

static int requeueProcess(ProcessTable* table, Bcp* bcp, int processIndex) {
    int result;

    if (!table || !bcp) return -1;
    result = bcp->privileged
        ? readyQueuePushFront(&table->readyQueue, processIndex)
        : readyQueuePush(&table->readyQueue, processIndex);
    if (result != 0) return -1;
    bcp->state = processStateReady;
    bcp->lastReadyTime = table->currentTime;
    return 0;
}

static int sendProcessToIo(SimulationController* controller, int processIndex) {
    ProcessTable* table;
    Bcp* bcp;
    const char* phrase;

    if (!controller || processIndex < 0 || processIndex >= TotalProcesses) return -1;
    table = &controller->table;
    bcp = &table->processes[processIndex];
    phrase = textRepositoryRandomPhrase(&controller->textRepository);
    pagingSystemAccessPhrase(controller->paging, bcp, processIndex, phrase, &controller->textRepository);
    if (ioQueueSend(&table->ioQueue, table->processes, processIndex,
                    randomIoDevice(), randomIoCycles()) == 0) {
        table->totalIoOperations++;
        return 0;
    }
    return requeueProcess(table, bcp, processIndex);
}

static int simulationControllerStep(SimulationController* controller) {
    ProcessTable* table;
    Scheduler* scheduler;
    Bcp* bcp;
    int processIndex;
    int instanceCycles;
    int quantum;
    int executeCycles;
    int contextTime;
    int shouldGoIo;

    if (!controller) return -1;
    table = &controller->table;
    scheduler = &controller->scheduler;

    processTablePromoteNew(table);
    ioQueueTick(&table->ioQueue, table->processes, &table->readyQueue, table->currentTime);
    processTableUpdateQueueMetrics(table);

    if (readyQueueIsEmpty(&table->readyQueue)) {
        table->currentTime++;
        return 0;
    }

    processIndex = schedulerSelectNext(scheduler, table);
    if (processIndex < 0) {
        table->currentTime++;
        return 0;
    }

    bcp = &table->processes[processIndex];
    if (bcp->state == processStateFinished) return 0;

    bcp->waitingTime += table->currentTime > bcp->lastReadyTime
        ? table->currentTime - bcp->lastReadyTime
        : 0;
    bcp->state = processStateRunning;
    if (bcp->startTime < 0) bcp->startTime = table->currentTime;

    contextTime = randomContextSwitchTime();
    bcp->contextSwitchTime = contextTime;
    bcp->contextSwitchCount++;
    bcp->totalContextSwitchTime += contextTime;
    table->totalContextSwitches++;
    table->totalContextSwitchTime += contextTime;
    table->currentTime += contextTime;

    pagingSystemTouchProcess(controller->paging, bcp, processIndex, &controller->textRepository);

    if (bcp->currentInstanceCycles <= 0) {
        bcp->currentInstanceCycles = randomCpuInstanceCycles();
    }
    instanceCycles = bcp->currentInstanceCycles;
    quantum = scheduler->algorithm == schedulerRr ? scheduler->quantum : instanceCycles;
    executeCycles = instanceCycles;
    if (scheduler->algorithm == schedulerRr && executeCycles > quantum) executeCycles = quantum;
    if (executeCycles > bcp->remainingCpuCycles) executeCycles = bcp->remainingCpuCycles;

    bcp->quantumAssigned = quantum;
    bcp->quantumUsed = executeCycles;
    bcp->remainingCpuCycles -= executeCycles;
    if (executeCycles >= bcp->currentInstanceCycles || bcp->remainingCpuCycles <= 0) {
        bcp->currentInstanceCycles = 0;
    } else {
        bcp->currentInstanceCycles -= executeCycles;
    }
    bcp->timeInExecution += executeCycles;
    bcp->timesExecuted++;
    table->totalCpuCyclesExecuted += executeCycles;
    table->currentTime += executeCycles;
    table->cpuIterations++;

    if (scheduler->algorithm == schedulerRr) {
        int waste = quantum > executeCycles ? quantum - executeCycles : 0;
        bcp->rrExecutionCount++;
        bcp->rrQuantumAssignedTotal += quantum;
        bcp->rrQuantumUsedTotal += executeCycles;
        bcp->cpuWasteCycles += waste;
        table->totalCpuWasteCycles += waste;
        bcp->cpuWasteRatio = bcp->rrQuantumAssignedTotal > 0
            ? (float)bcp->cpuWasteCycles / (float)bcp->rrQuantumAssignedTotal
            : 0.0f;
        if (bcp->currentInstanceCycles > 0 && bcp->remainingCpuCycles > 0) {
            bcp->timesReturnedToReady++;
            bcp->agingCounter++;
            if (requeueProcess(table, bcp, processIndex) != 0) return -1;
        } else if (bcp->remainingCpuCycles <= 0) {
            processTableFinishProcess(table, processIndex);
            pagingSystemDeallocateProcess(controller->paging, processIndex);
        } else {
            shouldGoIo = randomChance(15);
            if (shouldGoIo) {
                if (sendProcessToIo(controller, processIndex) != 0) return -1;
            } else {
                bcp->timesReturnedToReady++;
                bcp->agingCounter++;
                if (requeueProcess(table, bcp, processIndex) != 0) return -1;
            }
        }
    } else if (bcp->remainingCpuCycles <= 0) {
        processTableFinishProcess(table, processIndex);
        pagingSystemDeallocateProcess(controller->paging, processIndex);
    } else {
        shouldGoIo = randomChance(12);
        if (shouldGoIo) {
            if (sendProcessToIo(controller, processIndex) != 0) return -1;
        } else {
            if (requeueProcess(table, bcp, processIndex) != 0) return -1;
        }
    }

    pagingSystemUpdateBcpCounters(controller->paging, bcp);

    if (scheduler->algorithm == schedulerRr &&
        table->cpuIterations % RrRebalanceInterval == 0) {
        schedulerRebalanceQuantum(scheduler, table);
    }
    schedulerAutoSwitchIfNeeded(scheduler, table);
    schedulerUpdateRankings(scheduler, table);
    return 0;
}

static int simulationFinished(const SimulationController* controller) {
    if (!controller) return 1;
    return controller->table.finishedCount >= TotalProcesses &&
           controller->table.newCount == 0 &&
           controller->table.readyQueue.count == 0 &&
           ioQueueTotalCount(&controller->table.ioQueue) == 0;
}

static void handleAlgorithmCommand(SimulationController* controller) {
    int option = guiControllerAskAlgorithm();
    if (option == 1) {
        schedulerClearPrivilegedProcess(&controller->scheduler, &controller->table);
        controller->scheduler.algorithm = schedulerFcfs;
        controller->table.algorithmChanges++;
    } else if (option == 2) {
        int quantum = guiControllerAskQuantum();
        if (quantum > 0) controller->scheduler.quantum = quantum;
        if (controller->scheduler.quantum < MinQuantum) controller->scheduler.quantum = MinQuantum;
        if (controller->scheduler.quantum > MaxQuantum) controller->scheduler.quantum = MaxQuantum;
        controller->scheduler.algorithm = schedulerRr;
        controller->table.currentQuantum = controller->scheduler.quantum;
        schedulerRecordQuantum(&controller->scheduler);
        controller->table.algorithmChanges++;
    }
}

static void handleRankingCommand(SimulationController* controller) {
    char processId[ProcessIdLen];
    if (controller->scheduler.algorithm != schedulerRr) return;
    schedulerUpdateRankings(&controller->scheduler, &controller->table);
    guiControllerShowRankings(&controller->scheduler);
    memset(processId, 0, sizeof(processId));
    if (guiControllerAskProcessId(processId, sizeof(processId)) > 0) {
        schedulerPrivilegeProcess(&controller->scheduler, &controller->table, processId);
    }
}

static void handlePauseCommand(void) {
    int paused = 1;
    guiControllerShowPauseMessage();
    while (paused) {
        if (consoleIoKbhit()) {
            int command = consoleIoGetChar();
            if (command == 'P' || command == 'p') paused = 0;
        }
        delayMilliseconds(50);
    }
    guiControllerShowResumeMessage();
}

static void handleCommand(SimulationController* controller, int command) {
    if (!controller || command == 0) return;
    switch (command) {
        case 'X': case 'x':
            handleAlgorithmCommand(controller);
            break;
        case 'A': case 'a':
            handleRankingCommand(controller);
            break;
        case 'P': case 'p':
            handlePauseCommand();
            break;
        case 'Q': case 'q': case 27:
            controller->running = 0;
            break;
        default:
            break;
    }
}

SimulationController* simulationControllerCreate(PvmMode pvmMode) {
    SimulationController* controller = (SimulationController*)calloc(1, sizeof(SimulationController));
    if (!controller) return NULL;
    controller->pvmMode = pvmMode;
    return controller;
}

int simulationControllerInit(SimulationController* controller) {
    if (!controller) return -1;
    controller->paging = (PagingSystem*)malloc(sizeof(*controller->paging));
    if (!controller->paging) return -1;

    randomInit(0);
    mkdir(LogDirectory, 0775);
    if (textRepositoryInit(&controller->textRepository, WordBookPath, PhraseBookPath) != 0) {
        fprintf(stderr, "No se pudieron cargar los libros requeridos: %s y %s\n",
                WordBookPath, PhraseBookPath);
        return -1;
    }

    processTableInit(&controller->table);
    schedulerInit(&controller->scheduler, schedulerFcfs, DefaultQuantum);
    controller->table.currentQuantum = controller->scheduler.quantum;
    pagingSystemInit(controller->paging, controller->table.processes);
    updateMemoryMetrics(controller);
    processTableUpdateAverages(&controller->table);
    loggerOpen(&controller->logger, ProcessTableLog, BcpLog);
    loggerWriteHeaders(&controller->logger);

    pvmControllerInit(&controller->pvmController, controller->pvmMode);
    if (pvmControllerStart(&controller->pvmController) != 0) {
        return controller->pvmMode == pvmModeReal ? -1 : 0;
    }

    controller->running = 1;
    controller->initialized = 1;
    return 0;
}

int simulationControllerRun(SimulationController* controller) {
    int interactive;
    int finalPvmResult;
    if (!controller || !controller->initialized) return -1;
    interactive = consoleIoInit() == 0;
    if (interactive) showDashboard(controller);

    while (controller->running &&
           !simulationFinished(controller) &&
           controller->table.cpuIterations < MaxCpuIterations) {
        int previousIterations = controller->table.cpuIterations;
        int iterationAdvanced;

        if (simulationControllerStep(controller) != 0) break;
        iterationAdvanced = controller->table.cpuIterations != previousIterations;

        if (interactive) {
            handleCommand(controller, guiControllerReadCommand());
            if (!controller->running) break;
        }

        updateMemoryMetrics(controller);
        processTableUpdateAverages(&controller->table);

        if (iterationAdvanced &&
            controller->table.cpuIterations % MemoryResizeInterval == 0 &&
            controller->table.cpuIterations != controller->lastResizeIteration) {
            pagingSystemResizeActive(controller->paging, controller->table.activeSlots,
                                     controller->table.processes, controller->table.currentTime);
            controller->table.resizeCount++;
            controller->lastResizeIteration = controller->table.cpuIterations;
        }

        if (iterationAdvanced &&
            controller->table.cpuIterations % 1000 == 0 &&
            controller->table.cpuIterations != controller->lastLogIteration) {
            processTableLogSnapshot(&controller->table, &controller->logger);
            controller->lastLogIteration = controller->table.cpuIterations;
        }

        if (iterationAdvanced) {
            pvmControllerRunPeriodic(&controller->pvmController, &controller->table,
                                     controller->table.cpuIterations);
        }

        /* Refresco rapido y estable: muestra cambios cada 100 iteraciones sin inundar la terminal. */
        if (interactive &&
            iterationAdvanced &&
            controller->table.cpuIterations % DashboardRefreshIterations == 0 &&
            controller->table.cpuIterations != controller->lastDashboardIteration) {
            recordHistory(controller);
            showDashboard(controller);
            controller->lastDashboardIteration = controller->table.cpuIterations;
        }

        /* Pausa corta para mantener respuesta fluida al teclado y refresco visual mas rapido. */
        if (interactive) delayMilliseconds(InteractiveDelayMilliseconds);
    }

    updateMemoryMetrics(controller);
    processTableUpdateAverages(&controller->table);
    processTableLogSnapshot(&controller->table, &controller->logger);
    processTableLogBcps(&controller->table, &controller->logger);
    finalPvmResult = pvmControllerRunFinal(&controller->pvmController, &controller->table);
    recordHistory(controller);
    if (interactive) {
        showDashboard(controller);
        consoleIoCleanup();
    } else if (controller->pvmMode == pvmModeDisabled) {
        printf("%s\n", controller->pvmController.statusText);
    } else if (finalPvmResult != 0) {
        printf("%s\n", controller->pvmController.statusText);
    } else {
        pvmControllerPrintReport(controller->pvmMode == pvmModeReal ? "PVM real" : "PVM local",
                                 &controller->pvmController.lastReport);
    }
    return 0;
}

void simulationControllerDestroy(SimulationController* controller) {
    if (!controller) return;
    pvmControllerDestroy(&controller->pvmController);
    loggerClose(&controller->logger);
    free(controller->paging);
    free(controller);
}
