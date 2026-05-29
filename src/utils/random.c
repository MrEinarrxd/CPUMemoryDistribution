#include "random.h"
#include <stdlib.h>
#include <time.h>
#include <string.h>

void initRandom(unsigned int seed) {
    unsigned int s = seed;
    if (s == 0) {
        s = (unsigned int)time(NULL);
    }
    srand(s);
}

int randomInt(int min, int max) {
    if (min > max) {
        int tmp = min;
        min = max;
        max = tmp;
    }
    int range = max - min + 1;
    if (range <= 0) {
        return min;
    }
    return min + (rand() % range);
}

float randomFloat(float min, float max) {
    if (min > max) {
        float tmp = min;
        min = max;
        max = tmp;
    }
    float r = (float)rand() / ((float)RAND_MAX + 1.0f);
    return min + r * (max - min);
}

void generateUniqueArrivalTimes(int* arrivalTimes, int processCount) {
    if (arrivalTimes == NULL || processCount <= 0) return;
    int min = tiempoLlegadaMin;
    int max = tiempoLlegadaMax;
    int rangeSize = max - min + 1;
    if (rangeSize >= processCount) {
        int *pool = (int*)malloc(sizeof(int) * (size_t)rangeSize);
        if (pool == NULL) {
            for (int i = 0; i < processCount; ++i) {
                arrivalTimes[i] = randomInt(min, max);
            }
            return;
        }
        for (int i = 0; i < rangeSize; ++i) {
            pool[i] = min + i;
        }
        for (int i = rangeSize - 1; i > 0; --i) {
            int j = randomInt(0, i);
            int tmp = pool[i];
            pool[i] = pool[j];
            pool[j] = tmp;
        }
        for (int i = 0; i < processCount; ++i) {
            arrivalTimes[i] = pool[i];
        }
        free(pool);
        return;
    }
    for (int i = 0; i < processCount; ++i) {
        arrivalTimes[i] = randomInt(min, max);
    }
}

int randomCpuCycles(void) {
    return randomInt(ciclosCpuMin, ciclosCpuMax);
}

int randomCpuInstanceCycles(void) {
    return randomInt(ciclosPorInstanciaMin, ciclosPorInstanciaMax);
}

int randomContextSwitchTime(void) {
    return randomInt(cambioContextoMin, cambioContextoMax);
}

int randomMemoryGrowth(void) {
    static int list[growthListSize];
    static int initialized = 0;
    static int idx = 0;
    if (!initialized) {
        initialized = 1;
        int zeros = growthListSize - 5;
        int pos = 0;
        for (int i = 0; i < zeros; ++i) {
            list[pos++] = 0;
        }
        for (int i = 0; i < 5; ++i) {
            list[pos++] = randomInt(1, 50);
        }
        for (int i = growthListSize - 1; i > 0; --i) {
            int j = randomInt(0, i);
            int tmp = list[i];
            list[i] = list[j];
            list[j] = tmp;
        }
        idx = 0;
    }
    int value = list[idx];
    idx = (idx + 1) % growthListSize;
    return value;
}