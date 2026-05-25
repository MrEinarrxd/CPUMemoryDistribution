// === src/business/systemController.c ===
// Orquestador principal del simulador: inicializa, coordina y destruye todos los
// subsistemas (procesos, planificación, memoria, E/S, estadísticas, logs).
// El flujo de ejecución se divide en ciclos manejados por systemControllerCycle().

#include "systemController.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// -----------------------------------------------------------------------------
// Headers de todos los módulos utilizados
// -----------------------------------------------------------------------------
#include "../utils/constants.h"
#include "../utils/random.h"
#include "../utils/wordLoader.h"
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

#include "../infrastructure/logger.h"
#include "../infrastructure/bcpLog.h"
#include "../infrastructure/processLog.h"

#include "../presentation/consoleIo.h"
#include "../presentation/menu.h"

// -----------------------------------------------------------------------------
// Estructura privada de SystemController (definición oculta en el .c)
// -----------------------------------------------------------------------------
struct SystemController {
    // Procesos
    ProcessTable*          processTable;

    // Planificación
    ReadyQueue*            readyQueue;
    FcfsScheduler*         fcfsScheduler;
    RrScheduler*           rrScheduler;
    Scheduler*             scheduler;
    Rebalancer*            rebalancer;
    PreemptionController*  preemptionController;
    AlgorithmSwitcher*     algorithmSwitcher;

    // Entrada/Salida
    IoQueue*               ioQueue;
    IoDispatcher*          ioDispatcher;
    IoCompletionHandler*   ioCompletionHandler;

    // Memoria (Bitmap + FIFO)
    BitmapManager*         bitmapManager;
    PagingManager*         pagingManager;
    SwapManager*           swapManager;
    MemoryResizer*         memoryResizer;

    // Estadísticas y rendimiento
    StatsCollector*        statsCollector;
    PerformanceBar*        performanceBar;
    AgingAnalysis*         agingAnalysis;

    // Logs
    Logger*                mainLogger;      // archivo general simulation.log
    Logger*                bcpLogger;       // archivo específico bcp.log
    BcpLog*                bcpLog;
    ProcessLog*            processLog;

    // Control de la simulación
    int                    running;         // 1 = en ejecución, 0 = detenido
    int                    currentProcess;  // índice del proceso en CPU (-1 si libre)
};

// -----------------------------------------------------------------------------
// systemControllerCreate - Reserva memoria y establece valores por defecto
// -----------------------------------------------------------------------------
SystemController* systemControllerCreate(void) {
    SystemController* ctrl = (SystemController*)malloc(sizeof(SystemController));
    if (!ctrl) {
        fprintf(stderr, "[SystemController] Error: no se pudo asignar memoria.\n");
        return NULL;
    }
    memset(ctrl, 0, sizeof(SystemController));
    ctrl->running        = 0;
    ctrl->currentProcess = -1;
    return ctrl;
}

