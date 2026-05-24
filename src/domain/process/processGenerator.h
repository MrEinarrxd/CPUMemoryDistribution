// === src/domain/process/processGenerator.h ===

#ifndef PROCESS_GENERATOR_H
#define PROCESS_GENERATOR_H

struct ProcessTable;

// Process generator
// Generates all 250 processes with unique IDs and arrival times
// Assigns random CPU cycles and memory, uses sleep between creations

// Initialize process generator
void processGeneratorInit(void);

// Run process generation on process table
void processGeneratorRun(struct ProcessTable* table);

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
