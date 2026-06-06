#include "random.h"

#include <stdlib.h>
#include <time.h>

void random_source_init(RandomSource* random, unsigned int seed) {
    if (!random) return;
    random->seed = seed == 0 ? (unsigned int)time(NULL) : seed;
    random->growth_index = 0;
    random->growth_initialized = 0;
    srand(random->seed);
}

int random_int(RandomSource* random, int min, int max) {
    (void)random;
    if (min > max) {
        int tmp = min;
        min = max;
        max = tmp;
    }
    return min + rand() % (max - min + 1);
}

void random_unique_arrivals(RandomSource* random, int* out_values, int count, int min, int max) {
    if (!out_values || count <= 0) return;
    int range = max - min + 1;
    if (range < count) {
        for (int i = 0; i < count; i++) out_values[i] = random_int(random, min, max);
        return;
    }

    int* pool = (int*)malloc(sizeof(int) * (size_t)range);
    if (!pool) {
        for (int i = 0; i < count; i++) out_values[i] = random_int(random, min, max);
        return;
    }

    for (int i = 0; i < range; i++) pool[i] = min + i;
    for (int i = range - 1; i > 0; i--) {
        int j = random_int(random, 0, i);
        int tmp = pool[i];
        pool[i] = pool[j];
        pool[j] = tmp;
    }
    for (int i = 0; i < count; i++) out_values[i] = pool[i];
    free(pool);
}

int random_memory_growth(RandomSource* random) {
    if (!random) return 0;
    if (!random->growth_initialized) {
        int pos = 0;
        for (int i = 0; i < SIM_GROWTH_LIST_SIZE - 5; i++) random->growth_values[pos++] = 0;
        for (int i = 0; i < 5; i++) random->growth_values[pos++] = random_int(random, 1, 50);
        for (int i = SIM_GROWTH_LIST_SIZE - 1; i > 0; i--) {
            int j = random_int(random, 0, i);
            int tmp = random->growth_values[i];
            random->growth_values[i] = random->growth_values[j];
            random->growth_values[j] = tmp;
        }
        random->growth_index = 0;
        random->growth_initialized = 1;
    }
    int value = random->growth_values[random->growth_index];
    random->growth_index = (random->growth_index + 1) % SIM_GROWTH_LIST_SIZE;
    return value;
}
