// === src/domain/process/processGenerator.h ===

#ifndef PROCESS_GENERATOR_H
#define PROCESS_GENERATOR_H

struct ProcessTable;

// Process generator
// Generates all 250 processes with unique IDs and arrival times
// Assigns random CPU cycles and memory, uses sleep between creations

// Initialize process generator (prepara lista de 250 procesos no creados)
void processGeneratorInit(void);

// [DEPRECATED] Run process generation on process table (crea todos de golpe)
void processGeneratorRun(struct ProcessTable* table);

// ============ GENERADOR INCREMENTAL (NEW) ============
// Devuelve el siguiente proceso a crear, o NULL si todos están creados
struct Bcp* processGeneratorGetNext(void);

// Devuelve verdadero si hay procesos pendientes de crear
int processGeneratorHasMore(void);

// Reinicia el generador (para pruebas)
void processGeneratorReset(void);

// Obtiene el tiempo de sleep hasta próxima creación (1-50 ciclos)
int processGeneratorGetNextSleepTime(void);

// ============ FUNCIONES AUXILIARES ============
// Assign unique process IDs to all processes
void processGeneratorAssignUniqueIds(struct ProcessTable* table, int count);

// Assign unique arrival times to all processes
void processGeneratorAssignUniqueArrivalTimes(struct ProcessTable* table, int count);

// Assign random CPU cycles to all processes
void processGeneratorAssignCpuCycles(struct ProcessTable* table, int count);

// Assign random memory requirements to all processes
void processGeneratorAssignMemory(struct ProcessTable* table, int count);

// Cleanup process generator resources
void processGeneratorCleanup(void);

#endif
