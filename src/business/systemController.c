// src/business/systemController.c
// Orquestador principal del simulador: inicializa, coordina y destruye todos los subsistemas.

#include "systemController.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../utils/constants.h"
#include "../utils/random.h"
#include "../utils/wordLoader.h"
#include "../utils/phraseLoader.h"
#include "../utils/timeHelper.h"
#include "../utils/errorHandler.h"

#include "../domain/process/bcp.h"
#include "../domain/process/process.h"
#include "../domain/process/processTable.h"
#include "../domain/process/processGenerator.h"

#include "../domain/scheduler/readyQueue.h"
#include "../domain/scheduler/fcfsScheduler.h"
#include "../domain/scheduler/rrScheduler.h"
#include "../domain/scheduler/scheduler.h"
#include "../domain/scheduler/rebalancing.h"
#include "../domain/scheduler/preemption.h"
#include "../domain/scheduler/algorithmSwitcher.h"

#include "../domain/io/ioQueue.h"
#include "../domain/io/ioDispatcher.h"
#include "../domain/io/ioCompletionHandler.h"

#include "../domain/memory/memoria.h"
#include "../domain/memory/bitmapManager.h"
#include "../domain/memory/fifoReplacement.h"
#include "../domain/memory/swapManager.h"
#include "../domain/memory/pagingManager.h"
#include "../domain/memory/memoryResize.h"

#include "../domain/stats/statsCollector.h"
#include "../domain/stats/performanceBar.h"
#include "../domain/stats/agingAnalysis.h"

#if pvmModeEnabled
#include "../domain/distributed/pvmMaster.h"
#endif

#include "../infrastructure/logger.h"
#include "../infrastructure/bcpLog.h"
#include "../infrastructure/processLog.h"

#include "../presentation/consoleIo.h"
#include "../presentation/menu.h"

struct SystemController {
    ProcessTable*          processTable;
    ReadyQueue*            readyQueue;
    FcfsScheduler*         fcfsScheduler;
    RrScheduler*           rrScheduler;
    Scheduler*             scheduler;
    Rebalancer*            rebalancer;
    PreemptionController*  preemptionController;
    AlgorithmSwitcher*     algorithmSwitcher;
    IoQueue*               ioQueue;
    IoDispatcher*          ioDispatcher;
    IoCompletionHandler*   ioCompletionHandler;
    BitmapManager*         bitmapManager;
    PagingManager*         pagingManager;
    SwapManager*           swapManager;
    MemoryResizer*         memoryResizer;
    StatsCollector*        statsCollector;
    PerformanceBar*        performanceBar;
    AgingAnalysis*         agingAnalysis;
    Logger*                mainLogger;
    Logger*                bcpLogger;
    BcpLog*                bcpLog;
    ProcessLog*            processLog;
    int                    running;
    SystemRunMode          runMode;
    Process*               currentProcess;
};

SystemController* systemControllerCreate(void) {
    SystemController* ctrl = (SystemController*)malloc(sizeof(SystemController));
    if (!ctrl) {
        fprintf(stderr, "[SystemController] Error: no se pudo asignar memoria.\n");
        return NULL;
    }
    memset(ctrl, 0, sizeof(SystemController));
    ctrl->running = 0;
    ctrl->runMode = SystemRunModeLocal;
    ctrl->currentProcess = NULL;
    return ctrl;
}

void systemControllerSetRunMode(SystemController* ctrl, SystemRunMode mode) {
    if (!ctrl) return;
    ctrl->runMode = (mode == SystemRunModePvm) ? SystemRunModePvm : SystemRunModeLocal;
}

SystemRunMode systemControllerGetRunMode(const SystemController* ctrl) {
    return ctrl ? ctrl->runMode : SystemRunModeLocal;
}

