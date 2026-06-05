#include "process.h"
#include <stdlib.h>
#include <string.h>

Process* processCreateWithBcp(Bcp* bcp) {
    if (!bcp) return NULL;
    Process* proc = (Process*)calloc(1, sizeof(Process));
    if (!proc) return NULL;
    proc->bcp = bcp;
    proc->isActive = 0;
    return proc;
}

Process* processCreate(const char* processId, int pid) {
    Process* proc = (Process*)calloc(1, sizeof(Process));
    if (!proc) return NULL;
    proc->bcp = bcpCreate(processId, pid);
    if (!proc->bcp) { free(proc); return NULL; }
    proc->isActive = 0;
    return proc;
}

void processDestroy(Process* process) {
    if (process) {
        free(process);
    }
}

void processActivate(Process* process)   { if (process) process->isActive = 1; }
void processDeactivate(Process* process) { if (process) process->isActive = 0; }
int  processIsActive(Process* process)   { return process ? process->isActive : 0; }
