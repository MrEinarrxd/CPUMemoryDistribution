#include "pvmController.h"
#include "../distributed/localRunner.h"
#include "../distributed/pvmMaster.h"
#include "../utils/constants.h"
#include "../utils/randomUtils.h"

#include <pvm3.h>
#include <stdio.h>
#include <string.h>

static void pvmControllerPrepareTestTable(ProcessTable* table) {
    if (!table) return;
    randomInit(7);
    processTableInit(table);
    table->currentTime = 5000;
    table->cpuIterations = 120;

    for (int i = 0; i < TotalProcesses; ++i) {
        Bcp* bcp = &table->processes[i];
        bcp->timeInExecution = i * 3;
        bcp->timesInIo = i % 6;
        bcp->timesReturnedToReady = i % 11;
        bcp->cpuWasteCycles = (i * 7) % 200;
        bcp->quantumAssigned = DefaultQuantum;
        bcp->quantumUsed = DefaultQuantum - (i % 5);
        bcp->rrExecutionCount = i % 9 + 1;
        bcp->rrQuantumAssignedTotal = bcp->quantumAssigned * bcp->rrExecutionCount;
        bcp->rrQuantumUsedTotal = bcp->rrQuantumAssignedTotal - bcp->cpuWasteCycles;
        if (bcp->rrQuantumUsedTotal < 0) bcp->rrQuantumUsedTotal = 0;
        bcp->cpuWasteRatio = bcp->rrQuantumAssignedTotal > 0
            ? (float)bcp->cpuWasteCycles / (float)bcp->rrQuantumAssignedTotal
            : 0.0f;
        if (bcp->cpuWasteRatio > 1.0f) bcp->cpuWasteRatio = 1.0f;

        if (i < 40) {
            bcp->state = processStateFinished;
            bcp->remainingCpuCycles = 0;
            bcp->finishTime = table->currentTime + i;
        } else if (i < 90) {
            bcp->state = processStateWaitingIo;
        } else {
            bcp->state = processStateReady;
        }
    }

    table->finishedCount = 40;
    table->totalIoOperations = 90;
    processTableUpdateAverages(table);
    processTableUpdateQueueMetrics(table);
}

void pvmControllerInit(PvmController* controller, PvmMode mode) {
    if (!controller) return;
    memset(controller, 0, sizeof(*controller));
    controller->mode = mode;
    if (mode == pvmModeReal) {
        snprintf(controller->statusText, sizeof(controller->statusText), "[PVM REAL] sin iniciar");
    } else if (mode == pvmModeDisabled) {
        snprintf(controller->statusText, sizeof(controller->statusText), "[PVM DESACTIVADO]");
    } else {
        snprintf(controller->statusText, sizeof(controller->statusText), "[PVM LOCAL] listo");
    }
}

int pvmControllerStart(PvmController* controller) {
    int tid;
    if (!controller) return -1;
    if (controller->mode == pvmModeDisabled) {
        controller->started = 0;
        snprintf(controller->statusText, sizeof(controller->statusText), "[PVM DESACTIVADO]");
        return 0;
    }
    if (controller->mode == pvmModeLocal) {
        controller->started = 1;
        snprintf(controller->statusText, sizeof(controller->statusText), "[PVM LOCAL] activo");
        return 0;
    }

    tid = pvm_mytid();
    if (tid < 0) {
        snprintf(controller->statusText, sizeof(controller->statusText), "[PVM ERROR] pvmd no disponible");
        return -1;
    }
    pvm_exit();
    controller->started = 1;
    snprintf(controller->statusText, sizeof(controller->statusText), "[PVM REAL] activo");
    return 0;
}

int pvmControllerRunPeriodic(PvmController* controller, const ProcessTable* table, int currentIteration) {
    if (!controller || controller->mode == pvmModeDisabled) return 0;
    if (!controller || !table || !controller->started) return -1;
    if (currentIteration <= 0 || currentIteration == controller->lastAnalysisIteration ||
        currentIteration % PvmAnalysisInterval != 0) {
        return 0;
    }
    controller->lastAnalysisIteration = currentIteration;
    return pvmControllerRunFinal(controller, table);
}

int pvmControllerRunFinal(PvmController* controller, const ProcessTable* table) {
    int result;
    if (!controller || !table) return -1;
    if (controller->mode == pvmModeDisabled) {
        snprintf(controller->statusText, sizeof(controller->statusText), "[PVM DESACTIVADO]");
        return 0;
    }
    if (!controller->started) return -1;

    if (controller->mode == pvmModeLocal) {
        localRunnerRun(table, &controller->lastReport);
        controller->analysisCount++;
        snprintf(controller->statusText, sizeof(controller->statusText),
                 "[PVM LOCAL] analisis %d", controller->analysisCount);
        return 0;
    }

    result = pvmMasterRunReal(table, &controller->lastReport);
    if (result == 0) {
        controller->analysisCount++;
        snprintf(controller->statusText, sizeof(controller->statusText),
                 "[PVM REAL] analisis %d", controller->analysisCount);
    } else {
        snprintf(controller->statusText, sizeof(controller->statusText),
                 "[PVM ERROR] fallo en analisis PVM real");
    }
    return result;
}

int pvmControllerRunTest(void) {
    ProcessTable table;
    DistributedReport report;
    pvmControllerPrepareTestTable(&table);
    if (pvmMasterRunReal(&table, &report) != 0) {
        printf("Prueba PVM fallida. Verifique pvmd y %s.\n", DefaultPvmSlaveExec);
        return 1;
    }
    pvmControllerPrintReport("Prueba PVM real", &report);
    return 0;
}

void pvmControllerPrintReport(const char* title, const DistributedReport* report) {
    if (!report) return;
    printf("\n=== %s ===\n", title ? title : "Resultados PVM");
    printf("Procesos analizados: %d\n", report->stats.processCount);
    printf("Finalizados: %d | En E/S: %d | Operaciones E/S: %d\n",
           report->stats.finishedCount,
           report->stats.waitingCount,
           report->stats.ioOperations);
    printf("Promedio ciclos pendientes: %d\n", report->stats.avgRemainingCycles);
    printf("Aprovechamiento CPU promedio: %.3f\n", report->stats.avgCpuUtilization);
    printf("Top 5 envejecidos RR:\n");
    if (report->aging.topAgedCount == 0) printf("  Sin retornos RR registrados.\n");
    for (int i = 0; i < report->aging.topAgedCount; ++i) {
        printf("  %d. %s retornos=%d pendientes=%d\n",
               i + 1,
               report->aging.topAgedIds[i],
               report->aging.topAgedReturns[i],
               report->aging.topAgedRemaining[i]);
    }
    printf("Top 5 desperdicio CPU RR:\n");
    if (report->aging.topWastersCount == 0) printf("  Sin desperdicio RR registrado.\n");
    for (int i = 0; i < report->aging.topWastersCount; ++i) {
        printf("  %d. %s desperdicio=%d\n",
               i + 1,
               report->aging.topWastersIds[i],
               report->aging.topWastersWaste[i]);
    }
}

void pvmControllerDestroy(PvmController* controller) {
    if (!controller) return;
    controller->started = 0;
}