int systemControllerInit(SystemController* ctrl) {
    if (!ctrl) return -1;

    initRandom(0);
    wordLoaderInit("libro1.odt");
    phraseLoaderInit("frases.odt");

    ctrl->mainLogger = loggerCreate("simulation.log", LogLevelInfo);
    if (!ctrl->mainLogger)
        fprintf(stderr, "[SystemController] Advertencia: no se pudo abrir simulation.log\n");

    ctrl->bcpLogger = loggerCreate("bcp.log", LogLevelInfo);  // CORREGIDO: era LogLevelDebug → genera 40GB
    if (!ctrl->bcpLogger)
        fprintf(stderr, "[SystemController] Advertencia: no se pudo abrir bcp.log\n");

    ctrl->bcpLog = bcpLogCreate(ctrl->bcpLogger);
    ctrl->processLog = processLogCreate(ctrl->mainLogger);

    ctrl->readyQueue = readyQueueCreate();
    if (!ctrl->readyQueue) goto rollback_logs;

    const int mult[numColasEs] = { multColaEs1, multColaEs2, multColaEs3, multColaEs4 };
    ctrl->ioQueue = ioQueueCreate(mult);
    if (!ctrl->ioQueue) goto rollback_readyqueue;

    ctrl->processTable = processTableCreate();
    if (!ctrl->processTable) goto rollback_ioqueue;
    ctrl->processTable->readyQueue = ctrl->readyQueue;
    ctrl->processTable->ioQueue = ctrl->ioQueue;

    processGeneratorInit();
    ctrl->processTable->totalProcesses = 0;

    ctrl->fcfsScheduler = fcfsSchedulerCreate();
    if (!ctrl->fcfsScheduler) goto rollback_proctable;

    ctrl->rrScheduler = rrSchedulerCreate(quantumDefault);
    if (!ctrl->rrScheduler) goto rollback_fcfs;

    ctrl->scheduler = schedulerCreate(SchedulerAlgorithmFcfs);
    if (!ctrl->scheduler) goto rollback_rr;
    schedulerSetFcfs(ctrl->scheduler, ctrl->fcfsScheduler);
    schedulerSetRr(ctrl->scheduler, ctrl->rrScheduler);

    ctrl->rebalancer = rebalancerCreate();
    if (!ctrl->rebalancer) goto rollback_scheduler;

    ctrl->preemptionController = preemptionControllerCreate(0);
    if (!ctrl->preemptionController) goto rollback_rebalancer;

    ctrl->algorithmSwitcher = algorithmSwitcherCreate(quantumDefault);
    if (!ctrl->algorithmSwitcher) goto rollback_preemption;

    ctrl->ioDispatcher = ioDispatcherCreate(ctrl->ioQueue);
    if (!ctrl->ioDispatcher) goto rollback_algoswitch;

    ctrl->ioCompletionHandler = ioCompletionHandlerCreate();
    if (!ctrl->ioCompletionHandler) goto rollback_iodispatch;

    ctrl->bitmapManager = bitmapManagerCreate(1);
    if (!ctrl->bitmapManager) goto rollback_iocomp;

    ctrl->swapManager = swapManagerCreate(maxSwap);
    if (!ctrl->swapManager) goto rollback_bitmap;

    ctrl->pagingManager = pagingManagerCreate(maxMarcos, ctrl->bitmapManager);
    if (!ctrl->pagingManager) goto rollback_swap;
    pagingManagerSetSwapManager(ctrl->pagingManager, ctrl->swapManager);

    ctrl->memoryResizer = memoryResizerCreate();
    if (!ctrl->memoryResizer) goto rollback_paging;

    ctrl->statsCollector = statsCollectorCreate();
    if (!ctrl->statsCollector) goto rollback_resizer;

    ctrl->performanceBar = performanceBarCreate();
    if (!ctrl->performanceBar) goto rollback_stats;

    ctrl->agingAnalysis = agingAnalysisCreate();
    if (!ctrl->agingAnalysis) goto rollback_perfbar;

    ctrl->running = 1;
    if (ctrl->mainLogger)
        loggerLog(ctrl->mainLogger, LogLevelInfo, "SystemController inicializado correctamente.");
    return 0;

rollback_perfbar:   performanceBarDestroy(ctrl->performanceBar);
rollback_stats:     statsCollectorDestroy(ctrl->statsCollector);
rollback_resizer:   memoryResizerDestroy(ctrl->memoryResizer);
rollback_paging:    pagingManagerDestroy(ctrl->pagingManager);
rollback_swap:      swapManagerDestroy(ctrl->swapManager);
rollback_bitmap:    bitmapManagerDestroy(ctrl->bitmapManager);
rollback_iocomp:    ioCompletionHandlerDestroy(ctrl->ioCompletionHandler);
rollback_iodispatch:ioDispatcherDestroy(ctrl->ioDispatcher);
rollback_algoswitch:algorithmSwitcherDestroy(ctrl->algorithmSwitcher);
rollback_preemption:preemptionControllerDestroy(ctrl->preemptionController);
rollback_rebalancer:rebalancerDestroy(ctrl->rebalancer);
rollback_scheduler: schedulerDestroy(ctrl->scheduler);
rollback_rr:        rrSchedulerDestroy(ctrl->rrScheduler);
rollback_fcfs:      fcfsSchedulerDestroy(ctrl->fcfsScheduler);
rollback_proctable: processTableDestroy(ctrl->processTable);
rollback_ioqueue:   ioQueueDestroy(ctrl->ioQueue);
rollback_readyqueue:readyQueueDestroy(ctrl->readyQueue);
rollback_logs:
    if (ctrl->processLog) processLogDestroy(ctrl->processLog);
    if (ctrl->bcpLog) bcpLogDestroy(ctrl->bcpLog);
    if (ctrl->bcpLogger) loggerDestroy(ctrl->bcpLogger);
    if (ctrl->mainLogger) loggerDestroy(ctrl->mainLogger);
    wordLoaderCleanup();
    phraseLoaderCleanup();
    return -1;
}

