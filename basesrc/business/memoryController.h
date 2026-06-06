#ifndef MEMORY_CONTROLLER_H
#define MEMORY_CONTROLLER_H

#include "../domain/memory/bitmapManager.h"
#include "../domain/memory/pagingManager.h"
#include "../domain/memory/swapManager.h"
#include "../domain/memory/memoryResize.h"

typedef struct MemoryController {
    BitmapManager* bitmapManager;
    PagingManager* pagingManager;
    SwapManager* swapManager;
    MemoryResizer* memoryResizer;
} MemoryController;

MemoryController* memoryControllerCreate(void);
void memoryControllerDestroy(MemoryController* controller);
int memoryControllerAllocateProcess(MemoryController* controller, int processIndex, int pageCount);
int memoryControllerGrowProcess(MemoryController* controller, int processIndex, int newPageCount);
int memoryControllerHandlePageFault(MemoryController* controller, int processIndex, int pageNumber, const char* missingWord);

#endif
