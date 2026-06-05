#ifndef PVM_CONTROLLER_H
#define PVM_CONTROLLER_H

struct ProcessTable;
struct RrScheduler;

typedef enum {
    pvmModeReal,
    pvmModeFake
} PvmMode;

typedef struct PvmController PvmController;

PvmController* pvmControllerCreate(PvmMode mode);
int pvmControllerStart(PvmController* controller);
int pvmControllerRunStatsTask(PvmController* controller, struct ProcessTable* table);
int pvmControllerRunAgingTask(PvmController* controller, struct ProcessTable* table, struct RrScheduler* rrScheduler);
int pvmControllerPrintResults(PvmController* controller);
void pvmControllerDestroy(PvmController* controller);

#endif