static int findRunningSlot(SystemController* ctrl, Process* process) {
    if (!ctrl || !process) return -1;
    for (int i = 0; i < procesosEnEjecucion; i++)
        if (ctrl->processTable->runningProcesses[i] == process) return i;
    return -1;
}

static int findFreeRunningSlot(SystemController* ctrl) {
    if (!ctrl) return -1;
    for (int i = 0; i < procesosEnEjecucion; i++)
        if (ctrl->processTable->runningProcesses[i] == NULL) return i;
    return -1;
}

static int moveToNew(SystemController* ctrl, Process* process) {
    if (!ctrl || !process || !process->bcp) return -1;
    for (int i = 0; i < procesosEnEspera; i++) {
        if (ctrl->processTable->newRequests[i] == NULL) {
            ctrl->processTable->newRequests[i] = process;
            bcpSetState(process->bcp, ProcessStateNew);
            return 0;
        }
    }
    return -1;
}

static int moveToReady(SystemController* ctrl, Process* process) {
    if (!ctrl || !process || !process->bcp) return -1;
    if (readyQueueEnqueue(ctrl->readyQueue, process) != 0) return -1;
    bcpSetState(process->bcp, ProcessStateReady);
    process->bcp->timesReturnedToReady++;
    return 0;
}

static int moveToIo(SystemController* ctrl, Process* process, int device) {
    if (!ctrl || !process || !process->bcp) return -1;
    if (ioDispatcherDispatch(ctrl->ioDispatcher, process, device) != 0) return -1;
    bcpSetState(process->bcp, ProcessStateWaitingIo);
    return 0;
}

static void closePartialQuantum(SystemController* ctrl, Bcp* bcp, int quantum) {
    if (!bcp || quantum <= 0) return;
    if (bcp->quantumUsed > 0 && bcp->quantumUsed < quantum) {
        int waste = quantum - bcp->quantumUsed;
        bcp->wastedCpuCycles += waste;
        bcp->cpuWasteRatio = (bcp->timeInExecution + bcp->wastedCpuCycles) > 0
            ? (float)bcp->wastedCpuCycles / (float)(bcp->timeInExecution + bcp->wastedCpuCycles)
            : 0.0f;
        if (ctrl && ctrl->processTable)
            ctrl->processTable->totalCpuWasteCycles += waste;
    }
    bcp->quantumUsed = 0;
}

static void syncSimulationMetrics(SystemController* ctrl) {
    if (!ctrl || !ctrl->processTable) return;
    ProcessTable* table = ctrl->processTable;

    if (ctrl->pagingManager) {
        table->totalPageFaults = pagingManagerGetPageFaultCount(ctrl->pagingManager);
        table->internalWaste = pagingManagerGetInternalWaste(ctrl->pagingManager);
        table->externalWaste = pagingManagerGetExternalWaste(ctrl->pagingManager);
        table->fragmentation = pagingManagerGetFragmentation(ctrl->pagingManager);
    }

    if (ctrl->bitmapManager) {
        bitmapManagerUpdateMetrics(ctrl->bitmapManager);
        table->memoryUsedBlocks = bitmapManagerGetUsedBlocks(ctrl->bitmapManager);
        table->memoryFreeBlocks = bitmapManagerGetFreeBlocks(ctrl->bitmapManager);
        table->largestFreeRun = bitmapManagerGetLargestFreeRun(ctrl->bitmapManager);
        table->freeRunCount = bitmapManagerGetFreeRunCount(ctrl->bitmapManager);
    }

    int accountedCpu = table->totalCpuCyclesExecuted + table->totalCpuWasteCycles;
    table->cpuWasteRatio = accountedCpu > 0
        ? (float)table->totalCpuWasteCycles / (float)accountedCpu
        : 0.0f;
}

static void finishProcess(SystemController* ctrl, Process* process, int slot, int ciclo) {
    if (!ctrl || !process || !process->bcp) return;
    Bcp* bcp = process->bcp;
    bcp->finishTime = ciclo;
    bcp->quantumUsed = 0;
    bcpSetState(bcp, ProcessStateFinished);
    processDeactivate(process);
    ctrl->processTable->totalWaitingTimeFinished += bcp->timeInWaiting;
    ctrl->processTable->totalTurnaroundTimeFinished += bcp->finishTime - bcp->arrivalTime;
    ctrl->processTable->totalExecutionTimeFinished += bcp->timeInExecution;
    processTableIncrementFinished(ctrl->processTable);
    if (ctrl->processLog) processLogRecordTermination(ctrl->processLog, process);
    if (slot >= 0 && slot < procesosEnEjecucion)
        ctrl->processTable->runningProcesses[slot] = NULL;
    if (slot >= 0)
        pagingManagerDeallocatePagesForProcess(ctrl->pagingManager, slot);
    processDestroy(process);
}

