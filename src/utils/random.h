#ifndef RANDOM_H
#define RANDOM_H

#include "constants.h"

void initRandom(unsigned int seed);
int randomInt(int min, int max);
float randomFloat(float min, float max);
void generateUniqueArrivalTimes(int* arrivalTimes, int processCount);
int randomCpuCycles(void);
int randomCpuInstanceCycles(void);
int randomContextSwitchTime(void);
int randomMemoryGrowth(void);

#endif
