#ifndef SYSTEM_CONTROLLER_H
#define SYSTEM_CONTROLLER_H

typedef struct SystemController SystemController;

typedef enum {
    SystemRunModeLocal = 1,
    SystemRunModePvm = 2
} SystemRunMode;

SystemController* systemControllerCreate(void);
void systemControllerDestroy(SystemController* controller);
int systemControllerInit(SystemController* controller);
int systemControllerRun(SystemController* controller);
int systemControllerRunPvm(SystemController* controller);
void systemControllerSetRunMode(SystemController* controller, SystemRunMode mode);
SystemRunMode systemControllerGetRunMode(const SystemController* controller);
int systemControllerHandleCommand(SystemController* controller, int command);
int systemControllerCycle(SystemController* controller);

#endif