static int hasPendingNewRequests(ProcessTable* table) {
    if (!table) return 0;
    for (int i = 0; i < procesosEnEspera; i++)
        if (table->newRequests[i] != NULL) return 1;
    return 0;
}

static int hasIoProcesses(ProcessTable* table) {
    if (!table || !table->ioQueue) return 0;
    for (int d = 0; d < numColasEs; d++)
        if (table->ioQueue->devices[d].size > 0) return 1;
    return 0;
}

int systemControllerCycle(SystemController* ctrl) {
    if (!ctrl || !ctrl->running) return -1;

    ctrl->processTable->currentCycle++;
    int ciclo = ctrl->processTable->currentCycle;

    // DEBUG cada 200 ciclos (solo en compilación DEBUG_VERBOSE)
#ifdef DEBUG_VERBOSE
    if (ciclo % 200 == 0) {
        fprintf(stderr, "[DEBUG] Ciclo %d | Listos: %d | Total: %d | Finalizados: %d | CPU cycles: %d\n",
               ciclo,
               readyQueueGetCount(ctrl->readyQueue),
               ctrl->processTable->totalProcesses,
               ctrl->processTable->finishedProcesses,
               ctrl->processTable->totalCpuCyclesExecuted);
        fflush(stderr);
    }
#endif

    // -------------------------------------------------------------------------
    // 2. Admitir nuevos procesos usando peek (NO extraer si aún no toca)
    // -------------------------------------------------------------------------
    while (processGeneratorHasMore()) {
        Bcp* bcp = processGeneratorPeekNext();
        if (!bcp) break;
        if (bcp->arrivalTime > ciclo) break;
        bcp = processGeneratorGetNext();
        if (!bcp) break;

        // CORREGIDO: usar processCreateWithBcp evita crear y descartar un BCP innecesario
        Process* proc = processCreateWithBcp(bcp);
        if (!proc) {
            fprintf(stderr, "[SystemController] Fallo processCreateWithBcp para %s\n", bcp->processId);
            continue;
        }
        processActivate(proc);
        ctrl->processTable->totalProcesses++;

        int slot = findFreeRunningSlot(ctrl);

        if (slot != -1 && !readyQueueIsFull(ctrl->readyQueue)) {
            ctrl->processTable->runningProcesses[slot] = proc;
            if (moveToReady(ctrl, proc) == 0) {
                if (pagingManagerAllocatePageForProcess(ctrl->pagingManager, slot, bcp->pageCount) != 0 &&
                    ctrl->mainLogger)
                    loggerLogFormat(ctrl->mainLogger, LogLevelWarning,
                        "No se pudo inicializar tabla de paginas para %s", bcp->processId);
                if (ctrl->processLog)
                    processLogRecordCreation(ctrl->processLog, proc);
            } else {
                ctrl->processTable->runningProcesses[slot] = NULL;
                bcpSetState(bcp, ProcessStateNew);
                if (moveToNew(ctrl, proc) != 0) {
                    ctrl->processTable->totalProcesses--;
                    processDestroy(proc);
                }
            }
        } else {
            if (moveToNew(ctrl, proc) != 0) {
                fprintf(stderr, "[ERROR] No hay espacio para admitir proceso %s\n", bcp->processId);
                ctrl->processTable->totalProcesses--;
                processDestroy(proc);
            }
        }
    }

    // 3. Mover desde newRequests a runningProcesses solo cuando tambien puede entrar a ready.
    for (int i = 0; i < procesosEnEspera; i++) {
        Process* espera = ctrl->processTable->newRequests[i];
        if (!espera || !espera->bcp) continue;
        if (espera->bcp->arrivalTime > ciclo) continue;
        if (readyQueueIsFull(ctrl->readyQueue)) break;

        int slotLibre = findFreeRunningSlot(ctrl);
        if (slotLibre == -1) continue;

        ctrl->processTable->runningProcesses[slotLibre] = espera;
        if (moveToReady(ctrl, espera) == 0) {
            ctrl->processTable->newRequests[i] = NULL;
            if (pagingManagerAllocatePageForProcess(ctrl->pagingManager, slotLibre, espera->bcp->pageCount) != 0 &&
                ctrl->mainLogger)
                loggerLogFormat(ctrl->mainLogger, LogLevelWarning,
                    "No se pudo inicializar tabla de paginas para %s", espera->bcp->processId);
            if (ctrl->processLog)
                processLogRecordCreation(ctrl->processLog, espera);
        } else {
            ctrl->processTable->runningProcesses[slotLibre] = NULL;
            bcpSetState(espera->bcp, ProcessStateNew);
        }
    }

    // 4. Finalización de E/S
    ioCompletionHandlerProcess(ctrl->ioCompletionHandler, ctrl->ioQueue, ctrl->readyQueue,
                               ctrl->pagingManager, ctrl->processTable);

    // 5. Seleccionar el próximo proceso
    Process* actual = schedulerSelectNext(ctrl->scheduler, ctrl->processTable);

    for (int i = 0; i < procesosEnEjecucion; i++) {
        Process* p = ctrl->processTable->runningProcesses[i];
        if (p && p->bcp && p->bcp->state == ProcessStateReady) p->bcp->timeInWaiting++;
    }
    for (int i = 0; i < procesosEnEspera; i++) {
        Process* p = ctrl->processTable->newRequests[i];
        if (p && p->bcp && p->bcp->state == ProcessStateReady) p->bcp->timeInWaiting++;
    }

    if (actual && actual->bcp) {
        Bcp* bcp = actual->bcp;
        bcpSetState(bcp, ProcessStateRunning);
        ctrl->currentProcess = actual;

        int instancia = randomCpuInstanceCycles();
        if (instancia > bcp->remainingCycles) instancia = bcp->remainingCycles;

        bcpUpdateRemainingTime(bcp, instancia);
        bcp->timeInExecution += instancia;
        bcp->quantumUsed += instancia;
        bcp->timesExecuted++;
        ctrl->processTable->totalCpuCyclesExecuted += instancia;

        int procIdx = findRunningSlot(ctrl, actual);

        // Crecimiento dinámico de memoria
        {
            int growth = randomMemoryGrowth();
            if (growth > 0) {
                if (procIdx >= 0) {
                    int newPages = bcp->pageCount + growth;
                    if (newPages > maxPaginasPorProceso) newPages = maxPaginasPorProceso;
                    if (pagingManagerResizeFrames(ctrl->pagingManager, procIdx, newPages) == 0)
                        bcp->pageCount = newPages;
                }
            }
        }

        bcp->contextSwitchTime = randomContextSwitchTime();
        bcpIncrementContextSwitches(bcp);
        ctrl->processTable->totalContextSwitches++;
        schedulerOnContextSwitch(ctrl->scheduler);
        if (ctrl->bcpLog) bcpLogRecordContextSwitch(ctrl->bcpLog, bcp);

        if (bcp->remainingCycles > 0 && randomInt(0, 100) < 30) {
            bcp->ioOperationsPending = 1;
            ioDispatcherLoadRandomPhrase(actual);
        }

        if (bcp->remainingCycles <= 0) {
            if (schedulerGetAlgorithm(ctrl->scheduler) == SchedulerAlgorithmRr)
                closePartialQuantum(ctrl, bcp, rrSchedulerGetCurrentQuantum(ctrl->rrScheduler));
            finishProcess(ctrl, actual, procIdx, ciclo);
            ctrl->currentProcess = NULL;

        } else if (bcp->ioOperationsPending > 0) {
            int dispositivo = bcp->timesInIo % numColasEs;
            if (schedulerGetAlgorithm(ctrl->scheduler) == SchedulerAlgorithmRr)
                closePartialQuantum(ctrl, bcp, rrSchedulerGetCurrentQuantum(ctrl->rrScheduler));
            if (moveToIo(ctrl, actual, dispositivo) == 0) {
                bcp->timesInIo++;
                ctrl->processTable->totalIoOperations++;
            } else {
                bcp->ioOperationsPending = 0;
                if (moveToReady(ctrl, actual) != 0)
                    bcpSetState(bcp, ProcessStateRunning);
            }
            ctrl->currentProcess = NULL;

        } else if (schedulerGetAlgorithm(ctrl->scheduler) == SchedulerAlgorithmRr &&
                   bcp->quantumUsed >= rrSchedulerGetCurrentQuantum(ctrl->rrScheduler)) {
            bcp->quantumUsed = 0;
            rrSchedulerOnQuantumExpired(ctrl->rrScheduler);
            if (ctrl->bcpLog) bcpLogRecordQuantumExpired(ctrl->bcpLog, bcp);
            if (moveToReady(ctrl, actual) != 0)
                bcpSetState(bcp, ProcessStateRunning);
            ctrl->currentProcess = NULL;

        } else {
            if (moveToReady(ctrl, actual) != 0)
                bcpSetState(bcp, ProcessStateRunning);
            ctrl->currentProcess = NULL;
        }
    }

    if (ctrl->currentProcess != NULL) {
        Bcp* pendingBcp = ctrl->currentProcess->bcp;
        if (pendingBcp && pendingBcp->remainingCycles > 0) {
            if (moveToReady(ctrl, ctrl->currentProcess) != 0) {
                bcpSetState(pendingBcp, ProcessStateRunning);
            }
        }
        ctrl->currentProcess = NULL;
    }

    ioDispatcherTick(ctrl->ioDispatcher);

    rebalancerTick(ctrl->rebalancer);
    if (rebalancerShouldCheck(ctrl->rebalancer)) {
        rrSchedulerUpdateProportions(ctrl->rrScheduler, ctrl->processTable);
        float propListos = ctrl->rrScheduler->proportionReady;
        if (rebalancerIsImbalanced(ctrl->rebalancer, propListos)) {
            int delta = rebalancerSuggestQuantumDelta(ctrl->rebalancer, propListos);
            int nuevoQuantum = ctrl->rrScheduler->currentQuantum + delta;
            if (nuevoQuantum < 5) nuevoQuantum = 5;  // CORREGIDO: mínimo 5 (1 causaba 250 ctx-switches/ciclo)
            ctrl->rrScheduler->currentQuantum = nuevoQuantum;
            rebalancerOnBalanceRestored(ctrl->rebalancer);
            menuShowBalanceAlert(ctrl->rrScheduler->proportionReady,
                                 ctrl->rrScheduler->proportionWaiting,
                                 nuevoQuantum);
        }
    }

    if (algorithmSwitcherShouldSwitch(ctrl->algorithmSwitcher, ctrl->scheduler, ctrl->processTable)) {
        algorithmSwitcherApply(ctrl->algorithmSwitcher, ctrl->scheduler);
        ctrl->processTable->algorithmChangeCount++;
    }

    if (schedulerGetAlgorithm(ctrl->scheduler) == SchedulerAlgorithmRr)
        rrSchedulerUpdateAgingRanking(ctrl->rrScheduler, ctrl->processTable);

    if (ciclo % growthListSize == 0)
        memoryResizerExecute(ctrl->memoryResizer, ctrl->processTable, ctrl->pagingManager);

    processTableUpdateAverages(ctrl->processTable);
    syncSimulationMetrics(ctrl);
    statsCollectorCollect(ctrl->statsCollector, ctrl->processTable);
    performanceBarUpdate(ctrl->performanceBar, ctrl->statsCollector);
    agingAnalysisCollectSample(ctrl->agingAnalysis, ctrl->processTable);

    // CONDICIÓN DE PARADA: esperar a que el generador haya entregado todos los
    // procesos Y todos hayan terminado (evita finalización prematura).
    if (!processGeneratorHasMore()
        && ctrl->processTable->finishedProcesses >= ctrl->processTable->totalProcesses
        && ctrl->processTable->totalProcesses > 0
        && !hasPendingNewRequests(ctrl->processTable)
        && readyQueueIsEmpty(ctrl->readyQueue)
        && !hasIoProcesses(ctrl->processTable)
        && ctrl->currentProcess == NULL) {
        if (ctrl->mainLogger)
            loggerLog(ctrl->mainLogger, LogLevelInfo, "Todos los procesos terminados. Finalizando simulación.");
        ctrl->running = 0;
    }

    return 0;
}

