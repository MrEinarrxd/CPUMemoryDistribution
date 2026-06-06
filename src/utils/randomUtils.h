#ifndef CpuMemoryRandomUtilsH
#define CpuMemoryRandomUtilsH

void randomInit(unsigned int seed);
int randomInt(int min, int max);
int randomChance(int percent);
void randomUniqueArrivals(int outValues[], int count);
int randomCpuCycles(void);
int randomCpuInstanceCycles(void);
int randomContextSwitchTime(void);
int randomCreationSleep(void);
int randomMemoryGrowth(void);
int randomEvenPageCount(void);
int randomIoDevice(void);
int randomIoCycles(void);

#endif
