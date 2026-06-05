#ifndef PROCESS_H
#define PROCESS_H

#include "bcp.h"

typedef struct Process {
    Bcp* bcp;
    int isActive;
} Process;

// Crea Process asignando un BCP externo ya existente (uso normal con el generador).
Process* processCreateWithBcp(Bcp* bcp);

// Crea Process con BCP propio (solo si el BCP no viene del generador).
Process* processCreate(const char* processId, int pid);

void processDestroy(Process* process);
void processActivate(Process* process);
void processDeactivate(Process* process);
int  processIsActive(Process* process);

#endif
