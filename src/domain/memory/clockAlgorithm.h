// === src/domain/memory/clockAlgorithm.h ===

#ifndef CLOCK_ALGORITHM_H
#define CLOCK_ALGORITHM_H

#include "memoria.h"

// Solo referencia, no destruye Marco array
typedef struct ClockAlgorithm {
    Marco** frames;
    int clockPointer;
    int totalFrames;
} ClockAlgorithm;

ClockAlgorithm* clockCreate(Marco** frames, int totalFrames);

void clockDestroy(ClockAlgorithm* ca);

int clockSelectVictim(ClockAlgorithm* ca);

void clockUpdateReferenceBit(ClockAlgorithm* ca, int frameIndex, int value);

void clockReset(ClockAlgorithm* ca);

#endif
