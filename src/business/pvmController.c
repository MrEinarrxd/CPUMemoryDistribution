#include "pvmController.h"
#include "../distributed/localRunner.h"
#include "../distributed/pvmMaster.h"
#include "../utils/constants.h"

#include <stdio.h>
#include <string.h>

static int pvmControllerEnsureRealSession(PvmController* controller) {
    if (!controller) return -1;
    if (controller->realSession.active) return 0;
    if (pvmMasterSessionStart(&controller->realSession) != 0) {
        snprintf(controller->statusText, sizeof(controller->statusText),
                 "[PVM ERROR] no se pudieron iniciar slaves persistentes");
        return -1;
    }
    snprintf(controller->statusText, sizeof(controller->statusText),
             "[PVM REAL] slaves persistentes activos");
    return 0;
}

static int pvmControllerRunRealAnalysis(PvmController* controller,
                                        const ProcessTable* table) {
    if (!controller || !table) return -1;
    if (pvmControllerEnsureRealSession(controller) != 0) return -1;
    if (pvmMasterSessionAnalyze(&controller->realSession, table,
                                &controller->lastReport) == 0) {
        controller->analysisCount++;
        snprintf(controller->statusText, sizeof(controller->statusText),
                 "[PVM REAL] analisis %d", controller->analysisCount);
        return 0;
    }

    pvmMasterSessionStop(&controller->realSession);
    snprintf(controller->statusText, sizeof(controller->statusText),
             "[PVM ERROR] fallo en analisis PVM real");
    return -1;
}

void pvmControllerInit(PvmController* controller, PvmMode mode) {
    if (!controller) return;
    memset(controller, 0, sizeof(*controller));
    controller->mode = mode;
    pvmMasterSessionInit(&controller->realSession);
    if (mode == pvmModeReal) {
        snprintf(controller->statusText, sizeof(controller->statusText), "[PVM REAL] sin iniciar");
    } else {
        snprintf(controller->statusText, sizeof(controller->statusText), "[PVM VIRTUAL] listo");
    }
}

int pvmControllerStart(PvmController* controller) {
    if (!controller) return -1;
    if (controller->mode == pvmModeLocal) {
        controller->started = 1;
        snprintf(controller->statusText, sizeof(controller->statusText), "[PVM VIRTUAL] activo");
        return 0;
    }

    if (pvmControllerEnsureRealSession(controller) != 0) return -1;
    controller->started = 1;
    return 0;
}

int pvmControllerRunPeriodic(PvmController* controller, const ProcessTable* table, int currentIteration) {
    if (!controller || !table || !controller->started) return -1;
    if (currentIteration <= 0 || currentIteration == controller->lastAnalysisIteration ||
        currentIteration % PvmAnalysisInterval != 0) {
        return 0;
    }
    controller->lastAnalysisIteration = currentIteration;

    if (controller->mode == pvmModeReal) {
        return pvmControllerRunRealAnalysis(controller, table);
    }

    return pvmControllerRunFinal(controller, table);
}

int pvmControllerRunFinal(PvmController* controller, const ProcessTable* table) {
    int result;
    if (!controller || !table) return -1;
    if (!controller->started) return -1;

    if (controller->mode == pvmModeLocal) {
        localRunnerRun(table, &controller->lastReport);
        controller->analysisCount++;
        snprintf(controller->statusText, sizeof(controller->statusText),
                 "[PVM VIRTUAL] analisis %d", controller->analysisCount);
        return 0;
    }

    result = pvmControllerRunRealAnalysis(controller, table);
    pvmMasterSessionStop(&controller->realSession);
    return result;
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
    if (controller->mode == pvmModeReal) {
        pvmMasterSessionStop(&controller->realSession);
    }
    controller->started = 0;
}
