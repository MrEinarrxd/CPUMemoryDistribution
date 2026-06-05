#ifndef PROCESS_GENERATOR_H
#define PROCESS_GENERATOR_H

struct ProcessTable;
struct Bcp;

void processGeneratorInit(void);
struct Bcp* processGeneratorGetByIndex(int index);
int processGeneratorGetCount(void);
int processGeneratorGetNextSleepTime(void);
void processGeneratorCleanup(void);

#endif
