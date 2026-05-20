// === src/business/systemController.h ===

#ifndef SYSTEM_CONTROLLER_H
#define SYSTEM_CONTROLLER_H

typedef struct SystemController SystemController;

SystemController* systemControllerCreate(void);

void systemControllerDestroy(SystemController* controller);

int systemControllerInit(SystemController* controller);

int systemControllerRun(SystemController* controller);

int systemControllerHandleCommand(SystemController* controller, int command);

int systemControllerCycle(SystemController* controller);

#endif
