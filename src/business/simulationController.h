#ifndef CpuMemorySimulationControllerH
#define CpuMemorySimulationControllerH

#include "pvmController.h"

typedef struct SimulationController SimulationController;

SimulationController* simulationControllerCreate(PvmMode pvmMode);
int simulationControllerInit(SimulationController* controller);
int simulationControllerRun(SimulationController* controller);
void simulationControllerDestroy(SimulationController* controller);

#endif
