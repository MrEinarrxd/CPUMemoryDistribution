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

#include "../domain/distributed/pvmMaster.h"

#include "../infrastructure/logger.h"
#include "../infrastructure/bcpLog.h"
#include "../infrastructure/processLog.h"

#include "../presentation/consoleIo.h"
#include "../presentation/menu.h"

// =============================================================================
// systemControllerCreate
// =============================================================================

// -----------------------------------------------------------------------------
// Estructura privada del controlador
// -----------------------------------------------------------------------------
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
    int                    running;          // 1 = en marcha, 0 = detenido
    Process*               currentProcess;   // proceso en CPU o NULL
};

// =============================================================================
// systemControllerCreate
// =============================================================================
SystemController* systemControllerCreate(void) {
    SystemController* ctrl = (SystemController*)malloc(sizeof(SystemController));
    if (!ctrl) {
        fprintf(stderr, "[SystemController] Error: no se pudo asignar memoria.\n");
        return NULL;
    }
    memset(ctrl, 0, sizeof(SystemController));
    ctrl->running = 0;
    ctrl->currentProcess = NULL;
    return ctrl;
}

// =============================================================================
// systemControllerInit – crea e interconecta todos los subsistemas
// =============================================================================
int systemControllerInit(SystemController* ctrl) {
    if (!ctrl) return -1;

    // 1. Utilidades globales
    initRandom(0);
    wordLoaderInit("libro1.odt");
    phraseLoaderInit("frases.odt");

    // 2. Logs
    ctrl->mainLogger = loggerCreate("simulation.log", LogLevelInfo);
    if (!ctrl->mainLogger)
        fprintf(stderr, "[SystemController] Advertencia: no se pudo abrir simulation.log\n");

    ctrl->bcpLogger = loggerCreate("bcp.log", LogLevelDebug);
    if (!ctrl->bcpLogger)
        fprintf(stderr, "[SystemController] Advertencia: no se pudo abrir bcp.log\n");

    ctrl->bcpLog = bcpLogCreate(ctrl->bcpLogger);
    ctrl->processLog = processLogCreate(ctrl->mainLogger);

    // 3. Cola de listos
    ctrl->readyQueue = readyQueueCreate();
    if (!ctrl->readyQueue) goto rollback_logs;

    // 4. Colas de E/S (4 dispositivos)
    const int mult[numColasEs] = { multColaEs1, multColaEs2, multColaEs3, multColaEs4 };
    ctrl->ioQueue = ioQueueCreate(mult);
    if (!ctrl->ioQueue) goto rollback_readyqueue;

    // 5. Tabla de procesos
    ctrl->processTable = processTableCreate();
    if (!ctrl->processTable) goto rollback_ioqueue;
    ctrl->processTable->readyQueue = ctrl->readyQueue;
    ctrl->processTable->ioQueue = ctrl->ioQueue;

    // 6. Inicializar generador de procesos (sin crear procesos aún)
    processGeneratorInit();
    ctrl->processTable->totalProcesses = 0;

    // 7. Planificadores
    ctrl->fcfsScheduler = fcfsSchedulerCreate();
    if (!ctrl->fcfsScheduler) goto rollback_proctable;

    ctrl->rrScheduler = rrSchedulerCreate(quantumDefault);
    if (!ctrl->rrScheduler) goto rollback_fcfs;

    ctrl->scheduler = schedulerCreate(SchedulerAlgorithmFcfs);
    if (!ctrl->scheduler) goto rollback_rr;
    schedulerSetFcfs(ctrl->scheduler, ctrl->fcfsScheduler);
    schedulerSetRr(ctrl->scheduler, ctrl->rrScheduler);

    // 8. Módulos auxiliares de planificación
    ctrl->rebalancer = rebalancerCreate();
    if (!ctrl->rebalancer) goto rollback_scheduler;

    ctrl->preemptionController = preemptionControllerCreate(0);
    if (!ctrl->preemptionController) goto rollback_rebalancer;

    ctrl->algorithmSwitcher = algorithmSwitcherCreate(quantumDefault);
    if (!ctrl->algorithmSwitcher) goto rollback_preemption;

    // 9. Subsistema de E/S
    ctrl->ioDispatcher = ioDispatcherCreate(ctrl->ioQueue);
    if (!ctrl->ioDispatcher) goto rollback_algoswitch;

    ctrl->ioCompletionHandler = ioCompletionHandlerCreate();
    if (!ctrl->ioCompletionHandler) goto rollback_iodispatch;

    // 10. Subsistema de memoria
    ctrl->bitmapManager = bitmapManagerCreate(1);
    if (!ctrl->bitmapManager) goto rollback_iocomp;

    ctrl->swapManager = swapManagerCreate(maxSwap);
    if (!ctrl->swapManager) goto rollback_bitmap;

    ctrl->pagingManager = pagingManagerCreate(maxMarcos, ctrl->bitmapManager);
    if (!ctrl->pagingManager) goto rollback_swap;

    ctrl->memoryResizer = memoryResizerCreate();
    if (!ctrl->memoryResizer) goto rollback_paging;

    // 11. Estadísticas
    ctrl->statsCollector = statsCollectorCreate();
    if (!ctrl->statsCollector) goto rollback_resizer;

    ctrl->performanceBar = performanceBarCreate();
    if (!ctrl->performanceBar) goto rollback_stats;

    ctrl->agingAnalysis = agingAnalysisCreate();
    if (!ctrl->agingAnalysis) goto rollback_perfbar;

    // 12. Todo listo
    ctrl->running = 1;
    if (ctrl->mainLogger)
        loggerLog(ctrl->mainLogger, LogLevelInfo, "SystemController inicializado correctamente.");
    return 0;

    // Etiquetas de deshacer (rollback)
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

// =============================================================================
// systemControllerCycle – ejecuta un ciclo de simulación (modelo incremental)
// =============================================================================
int systemControllerCycle(SystemController* ctrl) {
    if (!ctrl || !ctrl->running) return -1;

    // 1. Avanzar el reloj
    ctrl->processTable->currentCycle++;
    int ciclo = ctrl->processTable->currentCycle;

    // DEBUG: mostrar estado cada 50 ciclos
    if (ciclo % 50 == 0) {
        fprintf(stderr, "[DEBUG] Ciclo %d | Cola Listos: %d | Procesos Total: %d | Finalizados: %d | CPU cycles: %d\n",
               ciclo,
               readyQueueGetCount(ctrl->readyQueue),
               ctrl->processTable->totalProcesses,
               ctrl->processTable->finishedProcesses,
               ctrl->processTable->totalCpuCyclesExecuted);
        fflush(stderr);
    }

    // -------------------------------------------------------------------------
    // 2. Admitir nuevos procesos mediante processGeneratorGetNext() (generador incremental)
    // -------------------------------------------------------------------------
    while (processGeneratorHasMore()) {
        Bcp* bcp = processGeneratorGetNext();
        if (!bcp) break;

        // Si el proceso no toca aún, no lo procesamos (asume arrivalTimes ordenados)
        if (bcp->arrivalTime > ciclo) break;

        Process* proc = processCreate(bcp->processId, bcp->pid);
        if (!proc) {
            fprintf(stderr, "[SystemController] Fallo processCreate para %s\n", bcp->processId);
            continue;
        }
        proc->bcp = bcp;
        processActivate(proc);
        ctrl->processTable->totalProcesses++;

        int slot = -1;
        for (int i = 0; i < procesosEnEjecucion; i++) {
            if (ctrl->processTable->runningProcesses[i] == NULL) {
                slot = i;
                break;
            }
        }

        if (slot != -1) {
            ctrl->processTable->runningProcesses[slot] = proc;
            bcpSetState(bcp, ProcessStateReady);
            if (readyQueueEnqueue(ctrl->readyQueue, proc) != 0) {
                if (ctrl->mainLogger)
                    loggerLogFormat(ctrl->mainLogger, LogLevelWarning,
                        "Cola de listos llena al admitir %s en ciclo %d",
                        bcp->processId, ciclo);
            }
            pagingManagerAllocatePageForProcess(ctrl->pagingManager, slot, bcp->pageCount);
            if (ctrl->processLog)
                processLogRecordCreation(ctrl->processLog, proc);
        } else {
            for (int i = 0; i < procesosEnEspera; i++) {
                if (ctrl->processTable->newRequests[i] == NULL) {
                    ctrl->processTable->newRequests[i] = proc;
                    bcpSetState(bcp, ProcessStateNew);
                    break;
                }
            }
        }
    }

    // -------------------------------------------------------------------------
    // 3. Admitir procesos cuyo tiempo de llegada se ha cumplido (procesos ya creados)
    // -------------------------------------------------------------------------
    // 3a. Procesos que ya están en runningProcesses pero aún en estado New
    for (int i = 0; i < procesosEnEjecucion; i++) {
        Process* p = ctrl->processTable->runningProcesses[i];
        if (!p || !p->bcp) continue;
        if (p->bcp->state != ProcessStateNew) continue;
        if (p->bcp->arrivalTime > ciclo) continue;

        bcpSetState(p->bcp, ProcessStateReady);
        if (readyQueueEnqueue(ctrl->readyQueue, p) != 0) {
            if (ctrl->mainLogger)
                loggerLogFormat(ctrl->mainLogger, LogLevelWarning,
                    "Cola de listos llena al admitir %s en ciclo %d",
                    p->bcp->processId, ciclo);
        }
        pagingManagerAllocatePageForProcess(ctrl->pagingManager, i, p->bcp->pageCount);
        if (ctrl->processLog)
            processLogRecordCreation(ctrl->processLog, p);
    }

    // 3b. Mover procesos desde newRequests a runningProcesses cuando corresponda
    for (int i = 0; i < procesosEnEspera; i++) {
        Process* espera = ctrl->processTable->newRequests[i];
        if (!espera || !espera->bcp) continue;
        if (espera->bcp->arrivalTime > ciclo) continue;

        int slotLibre = -1;
        for (int j = 0; j < procesosEnEjecucion; j++) {
            if (ctrl->processTable->runningProcesses[j] == NULL) {
                slotLibre = j;
                break;
            }
        }
        if (slotLibre == -1) continue;

        ctrl->processTable->runningProcesses[slotLibre] = espera;
        ctrl->processTable->newRequests[i] = NULL;

        bcpSetState(espera->bcp, ProcessStateReady);
        if (readyQueueEnqueue(ctrl->readyQueue, espera) != 0) {
            if (ctrl->mainLogger)
                loggerLogFormat(ctrl->mainLogger, LogLevelWarning,
                    "Cola de listos llena al admitir %s desde espera", espera->bcp->processId);
        }
        pagingManagerAllocatePageForProcess(ctrl->pagingManager, slotLibre, espera->bcp->pageCount);
        if (ctrl->processLog)
            processLogRecordCreation(ctrl->processLog, espera);
    }

    // -------------------------------------------------------------------------
    // 4. Finalización de E/S
    // -------------------------------------------------------------------------
    ioCompletionHandlerProcess(ctrl->ioCompletionHandler, ctrl->ioQueue, ctrl->readyQueue,
                               ctrl->pagingManager, ctrl->processTable);

    // -------------------------------------------------------------------------
    // 5. Seleccionar el próximo proceso a ejecutar
    // -------------------------------------------------------------------------
    Process* actual = schedulerSelectNext(ctrl->scheduler, ctrl->processTable);

    // Incrementar tiempo de espera para todos los procesos en estado Ready
    for (int i = 0; i < procesosEnEjecucion; i++) {
        Process* p = ctrl->processTable->runningProcesses[i];
        if (p && p->bcp && p->bcp->state == ProcessStateReady) {
            p->bcp->timeInWaiting++;
        }
    }
    for (int i = 0; i < procesosEnEspera; i++) {
        Process* p = ctrl->processTable->newRequests[i];
        if (p && p->bcp && p->bcp->state == ProcessStateReady) {
            p->bcp->timeInWaiting++;
        }
    }

    if (actual && actual->bcp) {
        Bcp* bcp = actual->bcp;
        bcpSetState(bcp, ProcessStateRunning);
        ctrl->currentProcess = actual;

        // 5a. Ejecutar una ráfaga de CPU
        int instancia = randomCpuInstanceCycles();
        if (instancia > bcp->remainingCycles)
            instancia = bcp->remainingCycles;

        // DEBUG: mostrar proceso ejecutándose
        fprintf(stderr, "[DEBUG] Ejecutando %s: instancia=%d, remainingCycles=%d → %d\n",
               bcp->processId, instancia, bcp->remainingCycles, bcp->remainingCycles - instancia);
        fflush(stderr);

        bcpUpdateRemainingTime(bcp, instancia);
        bcp->timeInExecution += instancia;
        bcp->quantumUsed += instancia;
        bcp->timesExecuted++;
        ctrl->processTable->totalCpuCyclesExecuted += instancia;

        // Crecimiento dinámico de memoria: con probabilidad ~25% el proceso solicita más páginas
        {
            int growth = randomMemoryGrowth();
            if (growth > 0) {
                int procIdx = -1;
                for (int gi = 0; gi < procesosEnEjecucion; gi++) {
                    if (ctrl->processTable->runningProcesses[gi] == actual) { procIdx = gi; break; }
                }
                if (procIdx >= 0) {
                    int newPages = ctrl->pagingManager->frameCountPerProcess[procIdx] + growth;
                    if (newPages > maxPaginasPorProceso) newPages = maxPaginasPorProceso;
                    pagingManagerResizeFrames(ctrl->pagingManager, procIdx, newPages);
                    bcp->pageCount = newPages;
                }
            }
        }
        if (schedulerGetAlgorithm(ctrl->scheduler) == SchedulerAlgorithmRr) {
            int quantum = rrSchedulerGetCurrentQuantum(ctrl->rrScheduler);
            if (bcp->quantumUsed > quantum) {
                // No debería pasar, pero por seguridad
                bcp->wastedCpuCycles += 0;
            } else {
                int waste = quantum - bcp->quantumUsed;
                if (waste > 0) {
                    bcp->wastedCpuCycles += waste;
                }
            }
        }

        // 5b. Coste de cambio de contexto
        bcp->contextSwitchTime = randomContextSwitchTime();
        bcpIncrementContextSwitches(bcp);
        ctrl->processTable->totalContextSwitches++;
        schedulerOnContextSwitch(ctrl->scheduler);
        if (ctrl->bcpLog) bcpLogRecordContextSwitch(ctrl->bcpLog, bcp);

        // 5c. Decidir si el proceso solicita E/S (probabilidad 30%)
        if (bcp->remainingCycles > 0 && randomInt(0, 100) < 30) {
            bcp->ioOperationsPending = 1;
            ioDispatcherLoadRandomPhrase(actual);
        }

        // 5d. Verificar expiración del quantum (solo en RR)
        if (schedulerGetAlgorithm(ctrl->scheduler) == SchedulerAlgorithmRr) {
            int quantum = rrSchedulerGetCurrentQuantum(ctrl->rrScheduler);
            if (bcp->quantumUsed >= quantum) {
                bcp->quantumUsed = 0;
                rrSchedulerOnQuantumExpired(ctrl->rrScheduler);
                if (ctrl->bcpLog) bcpLogRecordQuantumExpired(ctrl->bcpLog, bcp);

                if (bcp->remainingCycles > 0 && bcp->ioOperationsPending == 0) {
                    bcpSetState(bcp, ProcessStateReady);
                    readyQueueEnqueue(ctrl->readyQueue, actual);
                }
                ctrl->currentProcess = NULL;
            }
        }

        // 5e. Proceso terminado
        if (bcp->remainingCycles <= 0) {
            bcp->finishTime = ciclo;
            bcpSetState(bcp, ProcessStateFinished);
            processDeactivate(actual);
            processTableIncrementFinished(ctrl->processTable);
            if (ctrl->processLog)
                processLogRecordTermination(ctrl->processLog, actual);

            int procIdx = -1;
            for (int j = 0; j < procesosEnEjecucion; j++) {
                if (ctrl->processTable->runningProcesses[j] == actual) {
                    procIdx = j;
                    ctrl->processTable->runningProcesses[j] = NULL;
                    break;
                }
            }
            if (procIdx >= 0) {
                pagingManagerDeallocatePagesForProcess(ctrl->pagingManager, procIdx);
            } else if (ctrl->mainLogger) {
                loggerLogFormat(ctrl->mainLogger, LogLevelWarning,
                    "No se encontró índice de proceso para liberar memoria de %s",
                    bcp->processId);
            }
            ctrl->currentProcess = NULL;

        // 5f. Enviar a E/S si hay operaciones pendientes
        } else if (bcp->ioOperationsPending > 0) {
            bcpSetState(bcp, ProcessStateWaitingIo);
            int dispositivo = bcp->timesInIo % numColasEs;
            bcp->timesInIo++;
            ctrl->processTable->totalIoOperations++;
            ioDispatcherDispatch(ctrl->ioDispatcher, actual, dispositivo);
            ctrl->currentProcess = NULL;
        }
    }

    // -------------------------------------------------------------------------
    // 6. Avanzar los relojes de E/S
    // -------------------------------------------------------------------------
    ioDispatcherTick(ctrl->ioDispatcher);

    // -------------------------------------------------------------------------
    // 7. Rebalanceo dinámico de quantum
    // -------------------------------------------------------------------------
    rebalancerTick(ctrl->rebalancer);
    if (rebalancerShouldCheck(ctrl->rebalancer)) {
        rrSchedulerUpdateProportions(ctrl->rrScheduler, ctrl->processTable);
        float propListos = ctrl->rrScheduler->proportionReady;
        if (rebalancerIsImbalanced(ctrl->rebalancer, propListos)) {
            int delta = rebalancerSuggestQuantumDelta(ctrl->rebalancer, propListos);
            int nuevoQuantum = ctrl->rrScheduler->currentQuantum + delta;
            if (nuevoQuantum < 1) nuevoQuantum = 1;
            ctrl->rrScheduler->currentQuantum = nuevoQuantum;
            rebalancerOnBalanceRestored(ctrl->rebalancer);
            menuShowBalanceAlert(ctrl->rrScheduler->proportionReady,
                                 ctrl->rrScheduler->proportionWaiting,
                                 nuevoQuantum);
        }
    }

    // -------------------------------------------------------------------------
    // 8. Cambio automático de algoritmo
    // -------------------------------------------------------------------------
    if (algorithmSwitcherShouldSwitch(ctrl->algorithmSwitcher, ctrl->scheduler, ctrl->processTable)) {
        algorithmSwitcherApply(ctrl->algorithmSwitcher, ctrl->scheduler);
        ctrl->processTable->algorithmChangeCount++;
    }

    // -------------------------------------------------------------------------
    // 9. Actualizar ranking de envejecimiento (solo RR)
    // -------------------------------------------------------------------------
    if (schedulerGetAlgorithm(ctrl->scheduler) == SchedulerAlgorithmRr)
        rrSchedulerUpdateAgingRanking(ctrl->rrScheduler, ctrl->processTable);

    // -------------------------------------------------------------------------
    // 10. Redimensionar memoria periódicamente
    // -------------------------------------------------------------------------
    if (ciclo % growthListSize == 0)
        memoryResizerExecute(ctrl->memoryResizer, ctrl->processTable, ctrl->pagingManager);

    // -------------------------------------------------------------------------
    // 11. Recolectar estadísticas
    // -------------------------------------------------------------------------
    statsCollectorCollect(ctrl->statsCollector, ctrl->processTable);
    performanceBarUpdate(ctrl->performanceBar, ctrl->statsCollector);
    agingAnalysisCollectSample(ctrl->agingAnalysis, ctrl->processTable);
    processTableUpdateAverages(ctrl->processTable);

    return 0;
}

// =============================================================================
// systemControllerDestroy – libera todos los recursos
// =============================================================================
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

// =============================================================================
// Manejadores internos de teclas (estáticos)
// =============================================================================
static void handleKeyX(SystemController* ctrl) {
    consoleIoClear();
    menuShowAlgorithmOptions();
    int opcion = menuGetAlgorithmChoice();   // 1=FCFS, 2=RR, 0=cancelar
    if (opcion == 0) return;

    if (opcion == 1 || opcion == 2) {
        systemControllerHandleCommand(ctrl, opcion);
    }
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
            loggerLogFormat(ctrl->mainLogger, LogLevelInfo,
                "Tecla A: proceso %s marcado como privilegiado.", idElegido);
    }
}

