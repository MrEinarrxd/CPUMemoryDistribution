#ifndef MEMORY_RESIZE_H
#define MEMORY_RESIZE_H

#include "../../utils/constants.h"

struct PagingManager;
struct ProcessTable;

typedef struct MemoryResizer {
    int resizeAttempts;
    int successfulResizes;
    int lastResizeCycle;
} MemoryResizer;

MemoryResizer* memoryResizerCreate(void);
void memoryResizerDestroy(MemoryResizer* resizer);
void memoryResizerExecute(MemoryResizer* resizer, struct ProcessTable* table, struct PagingManager* paging);
int memoryResizerGetFrameCount(struct PagingManager* paging, int processIndex);
int memoryResizerGetAttempts(MemoryResizer* resizer);
int memoryResizerGetSuccessful(MemoryResizer* resizer);

#endif
