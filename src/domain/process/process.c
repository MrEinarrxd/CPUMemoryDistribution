// src/domain/process/process.c
// CORRECCIÓN: processCreate ya NO crea BCP interno (evita fuga de memoria).
//             processDestroy ya NO libera el BCP (el generador es el dueño).

#include "process.h"
#include <stdlib.h>
#include <string.h>

// Nueva función: crea un Process y asigna un BCP ya existente.
// Úsala siempre en lugar de processCreate + asignación manual.
Process* processCreateWithBcp(Bcp* bcp) {
    if (!bcp) return NULL;
    Process* proc = (Process*)calloc(1, sizeof(Process));
    if (!proc) return NULL;
    proc->bcp = bcp;
    proc->isActive = 0;
    return proc;
}

// Mantenida por compatibilidad con el resto del código que aún la llama.
// Crea un BCP propio — solo úsala cuando el BCP no viene del generador.
Process* processCreate(const char* processId, int pid) {
    Process* proc = (Process*)calloc(1, sizeof(Process));
    if (!proc) return NULL;
    proc->bcp = bcpCreate(processId, pid);
    if (!proc->bcp) { free(proc); return NULL; }
    proc->isActive = 0;
    return proc;
}

// CORRECCIÓN: NO libera bcp->bcp aquí.
// El generador (processGeneratorCleanup) es el único responsable de liberar los BCPs.
void processDestroy(Process* process) {
    if (process) {
        // bcpDestroy(process->bcp);  <-- eliminado para evitar double-free
        free(process);
    }
}

void processActivate(Process* process)   { if (process) process->isActive = 1; }
void processDeactivate(Process* process) { if (process) process->isActive = 0; }
int  processIsActive(Process* process)   { return process ? process->isActive : 0; }