// =============================================================================
// systemControllerRun – bucle principal de simulación
// =============================================================================
int systemControllerRun(SystemController* ctrl) {
    if (!ctrl) return -1;

    consoleIoInit();
    menuShowMain();

    if (ctrl->mainLogger)
        loggerLog(ctrl->mainLogger, LogLevelInfo, "Bucle de simulación iniciado.");

    while (ctrl->running) {
        int ret = systemControllerCycle(ctrl);
        if (ret != 0) {
            if (ctrl->mainLogger)
                loggerLogFormat(ctrl->mainLogger, LogLevelError,
                    "systemControllerCycle devolvió %d en ciclo %d. Abortando.",
                    ret, ctrl->processTable->currentCycle);
            ctrl->running = 0;
            break;
        }

        if (ctrl->processTable->currentCycle % 10 == 0) {
            consoleIoClear();
            menuShowPerformanceBars(ctrl->performanceBar);
            menuShowMemoryStats(ctrl->statsCollector);
        }

        if (ctrl->processTable->totalProcesses > 0 &&
            ctrl->processTable->finishedProcesses >= ctrl->processTable->totalProcesses) {
            if (ctrl->mainLogger)
                loggerLog(ctrl->mainLogger, LogLevelInfo, "Todos los procesos terminaron. Simulación finalizada.");
            ctrl->running = 0;
            break;
        }

        if (!consoleIoKbhit()) {
            timeHelperDelay(20);
            continue;
        }

        char tecla = consoleIoGetChar();
        switch (tecla) {
            case 'X': case 'x':
                handleKeyX(ctrl);
                break;
            case 'A': case 'a':
                if (schedulerGetAlgorithm(ctrl->scheduler) == SchedulerAlgorithmRr)
                    handleKeyA(ctrl);
                else
                    consoleIoPrintLine("El ranking de envejecimiento solo está disponible en modo RR.");
                break;
            case 'P': case 'p': {
                consoleIoPrintLine("[PAUSADO] Presione P nuevamente para reanudar...");
                char reanudar = 0;
                while (reanudar != 'P' && reanudar != 'p') {
                    if (consoleIoKbhit()) reanudar = consoleIoGetChar();
                    else                  timeHelperDelay(50);
                }
                consoleIoPrintLine("[REANUDADO]");
                break;
            }
            case 'Q': case 'q': case 27:
                if (ctrl->mainLogger)
                    loggerLog(ctrl->mainLogger, LogLevelInfo, "Usuario solicitó detener la simulación.");
                ctrl->running = 0;
                break;
            default:
                systemControllerHandleCommand(ctrl, (int)tecla);
                break;
        }
    }

    // Resumen final
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

// =============================================================================
// Ejecución de tareas distribuidas PVM
// =============================================================================
int systemControllerRunPvm(SystemController* ctrl) {
    if (!ctrl) {
        errorHandlerLog(ErrorCodeInvalidArgument, "systemControllerRunPvm: ctrl es NULL");
        return -1;
    }

    consoleIoPrintLine("Inicializando maestro PVM...");
    PvmMaster* master = pvmMasterInit();
    if (!master) {
        errorHandlerLog(ErrorCodeNodeConnectionFailed,
                       "systemControllerRunPvm: no se pudo inicializar pvmMaster");
        return -1;
    }

    // Asignar referencias a processTable y rrScheduler
    master->processTable = ctrl->processTable;
    master->rrScheduler = ctrl->rrScheduler;

    // Lanzar esclavos
    consoleIoPrintLine("Lanzando procesos esclavos...");
    int numEsclavos = pvmMasterSpawnSlaves(master);
    if (numEsclavos != pvmNumEsclavos) {
        errorHandlerLog(ErrorCodeNodeConnectionFailed,
                       "systemControllerRunPvm: no se pudieron lanzar todos los esclavos");
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

    // Limpieza
    pvmMasterCleanup(master);
    consoleIoPrintLine("Tareas distribuidas completadas.");
    return 0;
}

int systemControllerHandleCommand(SystemController* ctrl, int command) {
    if (!ctrl || !ctrl->scheduler || !ctrl->rrScheduler || !ctrl->algorithmSwitcher || !ctrl->processTable)
        return -1;

    switch (command) {
        case 1: {
            algorithmSwitcherSetNext(ctrl->algorithmSwitcher, SchedulerAlgorithmFcfs);
            algorithmSwitcherApply(ctrl->algorithmSwitcher, ctrl->scheduler);
            ctrl->processTable->algorithmChangeCount++;
            if (ctrl->mainLogger)
                loggerLogFormat(ctrl->mainLogger, LogLevelInfo,
                                "Comando 1: cambio manual a FCFS");
            return 0;
        }
        case 2: {
            menuShowQuantumPrompt();
            int quantum = menuGetQuantumInput();
            if (quantum > 0) {
                ctrl->rrScheduler->currentQuantum = quantum;
            }
            algorithmSwitcherSetNext(ctrl->algorithmSwitcher, SchedulerAlgorithmRr);
            algorithmSwitcherApply(ctrl->algorithmSwitcher, ctrl->scheduler);
            ctrl->processTable->algorithmChangeCount++;
            if (ctrl->mainLogger)
                loggerLogFormat(ctrl->mainLogger, LogLevelInfo,
                                "Comando 2: cambio manual a RR con quantum %d", quantum);
            return 0;
        }
        case 3: {
            char processId[idProcesoLen];
            memset(processId, 0, sizeof(processId));
            if (menuGetPrivilegedProcessId(processId, idProcesoLen) > 0 && processId[0] != '\0') {
                rrSchedulerPrioritizeProcess(ctrl->rrScheduler, processId);
                if (ctrl->mainLogger)
                    loggerLogFormat(ctrl->mainLogger, LogLevelInfo,
                                    "Comando 3: proceso %s privilegiado", processId);
            }
            return 0;
        }
        default:
            return -1;
    }
}