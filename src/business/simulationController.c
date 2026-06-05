// Orquestador principal del simulador: inicializa, coordina y destruye todos los subsistemas.

#include "simulationController.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../utils/constants.h"
#include "../utils/random.h"
#include "../data/wordLoader.h"
#include "../data/phraseLoader.h"
#include "../utils/timeHelper.h"
#include "../utils/errorHandler.h"

#include "../domain/core/bcp.h"
#include "../domain/core/process.h"
#include "../domain/core/processTable.h"
#include "../domain/core/processGenerator.h"

#include "../domain/core/readyQueue.h"
#include "../domain/core/fcfsScheduler.h"
#include "../domain/core/rrScheduler.h"
#include "../domain/core/scheduler.h"
#include "../domain/core/rebalancing.h"
#include "../domain/core/preemption.h"
#include "../domain/core/algorithmSwitcher.h"

#include "../domain/core/ioQueue.h"
#include "../domain/core/ioDispatcher.h"
#include "../domain/core/ioCompletionHandler.h"

#include "../domain/memory/memoria.h"
#include "../domain/memory/bitmapManager.h"
#include "../domain/memory/fifoReplacement.h"
#include "../domain/memory/swapManager.h"
#include "../domain/memory/pagingManager.h"
#include "../domain/memory/memoryResize.h"

#include "../domain/core/statsCollector.h"
#include "../domain/core/performanceBar.h"
#include "../domain/core/agingAnalysis.h"

#if pvmModeEnabled
#include "../domain/distributed/realPvmMaster.h"
#endif

#include "../data/logger.h"
#include "../data/bcpLog.h"
#include "../data/processLog.h"

#include "../presentation/consoleIo.h"
#include "guiController.h"
#include "pvmController.h"
#include "memoryController.h"

struct SimulationController {
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
    MemoryController*      memoryController;
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
    PvmController*          pvmController;
    int                    running;
    SimulationRunMode          runMode;
    Process*               currentProcess;
};

static int preloadGeneratedProcesses(SimulationController* ctrl);
static int activateProcessInSlot(SimulationController* ctrl, int slot, int ciclo);
static void activateCreatedProcesses(SimulationController* ctrl, int ciclo);
static void admitNewRequests(SimulationController* ctrl, int ciclo);
static Process* findProcessById(SimulationController* ctrl, const char* processId);
static int normalizeEvenPageCount(int pageCount);

SimulationController* simulationControllerCreate(void) {
    SimulationController* ctrl = (SimulationController*)malloc(sizeof(SimulationController));
    if (!ctrl) {
        fprintf(stderr, "[SimulationController] Error: no se pudo asignar memoria.\n");
        return NULL;
    }
    memset(ctrl, 0, sizeof(SimulationController));
    ctrl->running = 0;
    ctrl->runMode = SimulationRunModeLocal;
    ctrl->currentProcess = NULL;
    return ctrl;
}

void simulationControllerSetRunMode(SimulationController* ctrl, SimulationRunMode mode) {
    if (!ctrl) return;
    ctrl->runMode = (mode == SimulationRunModePvm) ? SimulationRunModePvm : SimulationRunModeLocal;
}

SimulationRunMode simulationControllerGetRunMode(const SimulationController* ctrl) {
    return ctrl ? ctrl->runMode : SimulationRunModeLocal;
}

