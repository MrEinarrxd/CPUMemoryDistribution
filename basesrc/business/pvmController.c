#include "pvmController.h"
#include "../domain/distributed/fakePvmMaster.h"
#include "../domain/distributed/realPvmMaster.h"
#include "../utils/constants.h"
#include "../utils/errorHandler.h"
#include "../presentation/consoleIo.h"
#include <stdlib.h>

struct PvmController {
    PvmMode mode;
    PvmMaster* realMaster;
    FakePvmMaster* fakeMaster;
};

PvmController* pvmControllerCreate(PvmMode mode) {
    PvmController* controller = (PvmController*)calloc(1, sizeof(PvmController));
    if (!controller) return NULL;
    controller->mode = mode;
    return controller;
}

int pvmControllerStart(PvmController* controller) {
    if (!controller) return -1;
    if (controller->mode == pvmModeFake) {
        controller->fakeMaster = fakePvmMasterCreate();
        return controller->fakeMaster ? 0 : -1;
    }
#if pvmModeEnabled
    const char* hosts = getenv(pvmSlaveHostsEnvVar);
    if (!hosts || hosts[0] == '\0') {
        consoleIoPrintLine("PVM real requiere exportar PVM_SLAVE_HOSTS con dos hosts.");
        return -1;
    }
    controller->realMaster = pvmMasterInit();
    if (!controller->realMaster) {
        consoleIoPrintLine("PVM real no pudo iniciar: verifique que pvmd esté corriendo.");
        return -1;
    }
    int spawned = pvmMasterSpawnSlaves(controller->realMaster);
    if (spawned != pvmNumEsclavos)
        consoleIoPrintLine("PVM real no pudo lanzar los dos slaves requeridos.");
    return spawned == pvmNumEsclavos ? 0 : -1;
#else
    errorHandlerLog(ErrorCodeNodeConnectionFailed, "pvmControllerStart: PVM no habilitado");
    return -1;
#endif
}

int pvmControllerRunStatsTask(PvmController* controller, struct ProcessTable* table) {
    if (!controller || !table) return -1;
    if (controller->mode == pvmModeFake) return fakePvmMasterRunStatsTask(controller->fakeMaster, table);
#if pvmModeEnabled
    if (!controller->realMaster) return -1;
    controller->realMaster->processTable = table;
    pvmMasterTask1Stats(controller->realMaster);
    return 0;
#else
    return -1;
#endif
}

int pvmControllerRunAgingTask(PvmController* controller, struct ProcessTable* table, struct RrScheduler* rrScheduler) {
    if (!controller || !table) return -1;
    if (controller->mode == pvmModeFake) return fakePvmMasterRunAgingTask(controller->fakeMaster, table);
#if pvmModeEnabled
    if (!controller->realMaster) return -1;
    controller->realMaster->processTable = table;
    controller->realMaster->rrScheduler = rrScheduler;
    pvmMasterTask2Aging(controller->realMaster);
    return 0;
#else
    return -1;
#endif
}

int pvmControllerPrintResults(PvmController* controller) {
    if (!controller) return -1;
    if (controller->mode == pvmModeFake) {
        fakePvmMasterIntegrateResults(controller->fakeMaster);
        fakePvmMasterPrintResults(controller->fakeMaster);
        return 0;
    }
#if pvmModeEnabled
    if (!controller->realMaster) return -1;
    pvmMasterIntegrateResults(controller->realMaster);
    pvmMasterPrintResults(controller->realMaster);
    return 0;
#else
    return -1;
#endif
}

void pvmControllerDestroy(PvmController* controller) {
    if (!controller) return;
    if (controller->mode == pvmModeFake) {
        fakePvmMasterDestroy(controller->fakeMaster);
    } else if (controller->realMaster) {
        pvmMasterCleanup(controller->realMaster);
    }
    free(controller);
}