void systemControllerDestroy(SystemController* ctrl) {
    if (!ctrl) return;
    if (ctrl->agingAnalysis)        agingAnalysisDestroy(ctrl->agingAnalysis);
    if (ctrl->performanceBar)       performanceBarDestroy(ctrl->performanceBar);
    if (ctrl->statsCollector)       statsCollectorDestroy(ctrl->statsCollector);
    if (ctrl->memoryResizer)        memoryResizerDestroy(ctrl->memoryResizer);
    if (ctrl->pagingManager)        pagingManagerDestroy(ctrl->pagingManager);
    if (ctrl->swapManager)          swapManagerDestroy(ctrl->swapManager);
    if (ctrl->bitmapManager)        bitmapManagerDestroy(ctrl->bitmapManager);
    if (ctrl->ioCompletionHandler)  ioCompletionHandlerDestroy(ctrl->ioCompletionHandler);
    if (ctrl->ioDispatcher)         ioDispatcherDestroy(ctrl->ioDispatcher);
    if (ctrl->algorithmSwitcher)    algorithmSwitcherDestroy(ctrl->algorithmSwitcher);
    if (ctrl->preemptionController) preemptionControllerDestroy(ctrl->preemptionController);
    if (ctrl->rebalancer)           rebalancerDestroy(ctrl->rebalancer);
    if (ctrl->scheduler)            schedulerDestroy(ctrl->scheduler);
    if (ctrl->rrScheduler)          rrSchedulerDestroy(ctrl->rrScheduler);
    if (ctrl->fcfsScheduler)        fcfsSchedulerDestroy(ctrl->fcfsScheduler);
    processGeneratorCleanup();
    if (ctrl->processTable)         processTableDestroy(ctrl->processTable);
    if (ctrl->ioQueue)              ioQueueDestroy(ctrl->ioQueue);
    if (ctrl->readyQueue)           readyQueueDestroy(ctrl->readyQueue);
    if (ctrl->processLog)           processLogDestroy(ctrl->processLog);
    if (ctrl->bcpLog)               bcpLogDestroy(ctrl->bcpLog);
    if (ctrl->bcpLogger)            loggerDestroy(ctrl->bcpLogger);
    if (ctrl->mainLogger)           loggerDestroy(ctrl->mainLogger);
    wordLoaderCleanup();
    phraseLoaderCleanup();
    free(ctrl);
}