// -----------------------------------------------------------------------------
// systemControllerInit - Crea e interconecta todos los subsistemas
// -----------------------------------------------------------------------------
int systemControllerInit(SystemController* ctrl) {
    if (!ctrl) return -1;

    // ---------- 1. Utilidades globales ----------
    initRandom(0);                    // semilla automática
    wordLoaderInit("libro1.odt");

    // ---------- 2. Logs ----------
    ctrl->mainLogger = loggerCreate("simulation.log", LogLevelInfo);
    if (!ctrl->mainLogger)
        fprintf(stderr, "[SystemController] Advertencia: no se pudo abrir simulation.log\n");

    ctrl->bcpLogger = loggerCreate("bcp.log", LogLevelDebug);
    if (!ctrl->bcpLogger)
        fprintf(stderr, "[SystemController] Advertencia: no se pudo abrir bcp.log\n");

    ctrl->bcpLog    = bcpLogCreate(ctrl->bcpLogger);
    ctrl->processLog = processLogCreate(ctrl->mainLogger);

    // ---------- 3. Cola de listos (ReadyQueue) ----------
    ctrl->readyQueue = readyQueueCreate();
    if (!ctrl->readyQueue) {
        fprintf(stderr, "[SystemController] Error: readyQueueCreate falló.\n");
        goto rollback_logs;
    }

    // ---------- 4. Colas de E/S (4 dispositivos con multiplicadores) ----------
    const int multipliers[numColasEs] = {
        multColaEs1, multColaEs2, multColaEs3, multColaEs4
    };
    ctrl->ioQueue = ioQueueCreate(multipliers);
    if (!ctrl->ioQueue) {
        fprintf(stderr, "[SystemController] Error: ioQueueCreate falló.\n");
        goto rollback_readyqueue;
    }

    // ---------- 5. Tabla de procesos (referencia a readyQueue e ioQueue) ----------
    ctrl->processTable = processTableCreate();
    if (!ctrl->processTable) {
        fprintf(stderr, "[SystemController] Error: processTableCreate falló.\n");
        goto rollback_ioqueue;
    }
    ctrl->processTable->readyQueue = ctrl->readyQueue;
    ctrl->processTable->ioQueue    = ctrl->ioQueue;

    // ---------- 6. Generación de los 250 procesos (150 ejecución, 100 espera) ----------
    processGeneratorInit();
    processGeneratorRun(ctrl->processTable);

    // ---------- 7. Planificadores ----------
    ctrl->fcfsScheduler = fcfsSchedulerCreate();
    if (!ctrl->fcfsScheduler) {
        fprintf(stderr, "[SystemController] Error: fcfsSchedulerCreate falló.\n");
        goto rollback_proctable;
    }

    ctrl->rrScheduler = rrSchedulerCreate(quantumDefault);
    if (!ctrl->rrScheduler) {
        fprintf(stderr, "[SystemController] Error: rrSchedulerCreate falló.\n");
        goto rollback_fcfs;
    }

    // Scheduler maestro (comienza con FCFS)
    ctrl->scheduler = schedulerCreate(SchedulerAlgorithmFcfs);
    if (!ctrl->scheduler) {
        fprintf(stderr, "[SystemController] Error: schedulerCreate falló.\n");
        goto rollback_rr;
    }
    schedulerSetFcfs(ctrl->scheduler, ctrl->fcfsScheduler);
    schedulerSetRr(ctrl->scheduler,   ctrl->rrScheduler);

    // ---------- 8. Módulos auxiliares de planificación ----------
    ctrl->rebalancer = rebalancerCreate();
    if (!ctrl->rebalancer) {
        fprintf(stderr, "[SystemController] Error: rebalancerCreate falló.\n");
        goto rollback_scheduler;
    }

    ctrl->preemptionController = preemptionControllerCreate(0); // inicialmente sin apropiatividad
    if (!ctrl->preemptionController) {
        fprintf(stderr, "[SystemController] Error: preemptionControllerCreate falló.\n");
        goto rollback_rebalancer;
    }

    ctrl->algorithmSwitcher = algorithmSwitcherCreate(quantumDefault);
    if (!ctrl->algorithmSwitcher) {
        fprintf(stderr, "[SystemController] Error: algorithmSwitcherCreate falló.\n");
        goto rollback_preemption;
    }

    // ---------- 9. Dispatcher y manejador de finalización de E/S ----------
    ctrl->ioDispatcher = ioDispatcherCreate(ctrl->ioQueue);
    if (!ctrl->ioDispatcher) {
        fprintf(stderr, "[SystemController] Error: ioDispatcherCreate falló.\n");
        goto rollback_algoswitch;
    }

    ctrl->ioCompletionHandler = ioCompletionHandlerCreate();
    if (!ctrl->ioCompletionHandler) {
        fprintf(stderr, "[SystemController] Error: ioCompletionHandlerCreate falló.\n");
        goto rollback_iodispatch;
    }

    // ---------- 10. Subsistema de memoria (Bitmap + FIFO + Swap) ----------
    // 10a. Administrador de marcos físicos (Mapa de bits)
    ctrl->bitmapManager = bitmapManagerCreate(1); // cada bloque = 1 marco
    if (!ctrl->bitmapManager) {
        fprintf(stderr, "[SystemController] Error: bitmapManagerCreate falló.\n");
        goto rollback_iocomp;
    }

    // 10b. Área de intercambio (SWAP)
    ctrl->swapManager = swapManagerCreate(maxSwap);
    if (!ctrl->swapManager) {
        fprintf(stderr, "[SystemController] Error: swapManagerCreate falló.\n");
        goto rollback_bitmap;
    }

    // 10c. Gestor de paginación (conecta bitmap, fifo y directorio de páginas)
    ctrl->pagingManager = pagingManagerCreate(maxMarcos, ctrl->bitmapManager);
    if (!ctrl->pagingManager) {
        fprintf(stderr, "[SystemController] Error: pagingManagerCreate falló.\n");
        goto rollback_swap;
    }

    // 10d. Redimensionador dinámico de marcos (mitad reduce, mitad duplica)
    ctrl->memoryResizer = memoryResizerCreate();
    if (!ctrl->memoryResizer) {
        fprintf(stderr, "[SystemController] Error: memoryResizerCreate falló.\n");
        goto rollback_paging;
    }

    // ---------- 11. Estadísticas y barras de rendimiento ----------
    ctrl->statsCollector = statsCollectorCreate();
    if (!ctrl->statsCollector) {
        fprintf(stderr, "[SystemController] Error: statsCollectorCreate falló.\n");
        goto rollback_resizer;
    }

    ctrl->performanceBar = performanceBarCreate();
    if (!ctrl->performanceBar) {
        fprintf(stderr, "[SystemController] Error: performanceBarCreate falló.\n");
        goto rollback_stats;
    }

    ctrl->agingAnalysis = agingAnalysisCreate();
    if (!ctrl->agingAnalysis) {
        fprintf(stderr, "[SystemController] Error: agingAnalysisCreate falló.\n");
        goto rollback_perfbar;
    }

    // ---------- 12. Simulación lista para ejecutarse ----------
    ctrl->running = 1;
    if (ctrl->mainLogger) {
        loggerLog(ctrl->mainLogger, LogLevelInfo,
                  "SystemController inicializado correctamente.");
    }
    return 0;   // éxito

    // ----- Etiquetas de deshacer (rollback) en caso de error -----
rollback_perfbar:
    if (ctrl->performanceBar) performanceBarDestroy(ctrl->performanceBar);
rollback_stats:
    if (ctrl->statsCollector) statsCollectorDestroy(ctrl->statsCollector);
rollback_resizer:
    if (ctrl->memoryResizer) memoryResizerDestroy(ctrl->memoryResizer);
rollback_paging:
    if (ctrl->pagingManager) pagingManagerDestroy(ctrl->pagingManager);
rollback_swap:
    if (ctrl->swapManager) swapManagerDestroy(ctrl->swapManager);
rollback_bitmap:
    if (ctrl->bitmapManager) bitmapManagerDestroy(ctrl->bitmapManager);
rollback_iocomp:
    if (ctrl->ioCompletionHandler) ioCompletionHandlerDestroy(ctrl->ioCompletionHandler);
rollback_iodispatch:
    if (ctrl->ioDispatcher) ioDispatcherDestroy(ctrl->ioDispatcher);
rollback_algoswitch:
    if (ctrl->algorithmSwitcher) algorithmSwitcherDestroy(ctrl->algorithmSwitcher);
rollback_preemption:
    if (ctrl->preemptionController) preemptionControllerDestroy(ctrl->preemptionController);
rollback_rebalancer:
    if (ctrl->rebalancer) rebalancerDestroy(ctrl->rebalancer);
rollback_scheduler:
    if (ctrl->scheduler) schedulerDestroy(ctrl->scheduler);
rollback_rr:
    if (ctrl->rrScheduler) rrSchedulerDestroy(ctrl->rrScheduler);
rollback_fcfs:
    if (ctrl->fcfsScheduler) fcfsSchedulerDestroy(ctrl->fcfsScheduler);
rollback_proctable:
    if (ctrl->processTable) processTableDestroy(ctrl->processTable);
rollback_ioqueue:
    if (ctrl->ioQueue) ioQueueDestroy(ctrl->ioQueue);
rollback_readyqueue:
    if (ctrl->readyQueue) readyQueueDestroy(ctrl->readyQueue);
rollback_logs:
    if (ctrl->processLog) processLogDestroy(ctrl->processLog);
    if (ctrl->bcpLog) bcpLogDestroy(ctrl->bcpLog);
    if (ctrl->bcpLogger) loggerDestroy(ctrl->bcpLogger);
    if (ctrl->mainLogger) loggerDestroy(ctrl->mainLogger);
    wordLoaderCleanup();
    return -1;
}

