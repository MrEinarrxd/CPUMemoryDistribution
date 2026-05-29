#ifndef FIFO_REPLACEMENT_H
#define FIFO_REPLACEMENT_H

#include "../../utils/constants.h"
#include "memoria.h"

typedef struct FifoQueue {
    int* pageIds;
    int head;
    int tail;
    int count;
    int capacity;
} FifoQueue;

typedef struct FifoReplacement {
    FifoQueue fifoQueue;
    int totalReplacements;
    Marco** frames;
    int totalFrames;
} FifoReplacement;

FifoReplacement* fifoReplacementCreate(Marco** frames, int totalFrames);
void fifoReplacementDestroy(FifoReplacement* fifo);
int fifoReplacementSelectVictim(FifoReplacement* fifo);
void fifoReplacementAddPage(FifoReplacement* fifo, int pageId);
void fifoReplacementRemovePage(FifoReplacement* fifo, int pageId);
int fifoReplacementGetTotalReplacements(FifoReplacement* fifo);

#endif
