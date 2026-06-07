#ifndef CpuMemoryPvmControllerH
#define CpuMemoryPvmControllerH

#include "../distributed/pvmMaster.h"
#include "../domain/process/processTable.h"

typedef enum PvmMode {
    pvmModeReal = 0,
    pvmModeLocal = 1,
    pvmModeFake = pvmModeLocal,
    pvmModeDisabled = 2
} PvmMode;

typedef struct PvmController {
    PvmMode mode;
    int started;
    int lastAnalysisIteration;
    int analysisCount;
    PvmMasterSession realSession;
    char statusText[96];
    DistributedReport lastReport;
} PvmController;

void pvmControllerInit(PvmController* controller, PvmMode mode);
int pvmControllerStart(PvmController* controller);
int pvmControllerRunPeriodic(PvmController* controller, const ProcessTable* table, int currentIteration);
int pvmControllerRunFinal(PvmController* controller, const ProcessTable* table);
int pvmControllerRunTest(void);
void pvmControllerPrintReport(const char* title, const DistributedReport* report);
void pvmControllerDestroy(PvmController* controller);

#endif
