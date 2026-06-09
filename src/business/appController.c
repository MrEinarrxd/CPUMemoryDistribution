#include "appController.h"
#include "guiController.h"
#include "pvmController.h"
#include "simulationController.h"
#include "../utils/constants.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int runSimulation(PvmMode mode) {
    int result;
    SimulationController* simulation = simulationControllerCreate(mode);
    if (!simulation) return 1;
    if (simulationControllerInit(simulation) != 0) {
        if (mode == pvmModeReal) {
            const char* slaveExec = getenv("PVM_SLAVE_EXEC");
            const char* slaveHosts = getenv("PVM_SLAVE_HOSTS");
            if (!slaveExec || slaveExec[0] == '\0') slaveExec = DefaultPvmSlaveExec;
            if (!slaveHosts || slaveHosts[0] == '\0') slaveHosts = DefaultPvmSlaveHosts;
            printf("[PVM ERROR] No se pudo iniciar PVM real. Verifique pvmd, %s y hosts %s.\n",
                   slaveExec, slaveHosts);
        }
        simulationControllerDestroy(simulation);
        return 1;
    }
    result = simulationControllerRun(simulation);
    simulationControllerDestroy(simulation);
    return result == 0 ? 0 : 1;
}

static int modeFromText(const char* text, PvmMode* mode) {
    if (!text || !mode) return 0;
    if (strcmp(text, "real") == 0) {
        *mode = pvmModeReal;
        return 1;
    }
    if (strcmp(text, "demo") == 0 || strcmp(text, "virtual") == 0 ||
        strcmp(text, "local") == 0 || strcmp(text, "fake") == 0) {
        *mode = pvmModeLocal;
        return 1;
    }
    return 0;
}

int appControllerRun(void) {
    int option;
    PvmMode envMode;
    const char* envModeText = getenv("CPUMEM_PVM_MODE");

    if (modeFromText(envModeText, &envMode)) {
        return runSimulation(envMode);
    }

    if (!isatty(STDIN_FILENO)) {
        return runSimulation(pvmModeLocal);
    }

    option = guiControllerShowMainMenu();
    switch (option) {
        case 1:
            return runSimulation(pvmModeReal);
        case 2:
            return runSimulation(pvmModeLocal);
        case 0:
            return 0;
        default:
            printf("Opcion invalida.\n");
            return 1;
    }
}