static void handleKeyX(SystemController* ctrl) {
    consoleIoClear();
    menuShowAlgorithmOptions();
    int opcion = menuGetAlgorithmChoice();
    if (opcion == 0) return;
    if (opcion == 1 || opcion == 2) systemControllerHandleCommand(ctrl, opcion);
}

static void handleKeyA(SystemController* ctrl) {
    rrSchedulerUpdateAgingRanking(ctrl->rrScheduler, ctrl->processTable);
    AgingRanking* r = &ctrl->rrScheduler->agingRanking;
    consoleIoClear();
    menuShowTop5Aged((const char(*)[idProcesoLen])r->processIds, r->wasteValues, r->count);
    menuShowTop5Wasters((const char(*)[idProcesoLen])r->processIds, r->wasteValues, r->count);
    char idElegido[idProcesoLen];
    memset(idElegido, 0, sizeof(idElegido));
    if (menuGetPrivilegedProcessId(idElegido, idProcesoLen) > 0 && idElegido[0] != '\0') {
        rrSchedulerPrioritizeProcess(ctrl->rrScheduler, idElegido);
        if (ctrl->mainLogger)
            loggerLogFormat(ctrl->mainLogger, LogLevelInfo, "Tecla A: proceso %s marcado como privilegiado.", idElegido);
    }
}

