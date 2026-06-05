#include "appController.h"
#include "guiController.h"
#include "simulationController.h"
#include "../presentation/consoleIo.h"
#include "../presentation/uiTexts.h"
#include <stdlib.h>

struct AppController {
    int lastOption;
};

AppController* appControllerCreate(void) {
    return (AppController*)calloc(1, sizeof(AppController));
}

/* Coordina el flujo inicial y deja la lógica pesada en los controllers de dominio. */
int appControllerRun(AppController* controller) {
    if (!controller) return -1;
    consoleIoInit();
    controller->lastOption = guiControllerShowMainMenu();
    consoleIoCleanup();

    SimulationController* simulation = simulationControllerCreate();
    if (!simulation) return -1;
    if (simulationControllerInit(simulation) != 0) {
        simulationControllerDestroy(simulation);
        return -1;
    }

    int result = 0;
    switch (controller->lastOption) {
        case 1:
            result = simulationControllerRunFull(simulation, pvmModeReal);
            break;
        case 2:
            result = simulationControllerRunFull(simulation, pvmModeFake);
            break;
        case 3:
            result = simulationControllerRunPvm(simulation);
            break;
        default:
            consoleIoPrintLine(UiOpcionInvalida);
            result = -1;
            break;
    }

    simulationControllerDestroy(simulation);
    return result;
}

void appControllerDestroy(AppController* controller) {
    free(controller);
}
