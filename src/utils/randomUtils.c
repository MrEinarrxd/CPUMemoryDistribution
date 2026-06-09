#include "randomUtils.h"
#include "constants.h"

#include <stdlib.h>
#include <time.h>

void randomInit(unsigned int seed) {
    srand(seed == 0 ? (unsigned int)time(NULL) : seed);
}

int randomInt(int min, int max) {
    if (min > max) {
        int tmp = min;
        min = max;
        max = tmp;
    }
    return min + rand() % (max - min + 1);
}

int randomChance(int percent) {
    if (percent <= 0) return 0;
    if (percent >= 100) return 1;
    return randomInt(1, 100) <= percent;
}

void randomUniqueArrivals(int outValues[], int count) {
    int range = ArrivalMax - ArrivalMin + 1;
    int pool[ArrivalMax - ArrivalMin + 1];

    for (int i = 0; i < range; ++i) {
        pool[i] = ArrivalMin + i;
    }

    for (int i = range - 1; i > 0; --i) {
        int j = randomInt(0, i);
        int tmp = pool[i];
        pool[i] = pool[j];
        pool[j] = tmp;
    }

    for (int i = 0; i < count && i < range; ++i) {
        outValues[i] = pool[i];
    }
}

int randomCpuCycles(void) {
    return randomInt(CpuCyclesMin, CpuCyclesMax);
}

int randomCpuInstanceCycles(void) {
    return randomInt(CpuInstanceMin, CpuInstanceMax);
}

int randomContextSwitchTime(void) {
    return randomInt(ContextSwitchMin, ContextSwitchMax);
}

int randomCreationSleep(void) {
    return randomInt(CreationSleepMin, CreationSleepMax);
}

int randomMemoryGrowth(void) {
    static int values[MemoryGrowthListSize];
    static int initialized = 0;
    static int cursor = 0;

    if (!initialized) {
        int pos = 0;
        for (; pos < MemoryGrowthListSize - MemoryGrowthNonZero; ++pos) {
            values[pos] = 0;
        }
        for (; pos < MemoryGrowthListSize; ++pos) {
            values[pos] = randomInt(MemoryGrowthMin, MemoryGrowthMax);
        }
        for (int i = MemoryGrowthListSize - 1; i > 0; --i) {
            int j = randomInt(0, i);
            int tmp = values[i];
            values[i] = values[j];
            values[j] = tmp;
        }
        initialized = 1;
    }

    int value = values[cursor];
    cursor = (cursor + 1) % MemoryGrowthListSize;
    return value;
}

int randomEvenPageCount(void) {
    int value = randomInt(MinPageFrames, MaxPageFrames);
    if ((value % 2) != 0) value++;
    if (value > MaxPageFrames) value = MaxPageFrames;
    return value;
}

int randomIoDevice(void) {
    return randomInt(0, IoDeviceCount - 1);
}

int randomIoCycles(void) {
    return randomInt(IoCyclesMin, IoCyclesMax);
}
