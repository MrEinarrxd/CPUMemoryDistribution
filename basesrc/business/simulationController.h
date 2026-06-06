#ifndef SIMULATION_CONTROLLER_H
#define SIMULATION_CONTROLLER_H

#include "pvmController.h"

typedef struct SimulationController SimulationController;

typedef enum {
    SimulationRunModeLocal = 1,
    SimulationRunModePvm = 2
} SimulationRunMode;

SimulationController* simulationControllerCreate(void);
void simulationControllerDestroy(SimulationController* controller);
int simulationControllerInit(SimulationController* controller);
int simulationControllerRun(SimulationController* controller);
int simulationControllerRunFull(SimulationController* controller, PvmMode pvmMode);
int simulationControllerRunPvm(SimulationController* controller);
void simulationControllerSetRunMode(SimulationController* controller, SimulationRunMode mode);
SimulationRunMode simulationControllerGetRunMode(const SimulationController* controller);
int simulationControllerHandleCommand(SimulationController* controller, int command);
int simulationControllerCycle(SimulationController* controller);

#endif
