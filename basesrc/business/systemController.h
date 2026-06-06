// === src/business/systemController.h ===

#ifndef SYSTEM_CONTROLLER_H
#define SYSTEM_CONTROLLER_H

typedef struct SystemController SystemController;

SystemController* systemControllerCreate(void);

void systemControllerDestroy(SystemController* controller);

int systemControllerInit(SystemController* controller);

int systemControllerRun(SystemController* controller);

int systemControllerRunPvm(SystemController* controller);
// Ejecuta las tareas distribuidas PVM (tarea 1 y tarea 2)

int systemControllerHandleCommand(SystemController* controller, int command);

int systemControllerCycle(SystemController* controller);

#endif
