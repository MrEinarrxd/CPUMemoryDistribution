#include "memoryController.h"
#include "../utils/constants.h"
#include <stdlib.h>

MemoryController* memoryControllerCreate(void) {
    MemoryController* controller = (MemoryController*)calloc(1, sizeof(MemoryController));
    if (!controller) return NULL;
    controller->bitmapManager = bitmapManagerCreate(1);
    if (!controller->bitmapManager) { memoryControllerDestroy(controller); return NULL; }
    controller->swapManager = swapManagerCreate(maxSwap);
    if (!controller->swapManager) { memoryControllerDestroy(controller); return NULL; }
    controller->pagingManager = pagingManagerCreate(maxMarcos, controller->bitmapManager);
    if (!controller->pagingManager) { memoryControllerDestroy(controller); return NULL; }
    pagingManagerSetSwapManager(controller->pagingManager, controller->swapManager);
    controller->memoryResizer = memoryResizerCreate();
    if (!controller->memoryResizer) { memoryControllerDestroy(controller); return NULL; }
    return controller;
}

void memoryControllerDestroy(MemoryController* controller) {
    if (!controller) return;
    if (controller->memoryResizer) memoryResizerDestroy(controller->memoryResizer);
    if (controller->pagingManager) pagingManagerDestroy(controller->pagingManager);
    if (controller->swapManager) swapManagerDestroy(controller->swapManager);
    if (controller->bitmapManager) bitmapManagerDestroy(controller->bitmapManager);
    free(controller);
}

int memoryControllerAllocateProcess(MemoryController* controller, int processIndex, int pageCount) {
    if (!controller || !controller->pagingManager) return -1;
    return pagingManagerAllocatePageForProcess(controller->pagingManager, processIndex, pageCount);
}

int memoryControllerGrowProcess(MemoryController* controller, int processIndex, int newPageCount) {
    if (!controller || !controller->pagingManager) return -1;
    return pagingManagerResizeFrames(controller->pagingManager, processIndex, newPageCount);
}

int memoryControllerHandlePageFault(MemoryController* controller, int processIndex, int pageNumber, const char* missingWord) {
    if (!controller || !controller->pagingManager) return -1;
    return pagingManagerHandlePageFault(controller->pagingManager, processIndex, pageNumber, missingWord);
}
