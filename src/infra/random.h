#ifndef INFRA_RANDOM_H
#define INFRA_RANDOM_H

#include "../simulation/config.h"

typedef struct RandomSource {
    unsigned int seed;
    int growth_values[SIM_GROWTH_LIST_SIZE];
    int growth_index;
    int growth_initialized;
} RandomSource;

void random_source_init(RandomSource* random, unsigned int seed);
int random_int(RandomSource* random, int min, int max);
void random_unique_arrivals(RandomSource* random, int* out_values, int count, int min, int max);
int random_memory_growth(RandomSource* random);

#endif