// -----------------------------------------------------------------------------
// systemControllerDestroy - Libera todos los recursos (orden inverso a Init)
// -----------------------------------------------------------------------------
void systemControllerDestroy(SystemController* ctrl) {
    if (!ctrl) return;

    // Estadísticas
    if (ctrl->agingAnalysis)    agingAnalysisDestroy(ctrl->agingAnalysis);
    if (ctrl->performanceBar)   performanceBarDestroy(ctrl->performanceBar);
    if (ctrl->statsCollector)   statsCollectorDestroy(ctrl->statsCollector);

    // Memoria
    if (ctrl->memoryResizer)    memoryResizerDestroy(ctrl->memoryResizer);
    if (ctrl->pagingManager)    pagingManagerDestroy(ctrl->pagingManager);
    if (ctrl->swapManager)      swapManagerDestroy(ctrl->swapManager);
    if (ctrl->bitmapManager)    bitmapManagerDestroy(ctrl->bitmapManager);

    // E/S
    if (ctrl->ioCompletionHandler) ioCompletionHandlerDestroy(ctrl->ioCompletionHandler);
    if (ctrl->ioDispatcher)        ioDispatcherDestroy(ctrl->ioDispatcher);

    // Planificación
    if (ctrl->algorithmSwitcher)   algorithmSwitcherDestroy(ctrl->algorithmSwitcher);
    if (ctrl->preemptionController) preemptionControllerDestroy(ctrl->preemptionController);
    if (ctrl->rebalancer)          rebalancerDestroy(ctrl->rebalancer);
    if (ctrl->scheduler)           schedulerDestroy(ctrl->scheduler);
    if (ctrl->rrScheduler)         rrSchedulerDestroy(ctrl->rrScheduler);
    if (ctrl->fcfsScheduler)       fcfsSchedulerDestroy(ctrl->fcfsScheduler);

    // Tabla de procesos (destruye los procesos internos)
    processGeneratorCleanup();
    if (ctrl->processTable) processTableDestroy(ctrl->processTable);

    // Colas (después de la tabla para evitar referencias colgantes)
    if (ctrl->ioQueue)    ioQueueDestroy(ctrl->ioQueue);
    if (ctrl->readyQueue) readyQueueDestroy(ctrl->readyQueue);

    // Logs
    if (ctrl->processLog) processLogDestroy(ctrl->processLog);
    if (ctrl->bcpLog)     bcpLogDestroy(ctrl->bcpLog);
    if (ctrl->bcpLogger)  loggerDestroy(ctrl->bcpLogger);
    if (ctrl->mainLogger) loggerDestroy(ctrl->mainLogger);

    // Utilidades globales
    wordLoaderCleanup();

    free(ctrl);
}

// -----------------------------------------------------------------------------
// Stubs de los métodos que se completarán en etapas posteriores
// -----------------------------------------------------------------------------

int systemControllerRun(SystemController* ctrl) {
    (void)ctrl;   // evitar warning de parámetro no usado
    // Implementación futura: bucle principal de simulación
    return 0;
}

int systemControllerRunPvm(SystemController* ctrl) {
    (void)ctrl;
    // Implementación futura: ejecución de tareas distribuidas con PVM
    return 0;
}

int systemControllerHandleCommand(SystemController* ctrl, int command) {
    (void)ctrl;
    (void)command;
    // Implementación futura: comandos de usuario (cambiar algoritmo, etc.)
    return 0;
}

int systemControllerCycle(SystemController* ctrl) {
    (void)ctrl;
    // Implementación futura: avance de un ciclo de simulación
    return 0;
}