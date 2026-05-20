// === src/domain/scheduler/preemption.h ===

#ifndef PREEMPTION_H
#define PREEMPTION_H

struct ProcessTable;
struct Process;

typedef struct PreemptionController {
    int isPreemptive;
    int preemptionCount;
    int lastPreemptionTime;
} PreemptionController;

PreemptionController* preemptionControllerCreate(int isPreemptive);

void preemptionControllerDestroy(PreemptionController* controller);

int preemptionControllerCanPreempt(PreemptionController* controller, struct Process* current, struct Process* candidate);

void preemptionControllerOnPreemption(PreemptionController* controller);

int preemptionControllerGetPreemptionCount(PreemptionController* controller);

void preemptionControllerSetPreemptive(PreemptionController* controller, int isPreemptive);

#endif