int simulationControllerInit(SimulationController* ctrl) {
    if (!ctrl) return -1;

    initRandom(0);
    wordLoaderInit("libro1.odt");
    phraseLoaderInit("frases.odt");

    ctrl->mainLogger = loggerCreate("simulation.log", LogLevelInfo);
    if (!ctrl->mainLogger)
        fprintf(stderr, "[SimulationController] Advertencia: no se pudo abrir simulation.log\n");

    ctrl->bcpLogger = loggerCreate("bcp.log", LogLevelInfo);
    if (!ctrl->bcpLogger)
        fprintf(stderr, "[SimulationController] Advertencia: no se pudo abrir bcp.log\n");

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

    ctrl->memoryController = memoryControllerCreate();
    if (!ctrl->memoryController) goto rollback_iocomp;
    ctrl->bitmapManager = ctrl->memoryController->bitmapManager;
    ctrl->swapManager = ctrl->memoryController->swapManager;
    ctrl->pagingManager = ctrl->memoryController->pagingManager;
    ctrl->memoryResizer = ctrl->memoryController->memoryResizer;

    ctrl->statsCollector = statsCollectorCreate();
    if (!ctrl->statsCollector) goto rollback_resizer;

    ctrl->performanceBar = performanceBarCreate();
    if (!ctrl->performanceBar) goto rollback_stats;

    ctrl->agingAnalysis = agingAnalysisCreate();
    if (!ctrl->agingAnalysis) goto rollback_perfbar;

    if (preloadGeneratedProcesses(ctrl) != 0) goto rollback_aging;

    ctrl->running = 1;
    if (ctrl->mainLogger)
        loggerLog(ctrl->mainLogger, LogLevelInfo, "SimulationController inicializado correctamente.");
    return 0;

rollback_aging:     agingAnalysisDestroy(ctrl->agingAnalysis);
rollback_perfbar:   performanceBarDestroy(ctrl->performanceBar);
rollback_stats:     statsCollectorDestroy(ctrl->statsCollector);
rollback_resizer:   memoryControllerDestroy(ctrl->memoryController);
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

static int findRunningSlot(SimulationController* ctrl, Process* process) {
    if (!ctrl || !process) return -1;
    for (int i = 0; i < procesosEnEjecucion; i++)
        if (ctrl->processTable->runningProcesses[i] == process) return i;
    return -1;
}

static int findFreeRunningSlot(SimulationController* ctrl) {
    if (!ctrl) return -1;
    for (int i = 0; i < procesosEnEjecucion; i++)
        if (ctrl->processTable->runningProcesses[i] == NULL) return i;
    return -1;
}

static int moveToReady(SimulationController* ctrl, Process* process) {
    if (!ctrl || !process || !process->bcp) return -1;
    if (readyQueueEnqueue(ctrl->readyQueue, process) != 0) return -1;
    bcpSetState(process->bcp, ProcessStateReady);
    process->bcp->timesReturnedToReady++;
    return 0;
}

static int moveToIo(SimulationController* ctrl, Process* process, int device) {
    if (!ctrl || !process || !process->bcp) return -1;
    if (ioDispatcherDispatch(ctrl->ioDispatcher, process, device) != 0) return -1;
    bcpSetState(process->bcp, ProcessStateWaitingIo);
    return 0;
}

static void syncBcpMemoryFields(Bcp* bcp, int slot) {
    if (!bcp || slot < 0) return;
    bcp->pageTableBase = slot * maxPaginasPorProceso;
    if (bcp->memoryRequested <= 0)
        bcp->memoryRequested = bcp->pageCount * palabrasPorPagina;
    bcp->totalMemoryAllocated = bcp->pageCount * palabrasPorPagina;
}

static int normalizeEvenPageCount(int pageCount) {
    if (pageCount < marcosMin) pageCount = marcosMin;
    if (pageCount > marcosMax) pageCount = marcosMax;
    if ((pageCount % 2) != 0) pageCount++;
    if (pageCount > marcosMax) pageCount = marcosMax;
    return pageCount;
}

static int preloadGeneratedProcesses(SimulationController* ctrl) {
    if (!ctrl || !ctrl->processTable) return -1;
    int count = processGeneratorGetCount();
    if (count < totalProcesos) return -1;

    ctrl->processTable->totalProcesses = count;
    for (int i = 0; i < count; i++) {
        Bcp* bcp = processGeneratorGetByIndex(i);
        if (!bcp) return -1;
        bcp->pageCount = normalizeEvenPageCount(bcp->pageCount);
        bcp->memoryRequested = bcp->pageCount * palabrasPorPagina;
        bcp->totalMemoryAllocated = bcp->memoryRequested;
        bcpSetState(bcp, ProcessStateNew);

        Process* proc = processCreateWithBcp(bcp);
        if (!proc) return -1;
        processActivate(proc);

        if (i < procesosEnEjecucion) {
            ctrl->processTable->runningProcesses[i] = proc;
            syncBcpMemoryFields(bcp, i);
        } else {
            int newIndex = i - procesosEnEjecucion;
            if (newIndex >= procesosEnEspera) {
                processDestroy(proc);
                return -1;
            }
            ctrl->processTable->newRequests[newIndex] = proc;
            bcp->pageTableBase = -1;
        }
    }
    return 0;
}

static int activateProcessInSlot(SimulationController* ctrl, int slot, int ciclo) {
    if (!ctrl || slot < 0 || slot >= procesosEnEjecucion) return -1;
    Process* proc = ctrl->processTable->runningProcesses[slot];
    if (!proc || !proc->bcp) return -1;
    Bcp* bcp = proc->bcp;
    if (bcp->state != ProcessStateNew) return 0;
    if (bcp->creationTime > ciclo) return 0;
    if (readyQueueIsFull(ctrl->readyQueue)) return -1;

    syncBcpMemoryFields(bcp, slot);
    if (memoryControllerAllocateProcess(ctrl->memoryController, slot, bcp->pageCount) != 0 &&
        ctrl->mainLogger)
        loggerLogFormat(ctrl->mainLogger, LogLevelWarning,
                        "No se pudo inicializar tabla de paginas para %s", bcp->processId);

    if (moveToReady(ctrl, proc) != 0) return -1;
    if (ctrl->processLog) processLogRecordCreation(ctrl->processLog, proc);
    if (ctrl->bcpLog) bcpLogRecordFull(ctrl->bcpLog, bcp);
    return 0;
}

static void activateCreatedProcesses(SimulationController* ctrl, int ciclo) {
    if (!ctrl) return;
    for (int i = 0; i < procesosEnEjecucion; i++) {
        if (readyQueueIsFull(ctrl->readyQueue)) break;
        activateProcessInSlot(ctrl, i, ciclo);
    }
}

static void admitNewRequests(SimulationController* ctrl, int ciclo) {
    if (!ctrl) return;
    for (int i = 0; i < procesosEnEspera; i++) {
        if (readyQueueIsFull(ctrl->readyQueue)) break;
        Process* espera = ctrl->processTable->newRequests[i];
        if (!espera || !espera->bcp) continue;
        if (espera->bcp->creationTime > ciclo) continue;

        int slotLibre = findFreeRunningSlot(ctrl);
        if (slotLibre == -1) break;

        ctrl->processTable->runningProcesses[slotLibre] = espera;
        ctrl->processTable->newRequests[i] = NULL;
        syncBcpMemoryFields(espera->bcp, slotLibre);
        if (activateProcessInSlot(ctrl, slotLibre, ciclo) != 0) {
            ctrl->processTable->newRequests[i] = espera;
            ctrl->processTable->runningProcesses[slotLibre] = NULL;
            bcpSetState(espera->bcp, ProcessStateNew);
            break;
        }
    }
}

static void closePartialQuantum(SimulationController* ctrl, Bcp* bcp, int quantum) {
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

static void syncSimulationMetrics(SimulationController* ctrl) {
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

    int internalWaste = 0;
    for (int i = 0; i < procesosEnEjecucion; i++) {
        Process* p = table->runningProcesses[i];
        if (p && p->bcp && p->bcp->state != ProcessStateFinished &&
            p->bcp->totalMemoryAllocated > p->bcp->memoryRequested)
            internalWaste += p->bcp->totalMemoryAllocated - p->bcp->memoryRequested;
    }
    for (int i = 0; i < procesosEnEspera; i++) {
        Process* p = table->newRequests[i];
        if (p && p->bcp && p->bcp->state != ProcessStateFinished &&
            p->bcp->totalMemoryAllocated > p->bcp->memoryRequested)
            internalWaste += p->bcp->totalMemoryAllocated - p->bcp->memoryRequested;
    }
    table->internalWaste = internalWaste;

    int accountedCpu = table->totalCpuCyclesExecuted + table->totalCpuWasteCycles;
    table->cpuWasteRatio = accountedCpu > 0
        ? (float)table->totalCpuWasteCycles / (float)accountedCpu
        : 0.0f;
}

static void finishProcess(SimulationController* ctrl, Process* process, int slot, int ciclo) {
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

static Process* findProcessById(SimulationController* ctrl, const char* processId) {
    if (!ctrl || !processId || processId[0] == '\0') return NULL;
    for (int i = 0; i < procesosEnEjecucion; i++) {
        Process* p = ctrl->processTable->runningProcesses[i];
        if (p && p->bcp && strcmp(p->bcp->processId, processId) == 0)
            return p;
    }
    for (int i = 0; i < procesosEnEspera; i++) {
        Process* p = ctrl->processTable->newRequests[i];
        if (p && p->bcp && strcmp(p->bcp->processId, processId) == 0)
            return p;
    }
    return NULL;
}

int simulationControllerCycle(SimulationController* ctrl) {
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

    activateCreatedProcesses(ctrl, ciclo);
    admitNewRequests(ctrl, ciclo);

    ioCompletionHandlerProcess(ctrl->ioCompletionHandler, ctrl->ioQueue, ctrl->readyQueue,
                               ctrl->pagingManager, ctrl->processTable);

    int currentAlgorithm = schedulerGetAlgorithm(ctrl->scheduler);
    if (ctrl->currentProcess && currentAlgorithm != SchedulerAlgorithmFcfs) {
        Bcp* pendingBcp = ctrl->currentProcess->bcp;
        if (pendingBcp && pendingBcp->state == ProcessStateRunning && pendingBcp->remainingCycles > 0) {
            if (moveToReady(ctrl, ctrl->currentProcess) == 0)
                ctrl->currentProcess = NULL;
        } else {
            ctrl->currentProcess = NULL;
        }
    }

    Process* actual = NULL;
    int continuingCurrent = 0;
    if (currentAlgorithm == SchedulerAlgorithmFcfs &&
        ctrl->currentProcess &&
        ctrl->currentProcess->bcp &&
        ctrl->currentProcess->bcp->state == ProcessStateRunning) {
        actual = ctrl->currentProcess;
        continuingCurrent = 1;
    } else {
        actual = schedulerSelectNext(ctrl->scheduler, ctrl->processTable);
        if (actual && actual->bcp) {
            bcpSetState(actual->bcp, ProcessStateRunning);
            ctrl->currentProcess = actual;
        }
    }

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
        int isRr = currentAlgorithm == SchedulerAlgorithmRr;
        int quantum = rrSchedulerGetCurrentQuantum(ctrl->rrScheduler);
        if (isRr && bcp->quantumAssigned != quantum) bcp->quantumAssigned = quantum;

        int instancia = bcp->currentTimeSlice > 0 ? bcp->currentTimeSlice : randomCpuInstanceCycles();
        if (instancia > bcp->remainingCycles) instancia = bcp->remainingCycles;

        int ciclosAEjecutar = instancia;
        if (isRr) {
            int quantumRestante = quantum - bcp->quantumUsed;
            if (quantumRestante <= 0) quantumRestante = quantum;
            if (ciclosAEjecutar > quantumRestante)
                ciclosAEjecutar = quantumRestante;
        }
        bcp->currentTimeSlice = instancia > ciclosAEjecutar ? instancia - ciclosAEjecutar : 0;
        bcpUpdateRemainingTime(bcp, ciclosAEjecutar);
        bcp->timeInExecution += ciclosAEjecutar;
        if (isRr) bcp->quantumUsed += ciclosAEjecutar;
        if (bcp->timesExecuted == 0) bcp->startTime = ciclo;
        bcp->timesExecuted++;
        ctrl->processTable->totalCpuCyclesExecuted += ciclosAEjecutar;

        int procIdx = findRunningSlot(ctrl, actual);

        {
            int growthWords = randomMemoryGrowth();
            if (growthWords > 0) {
                if (procIdx >= 0) {
                    int maxWords = marcosMax * palabrasPorPagina;
                    int requestedWords = bcp->memoryRequested + growthWords;
                    if (requestedWords > maxWords) requestedWords = maxWords;
                    int pagesNeeded = (requestedWords + palabrasPorPagina - 1) / palabrasPorPagina;
                    int newPages = normalizeEvenPageCount(pagesNeeded);
                    if (newPages < bcp->pageCount) newPages = bcp->pageCount;
                    bcp->memoryRequested = requestedWords;
                    if (memoryControllerGrowProcess(ctrl->memoryController, procIdx, newPages) == 0) {
                        bcp->pageCount = normalizeEvenPageCount(newPages);
                        syncBcpMemoryFields(bcp, procIdx);
                        if (ctrl->bcpLog) bcpLogRecordFull(ctrl->bcpLog, bcp);
                    }
                }
            }
        }

        if (!continuingCurrent) {
            bcp->contextSwitchTime = randomContextSwitchTime();
            ctrl->processTable->totalContextSwitchTime += bcp->contextSwitchTime;
            bcpIncrementContextSwitches(bcp);
            ctrl->processTable->totalContextSwitches++;
            schedulerOnContextSwitch(ctrl->scheduler);
            if (ctrl->bcpLog) bcpLogRecordContextSwitch(ctrl->bcpLog, bcp);
            if (ctrl->bcpLog) bcpLogRecordFull(ctrl->bcpLog, bcp);
        }

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
                if (moveToReady(ctrl, actual) != 0) {
                    bcpSetState(bcp, ProcessStateRunning);
                    ctrl->currentProcess = actual;
                }
            }
            if (bcp->state != ProcessStateRunning)
                ctrl->currentProcess = NULL;

        } else if (schedulerGetAlgorithm(ctrl->scheduler) == SchedulerAlgorithmRr &&
                   bcp->quantumUsed >= rrSchedulerGetCurrentQuantum(ctrl->rrScheduler)) {
            bcp->quantumUsed = 0;
            rrSchedulerOnQuantumExpired(ctrl->rrScheduler);
            if (ctrl->bcpLog) bcpLogRecordQuantumExpired(ctrl->bcpLog, bcp);
            if (ctrl->bcpLog) bcpLogRecordFull(ctrl->bcpLog, bcp);
            if (moveToReady(ctrl, actual) != 0) {
                bcpSetState(bcp, ProcessStateRunning);
                ctrl->currentProcess = actual;
            } else {
                ctrl->currentProcess = NULL;
            }

        } else {
            if (isRr) {
                if (bcp->currentTimeSlice == 0) closePartialQuantum(ctrl, bcp, quantum);
                if (moveToReady(ctrl, actual) != 0) {
                    bcpSetState(bcp, ProcessStateRunning);
                    ctrl->currentProcess = actual;
                } else {
                    ctrl->currentProcess = NULL;
                }
            } else {
                bcpSetState(bcp, ProcessStateRunning);
                ctrl->currentProcess = actual;
            }
        }
    }

    ioDispatcherTick(ctrl->ioDispatcher);

    if (schedulerGetAlgorithm(ctrl->scheduler) == SchedulerAlgorithmRr) {
        rebalancerTick(ctrl->rebalancer);
        if (rebalancerShouldCheck(ctrl->rebalancer)) {
            rrSchedulerUpdateProportions(ctrl->rrScheduler, ctrl->processTable);
            float propListos = ctrl->rrScheduler->proportionReady;
            if (rebalancerIsImbalanced(ctrl->rebalancer, propListos)) {
                int delta = rebalancerSuggestQuantumDelta(ctrl->rebalancer, propListos);
                int nuevoQuantum = ctrl->rrScheduler->currentQuantum + delta;
                if (nuevoQuantum < 5) nuevoQuantum = 5;
                ctrl->rrScheduler->currentQuantum = nuevoQuantum;
                rebalancerOnBalanceRestored(ctrl->rebalancer);
                guiControllerShowBalanceAlert(ctrl->rrScheduler->proportionReady,
                                     ctrl->rrScheduler->proportionWaiting,
                                     nuevoQuantum);
            }
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

    if (ctrl->processTable->finishedProcesses >= ctrl->processTable->totalProcesses
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

void simulationControllerDestroy(SimulationController* ctrl) {
    if (!ctrl) return;
    if (ctrl->pvmController)         pvmControllerDestroy(ctrl->pvmController);
    if (ctrl->agingAnalysis)        agingAnalysisDestroy(ctrl->agingAnalysis);
    if (ctrl->performanceBar)       performanceBarDestroy(ctrl->performanceBar);
    if (ctrl->statsCollector)       statsCollectorDestroy(ctrl->statsCollector);
    if (ctrl->memoryController)     memoryControllerDestroy(ctrl->memoryController);
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

static void handleKeyX(SimulationController* ctrl) {
    consoleIoClear();
    guiControllerShowAlgorithmOptions();
    int opcion = guiControllerGetAlgorithmChoice();
    if (opcion == 0) return;
    if (opcion == 1 || opcion == 2) simulationControllerHandleCommand(ctrl, opcion);
}

static void handleKeyA(SimulationController* ctrl) {
    rrSchedulerUpdateAgingRanking(ctrl->rrScheduler, ctrl->processTable);
    AgingRanking* r = &ctrl->rrScheduler->agingRanking;
    consoleIoClear();
    guiControllerShowTop5Aged((const char(*)[idProcesoLen])r->processIds, r->wasteValues, r->count);
    guiControllerShowTop5Wasters((const char(*)[idProcesoLen])r->wasterProcessIds, r->wasterWasteValues, r->wasterCount);
    char idElegido[idProcesoLen];
    memset(idElegido, 0, sizeof(idElegido));
    if (guiControllerGetPrivilegedProcessId(idElegido, idProcesoLen) > 0 && idElegido[0] != '\0') {
        Process* selected = findProcessById(ctrl, idElegido);
        if (selected && selected->bcp && selected->bcp->state != ProcessStateFinished) {
            rrSchedulerPrioritizeProcess(ctrl->rrScheduler, idElegido);
            if (ctrl->mainLogger)
                loggerLogFormat(ctrl->mainLogger, LogLevelInfo, "Tecla A: proceso %s marcado como privilegiado.", idElegido);
        } else {
            consoleIoPrintLine("ID inválido o proceso finalizado.");
        }
    }
}

int simulationControllerRun(SimulationController* ctrl) {
    return simulationControllerRunFull(ctrl, pvmModeFake);
}

int simulationControllerRunFull(SimulationController* ctrl, PvmMode pvmMode) {
    if (!ctrl) return -1;
    consoleIoInit();
    simulationControllerSetRunMode(ctrl, SimulationRunModeLocal);
    const char* modeName = pvmMode == pvmModeReal
        ? "Simulación completa con PVM real"
        : "Simulación con PVM simulado";
    ctrl->pvmController = pvmControllerCreate(pvmMode);
    if (!ctrl->pvmController || pvmControllerStart(ctrl->pvmController) != 0) {
        consoleIoPrintLine("No se pudo inicializar el controlador PVM.");
        if (ctrl->pvmController) {
            pvmControllerDestroy(ctrl->pvmController);
            ctrl->pvmController = NULL;
        }
        consoleIoCleanup();
        return -1;
    }
    if (ctrl->mainLogger) loggerLog(ctrl->mainLogger, LogLevelInfo, "Bucle de simulación iniciado.");
    syncSimulationMetrics(ctrl);
    statsCollectorCollect(ctrl->statsCollector, ctrl->processTable);
    performanceBarUpdate(ctrl->performanceBar, ctrl->statsCollector);
    guiControllerShowDashboard(modeName,
                               ctrl->processTable,
                               ctrl->statsCollector,
                               ctrl->performanceBar,
                               schedulerGetAlgorithm(ctrl->scheduler),
                               ctrl->rrScheduler ? ctrl->rrScheduler->currentQuantum : 0);
    while (ctrl->running) {
        int ret = simulationControllerCycle(ctrl);
        if (ret != 0) {
            if (ctrl->mainLogger) loggerLogFormat(ctrl->mainLogger, LogLevelError,
                "simulationControllerCycle devolvió %d en ciclo %d. Abortando.", ret, ctrl->processTable->currentCycle);
            ctrl->running = 0;
            break;
        }
        if (ctrl->processTable->currentCycle % 10 == 0) {
            guiControllerShowDashboard(modeName,
                                       ctrl->processTable,
                                       ctrl->statsCollector,
                                       ctrl->performanceBar,
                                       schedulerGetAlgorithm(ctrl->scheduler),
                                       ctrl->rrScheduler ? ctrl->rrScheduler->currentQuantum : 0);
        }
        if (ctrl->processTable->currentCycle % 20 == 0) {
            /* Ejecuta el análisis distribuido sin detener el ciclo principal. */
            pvmControllerRunStatsTask(ctrl->pvmController, ctrl->processTable);
            pvmControllerRunAgingTask(ctrl->pvmController, ctrl->processTable, ctrl->rrScheduler);
        }
        if (ctrl->processTable->totalProcesses > 0 &&
            ctrl->processTable->finishedProcesses >= ctrl->processTable->totalProcesses &&
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
            default: simulationControllerHandleCommand(ctrl, (int)tecla); break;
        }
    }
    guiControllerShowDashboard(modeName,
                               ctrl->processTable,
                               ctrl->statsCollector,
                               ctrl->performanceBar,
                               schedulerGetAlgorithm(ctrl->scheduler),
                               ctrl->rrScheduler ? ctrl->rrScheduler->currentQuantum : 0);
    if (schedulerGetAlgorithm(ctrl->scheduler) == SchedulerAlgorithmRr) {
        rrSchedulerUpdateAgingRanking(ctrl->rrScheduler, ctrl->processTable);
        AgingRanking* r = &ctrl->rrScheduler->agingRanking;
        guiControllerShowTop5Aged((const char(*)[idProcesoLen])r->processIds, r->wasteValues, r->count);
        guiControllerShowTop5Wasters((const char(*)[idProcesoLen])r->wasterProcessIds, r->wasterWasteValues, r->wasterCount);
    }
    consoleIoPrintSeparator();
    pvmControllerRunStatsTask(ctrl->pvmController, ctrl->processTable);
    pvmControllerRunAgingTask(ctrl->pvmController, ctrl->processTable, ctrl->rrScheduler);
    pvmControllerPrintResults(ctrl->pvmController);
    pvmControllerDestroy(ctrl->pvmController);
    ctrl->pvmController = NULL;
    if (ctrl->mainLogger) {
        loggerLog(ctrl->mainLogger, LogLevelInfo, "Bucle de simulación finalizado.");
        loggerFlush(ctrl->mainLogger);
    }
    if (ctrl->bcpLogger) loggerFlush(ctrl->bcpLogger);
    consoleIoCleanup();
    return 0;
}

int simulationControllerRunPvm(SimulationController* ctrl) {
    if (!ctrl) { errorHandlerLog(ErrorCodeInvalidArgument, "simulationControllerRunPvm: ctrl es NULL"); return -1; }
#if !pvmModeEnabled
    consoleIoPrintLine("PVM no está habilitado en esta compilación.");
    return -1;
#else
    int warmupCycles = 1000;
    const char* warmupEnv = getenv("PVM_WARMUP_CYCLES");
    if (warmupEnv && warmupEnv[0] != '\0') {
        int parsed = atoi(warmupEnv);
        if (parsed > 0) warmupCycles = parsed;
    }

    consoleIoPrintLine("Preparando datos locales para analisis distribuido...");
    algorithmSwitcherSetNext(ctrl->algorithmSwitcher, SchedulerAlgorithmRr);
    algorithmSwitcherApply(ctrl->algorithmSwitcher, ctrl->scheduler);
    ctrl->processTable->algorithmChangeCount++;

    for (int i = 0; i < warmupCycles && ctrl->running; i++) {
        int ret = simulationControllerCycle(ctrl);
        if (ret != 0) {
            errorHandlerLog(ErrorCodeInvalidArgument, "simulationControllerRunPvm: fallo durante calentamiento local");
            return -1;
        }
    }

    consoleIoPrintLine("Inicializando maestro PVM...");
    PvmMaster* master = pvmMasterInit();
    if (!master) { errorHandlerLog(ErrorCodeNodeConnectionFailed, "simulationControllerRunPvm: no se pudo inicializar pvmMaster"); return -1; }
    master->processTable = ctrl->processTable;
    master->rrScheduler = ctrl->rrScheduler;
    consoleIoPrintLine("Lanzando procesos esclavos...");
    int numEsclavos = pvmMasterSpawnSlaves(master);
    if (numEsclavos != pvmNumEsclavos) {
        errorHandlerLog(ErrorCodeNodeConnectionFailed, "simulationControllerRunPvm: no se pudieron lanzar todos los esclavos");
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

int simulationControllerHandleCommand(SimulationController* ctrl, int command) {
    if (!ctrl || !ctrl->scheduler || !ctrl->rrScheduler || !ctrl->algorithmSwitcher || !ctrl->processTable) return -1;
    switch (command) {
        case 1:
            algorithmSwitcherSetNext(ctrl->algorithmSwitcher, SchedulerAlgorithmFcfs);
            algorithmSwitcherApply(ctrl->algorithmSwitcher, ctrl->scheduler);
            ctrl->processTable->algorithmChangeCount++;
            if (ctrl->mainLogger) loggerLogFormat(ctrl->mainLogger, LogLevelInfo, "Comando 1: cambio manual a FCFS");
            return 0;
        case 2:
            guiControllerShowQuantumPrompt();
            int quantum = guiControllerAskQuantum();
            if (quantum > 0) ctrl->rrScheduler->currentQuantum = quantum;
            algorithmSwitcherSetNext(ctrl->algorithmSwitcher, SchedulerAlgorithmRr);
            algorithmSwitcherApply(ctrl->algorithmSwitcher, ctrl->scheduler);
            ctrl->processTable->algorithmChangeCount++;
            if (ctrl->mainLogger) loggerLogFormat(ctrl->mainLogger, LogLevelInfo, "Comando 2: cambio manual a RR con quantum %d", quantum);
            return 0;
        case 3: {
            char processId[idProcesoLen];
            memset(processId, 0, sizeof(processId));
            if (guiControllerGetPrivilegedProcessId(processId, idProcesoLen) > 0 && processId[0] != '\0') {
                Process* selected = findProcessById(ctrl, processId);
                if (selected && selected->bcp && selected->bcp->state != ProcessStateFinished) {
                    rrSchedulerPrioritizeProcess(ctrl->rrScheduler, processId);
                    if (ctrl->mainLogger) loggerLogFormat(ctrl->mainLogger, LogLevelInfo, "Comando 3: proceso %s privilegiado", processId);
                } else {
                    consoleIoPrintLine("ID inválido o proceso finalizado.");
                }
            }
            return 0;
        }
        default: return -1;
    }
}
