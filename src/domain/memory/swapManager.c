#include "swapManager.h"
#include <stdlib.h>
#include <string.h>

typedef struct SwapMetadata {
    int address;
    int size;
    int inUse;
} SwapMetadata;

SwapManager* swapManagerCreate(int swapSize) {
    SwapManager* sm = (SwapManager*)calloc(1, sizeof(SwapManager));
    if (!sm) return NULL;
    sm->totalSwapSize = swapSize;
    sm->usedSwapSize = 0;
    sm->swapOperations = 0;
    sm->metadata = (SwapMetadata*)calloc(swapSize, sizeof(SwapMetadata));
    if (!sm->metadata) { free(sm); return NULL; }
    return sm;
}

void swapManagerDestroy(SwapManager* manager) {
    if (manager) { free(manager->metadata); free(manager); }
}

int swapManagerWrite(SwapManager* manager, const void* buffer, int size, int* outAddress) {
    if (!manager || !buffer || size <= 0 || !outAddress) return -1;
    if (manager->usedSwapSize + size > manager->totalSwapSize) return -1;
    *outAddress = manager->usedSwapSize;
    manager->metadata[*outAddress].address = *outAddress;
    manager->metadata[*outAddress].size = size;
    manager->metadata[*outAddress].inUse = 1;
    manager->usedSwapSize += size;
    manager->swapOperations++;
    return 0;
}

int swapManagerRead(SwapManager* manager, int swapAddress, void* outBuffer, int bufferSize) {
    if (!manager || swapAddress < 0 || !outBuffer) return -1;
    if (swapAddress >= manager->totalSwapSize) return -1;
    if (!manager->metadata[swapAddress].inUse) return -1;
    int size = manager->metadata[swapAddress].size;
    if (bufferSize < size) return -1;
    // Simulación: no hay datos reales
    manager->swapOperations++;
    return size;
}

int swapManagerFree(SwapManager* manager, int swapAddress) {
    if (!manager || swapAddress < 0) return -1;
    if (!manager->metadata[swapAddress].inUse) return -1;
    manager->metadata[swapAddress].inUse = 0;
    manager->usedSwapSize -= manager->metadata[swapAddress].size;
    manager->metadata[swapAddress].size = 0;
    return 0;
}
int swapManagerGetAvailableSwap(SwapManager* manager) { return manager ? manager->totalSwapSize - manager->usedSwapSize : 0; }
int swapManagerGetSwapOperationCount(SwapManager* manager) { return manager ? manager->swapOperations : 0; }
