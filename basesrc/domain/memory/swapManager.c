#include "swapManager.h"
#include <stdlib.h>
#include <string.h>

typedef struct SwapMetadata {
    int address;
    int size;
    int inUse;
    void* data;
} SwapMetadata;

SwapManager* swapManagerCreate(int swapSize) {
    SwapManager* sm = (SwapManager*)calloc(1, sizeof(SwapManager));
    if (!sm) return NULL;
    sm->totalSwapSize = swapSize;
    sm->usedSwapSize = 0;
    sm->swapOperations = 0;
    sm->metadata = (SwapMetadata*)calloc(swapSize, sizeof(SwapMetadata));
    if (!sm->metadata) { free(sm); return NULL; }
    for (int i = 0; i < swapSize; i++)
        sm->metadata[i].address = i;
    return sm;
}

void swapManagerDestroy(SwapManager* manager) {
    if (manager) {
        if (manager->metadata) {
            for (int i = 0; i < manager->totalSwapSize; i++)
                free(manager->metadata[i].data);
        }
        free(manager->metadata);
        free(manager);
    }
}

int swapManagerWrite(SwapManager* manager, const void* buffer, int size, int* outAddress) {
    if (!manager || !buffer || size <= 0 || !outAddress) return -1;
    if (manager->usedSwapSize >= manager->totalSwapSize) return -1;
    int address = -1;
    for (int i = 0; i < manager->totalSwapSize; i++) {
        if (!manager->metadata[i].inUse) {
            address = i;
            break;
        }
    }
    if (address < 0) return -1;
    void* copy = malloc((size_t)size);
    if (!copy) return -1;
    memcpy(copy, buffer, (size_t)size);
    *outAddress = address;
    free(manager->metadata[address].data);
    manager->metadata[address].data = copy;
    manager->metadata[address].address = address;
    manager->metadata[address].size = size;
    manager->metadata[address].inUse = 1;
    manager->usedSwapSize++;
    manager->swapOperations++;
    return 0;
}

int swapManagerRead(SwapManager* manager, int swapAddress, void* outBuffer, int bufferSize) {
    if (!manager || swapAddress < 0 || !outBuffer) return -1;
    if (swapAddress >= manager->totalSwapSize) return -1;
    if (!manager->metadata[swapAddress].inUse) return -1;
    int size = manager->metadata[swapAddress].size;
    if (bufferSize < size) return -1;
    if (!manager->metadata[swapAddress].data) return -1;
    memcpy(outBuffer, manager->metadata[swapAddress].data, (size_t)size);
    manager->swapOperations++;
    return size;
}

int swapManagerFree(SwapManager* manager, int swapAddress) {
    if (!manager || swapAddress < 0) return -1;
    if (swapAddress >= manager->totalSwapSize) return -1;
    if (!manager->metadata[swapAddress].inUse) return -1;
    manager->metadata[swapAddress].inUse = 0;
    manager->usedSwapSize--;
    free(manager->metadata[swapAddress].data);
    manager->metadata[swapAddress].data = NULL;
    manager->metadata[swapAddress].size = 0;
    return 0;
}
int swapManagerGetAvailableSwap(SwapManager* manager) { return manager ? manager->totalSwapSize - manager->usedSwapSize : 0; }
int swapManagerGetSwapOperationCount(SwapManager* manager) { return manager ? manager->swapOperations : 0; }
