#include "fcfsScheduler.h"
#include "readyQueue.h"
#include "../process/processTable.h"
#include <stdlib.h>

FcfsScheduler* fcfsSchedulerCreate(void) {
    FcfsScheduler* scheduler = (FcfsScheduler*)calloc(1, sizeof(FcfsScheduler));
    if (scheduler) {
        scheduler->totalScheduled = 0;
        scheduler->contextSwitchCount = 0;
    }
    return scheduler;
}

void fcfsSchedulerDestroy(FcfsScheduler* scheduler) {
    free(scheduler);
}

Process* fcfsSchedulerSelectNext(FcfsScheduler* scheduler, ProcessTable* table) {
    if (!scheduler || !table || !table->readyQueue) return NULL;
    Process* next = readyQueueDequeue(table->readyQueue);
    if (next) scheduler->totalScheduled++;
    return next;
}

void fcfsSchedulerOnContextSwitch(FcfsScheduler* scheduler) {
    if (scheduler) {
        scheduler->contextSwitchCount++;
    }
}
