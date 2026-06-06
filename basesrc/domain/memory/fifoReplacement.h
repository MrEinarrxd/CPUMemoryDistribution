// === src/domain/memory/fifoReplacement.h ===

#ifndef FIFO_REPLACEMENT_H
#define FIFO_REPLACEMENT_H

#include "../../utils/constants.h"
#include "memoria.h"

// FIFO queue structure for page replacement
typedef struct FifoQueue {
    int* pageIds;                                       // Array of page IDs in queue
    int head;                                           // Head pointer
    int tail;                                           // Tail pointer
    int count;                                          // Current number of pages in queue
    int capacity;                                       // Maximum capacity
} FifoQueue;

// FIFO page replacement algorithm for swap memory
// Oldest page is replaced first
typedef struct FifoReplacement {
    FifoQueue fifoQueue;                                // FIFO queue of pages
    int totalReplacements;                              // Total replacement operations performed
    Marco** frames;                                     // Pointer to frame array (NOT owned)
    int totalFrames;                                    // Number of frames available
} FifoReplacement;

// Create FIFO replacement handler with frame references
FifoReplacement* fifoReplacementCreate(Marco** frames, int totalFrames);

// Destroy FIFO replacement handler
void fifoReplacementDestroy(FifoReplacement* fifo);

// Select victim page to be replaced, returns page ID
int fifoReplacementSelectVictim(FifoReplacement* fifo);

// Add page to FIFO queue
void fifoReplacementAddPage(FifoReplacement* fifo, int pageId);

// Remove page from FIFO queue
void fifoReplacementRemovePage(FifoReplacement* fifo, int pageId);

// Get total number of replacement operations performed
int fifoReplacementGetTotalReplacements(FifoReplacement* fifo);

#endif
