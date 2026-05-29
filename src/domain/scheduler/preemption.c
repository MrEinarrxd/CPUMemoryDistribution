#include "preemption.h"
#include <stdlib.h>

PreemptionController* preemptionControllerCreate(int isPreemptive) {
    PreemptionController* pc = (PreemptionController*)calloc(1, sizeof(PreemptionController));
    if (pc) pc->isPreemptive = isPreemptive;
    return pc;
}
void preemptionControllerDestroy(PreemptionController* controller) { free(controller); }
int preemptionControllerCanPreempt(PreemptionController* controller, struct Process* current, struct Process* candidate) {
    if (!controller || !controller->isPreemptive) return 0;
    (void)current; (void)candidate;
    return 0;
}
void preemptionControllerOnPreemption(PreemptionController* controller) { if (controller) controller->preemptionCount++; }
int preemptionControllerGetPreemptionCount(PreemptionController* controller) { return controller ? controller->preemptionCount : 0; }
void preemptionControllerSetPreemptive(PreemptionController* controller, int isPreemptive) { if (controller) controller->isPreemptive = isPreemptive; }

//Parte 6 – Dominio: Entrada/Salida (I/O)