int systemControllerRun(SystemController* ctrl) {
    if (!ctrl) return -1;
    consoleIoInit();
    int mode = menuGetExecutionMode();
    if (mode == 2) {
        systemControllerSetRunMode(ctrl, SystemRunModePvm);
        consoleIoPrintLine("Modo PVM activado");
        int ret = systemControllerRunPvm(ctrl);
        consoleIoCleanup();
        return ret;
    }

    systemControllerSetRunMode(ctrl, SystemRunModeLocal);
    consoleIoPrintLine("Modo LOCAL activado");
    menuShowMain();
    if (ctrl->mainLogger) loggerLog(ctrl->mainLogger, LogLevelInfo, "Bucle de simulación iniciado.");
    while (ctrl->running) {
        int ret = systemControllerCycle(ctrl);
        if (ret != 0) {
            if (ctrl->mainLogger) loggerLogFormat(ctrl->mainLogger, LogLevelError,
                "systemControllerCycle devolvió %d en ciclo %d. Abortando.", ret, ctrl->processTable->currentCycle);
            ctrl->running = 0;
            break;
        }
        if (ctrl->processTable->currentCycle % 10 == 0) {
            consoleIoClear();
            menuShowPerformanceBars(ctrl->performanceBar);
            menuShowMemoryStats(ctrl->statsCollector);
        }
        if (ctrl->processTable->totalProcesses > 0 &&
            ctrl->processTable->finishedProcesses >= ctrl->processTable->totalProcesses &&
            !processGeneratorHasMore() &&
            !hasPendingNewRequests(ctrl->processTable) &&
            readyQueueIsEmpty(ctrl->readyQueue) &&
            !hasIoProcesses(ctrl->processTable)) {
            if (ctrl->mainLogger) loggerLog(ctrl->mainLogger, LogLevelInfo, "Todos los procesos terminaron. Simulación finalizada.");
            ctrl->running = 0;
            break;
        }
        if (!consoleIoKbhit()) {
            timeHelperDelay(retardoSimulacionMs);
            continue;
        }
        char tecla = consoleIoGetChar();
        switch (tecla) {
            case 'X': case 'x': handleKeyX(ctrl); break;
            case 'A': case 'a':
                if (schedulerGetAlgorithm(ctrl->scheduler) == SchedulerAlgorithmRr) handleKeyA(ctrl);
                else consoleIoPrintLine("El ranking de envejecimiento solo está disponible en modo RR.");
                break;
            case 'P': case 'p': {
                consoleIoPrintLine("[PAUSADO] Presione P nuevamente para reanudar...");
                char reanudar = 0;
                while (reanudar != 'P' && reanudar != 'p') {
                    if (consoleIoKbhit()) reanudar = consoleIoGetChar();
                    else timeHelperDelay(50);
                }
                consoleIoPrintLine("[REANUDADO]");
                break;
            }
            case 'Q': case 'q': case 27:
                if (ctrl->mainLogger) loggerLog(ctrl->mainLogger, LogLevelInfo, "Usuario solicitó detener la simulación.");
                ctrl->running = 0;
                break;
            default: systemControllerHandleCommand(ctrl, (int)tecla); break;
        }
    }
    consoleIoClear();
    if (schedulerGetAlgorithm(ctrl->scheduler) == SchedulerAlgorithmRr) {
        rrSchedulerUpdateAgingRanking(ctrl->rrScheduler, ctrl->processTable);
        AgingRanking* r = &ctrl->rrScheduler->agingRanking;
        menuShowTop5Aged((const char(*)[idProcesoLen])r->processIds, r->wasteValues, r->count);
        menuShowTop5Wasters((const char(*)[idProcesoLen])r->processIds, r->wasteValues, r->count);
    }
    menuShowPerformanceBars(ctrl->performanceBar);
    menuShowMemoryStats(ctrl->statsCollector);
    consoleIoPrintSeparator();
    consoleIoPrintInt("Ciclos totales:        ", ctrl->processTable->currentCycle);
    consoleIoPrintInt("Procesos terminados:   ", ctrl->processTable->finishedProcesses);
    consoleIoPrintInt("Cambios de contexto:   ", ctrl->processTable->totalContextSwitches);
    consoleIoPrintInt("Cambios de algoritmo:  ", ctrl->processTable->algorithmChangeCount);
    consoleIoPrintSeparator();
    if (ctrl->mainLogger) {
        loggerLog(ctrl->mainLogger, LogLevelInfo, "Bucle de simulación finalizado.");
        loggerFlush(ctrl->mainLogger);
    }
    if (ctrl->bcpLogger) loggerFlush(ctrl->bcpLogger);
    consoleIoCleanup();
    return 0;
}

