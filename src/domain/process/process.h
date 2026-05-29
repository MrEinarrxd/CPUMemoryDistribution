#ifndef PROCESS_H
#define PROCESS_H

#include "bcp.h"

typedef struct Process {
    Bcp* bcp;
    int isActive;
} Process;

Process* processCreate(const char* processId, int pid);
void processDestroy(Process* process);
void processActivate(Process* process);
void processDeactivate(Process* process);
int processIsActive(Process* process);

#endif