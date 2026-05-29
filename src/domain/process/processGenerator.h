#ifndef PROCESS_GENERATOR_H
#define PROCESS_GENERATOR_H

struct ProcessTable;

void processGeneratorInit(void);
void processGeneratorRun(struct ProcessTable* table);
struct Bcp* processGeneratorGetNext(void);
int processGeneratorHasMore(void);
void processGeneratorReset(void);
int processGeneratorGetNextSleepTime(void);
void processGeneratorAssignUniqueIds(struct ProcessTable* table, int count);
void processGeneratorAssignUniqueArrivalTimes(struct ProcessTable* table, int count);
void processGeneratorAssignCpuCycles(struct ProcessTable* table, int count);
void processGeneratorAssignMemory(struct ProcessTable* table, int count);
void processGeneratorCleanup(void);

#endif