int systemControllerRunPvm(SystemController* ctrl) {
    if (!ctrl) { errorHandlerLog(ErrorCodeInvalidArgument, "systemControllerRunPvm: ctrl es NULL"); return -1; }
#if !pvmModeEnabled
    consoleIoPrintLine("PVM no está habilitado en esta compilación.");
    return -1;
#else
    consoleIoPrintLine("Inicializando maestro PVM...");
    PvmMaster* master = pvmMasterInit();
    if (!master) { errorHandlerLog(ErrorCodeNodeConnectionFailed, "systemControllerRunPvm: no se pudo inicializar pvmMaster"); return -1; }
    master->processTable = ctrl->processTable;
    master->rrScheduler = ctrl->rrScheduler;
    consoleIoPrintLine("Lanzando procesos esclavos...");
    int numEsclavos = pvmMasterSpawnSlaves(master);
    if (numEsclavos != pvmNumEsclavos) {
        errorHandlerLog(ErrorCodeNodeConnectionFailed, "systemControllerRunPvm: no se pudieron lanzar todos los esclavos");
        pvmMasterCleanup(master);
        return -1;
    }
    consoleIoPrintLine("Ejecutando Tarea 1: Estadísticas de procesos...");
    pvmMasterTask1Stats(master);
    consoleIoPrintLine("Ejecutando Tarea 2: Análisis de Round-Robin...");
    pvmMasterTask2Aging(master);
    consoleIoPrintLine("Integrando resultados...");
    pvmMasterIntegrateResults(master);
    consoleIoPrintLine("Imprimiendo resultados finales...");
    pvmMasterPrintResults(master);
    pvmMasterCleanup(master);
    consoleIoPrintLine("Tareas distribuidas completadas.");
    return 0;
#endif
}

int systemControllerHandleCommand(SystemController* ctrl, int command) {
    if (!ctrl || !ctrl->scheduler || !ctrl->rrScheduler || !ctrl->algorithmSwitcher || !ctrl->processTable) return -1;
    switch (command) {
        case 1:
            algorithmSwitcherSetNext(ctrl->algorithmSwitcher, SchedulerAlgorithmFcfs);
            algorithmSwitcherApply(ctrl->algorithmSwitcher, ctrl->scheduler);
            ctrl->processTable->algorithmChangeCount++;
            if (ctrl->mainLogger) loggerLogFormat(ctrl->mainLogger, LogLevelInfo, "Comando 1: cambio manual a FCFS");
            return 0;
        case 2:
            menuShowQuantumPrompt();
            int quantum = menuGetQuantumInput();
            if (quantum > 0) ctrl->rrScheduler->currentQuantum = quantum;
            algorithmSwitcherSetNext(ctrl->algorithmSwitcher, SchedulerAlgorithmRr);
            algorithmSwitcherApply(ctrl->algorithmSwitcher, ctrl->scheduler);
            ctrl->processTable->algorithmChangeCount++;
            if (ctrl->mainLogger) loggerLogFormat(ctrl->mainLogger, LogLevelInfo, "Comando 2: cambio manual a RR con quantum %d", quantum);
            return 0;
        case 3: {
            char processId[idProcesoLen];
            memset(processId, 0, sizeof(processId));
            if (menuGetPrivilegedProcessId(processId, idProcesoLen) > 0 && processId[0] != '\0') {
                rrSchedulerPrioritizeProcess(ctrl->rrScheduler, processId);
                if (ctrl->mainLogger) loggerLogFormat(ctrl->mainLogger, LogLevelInfo, "Comando 3: proceso %s privilegiado", processId);
            }
            return 0;
        }
        default: return -1;
    }
}
