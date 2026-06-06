// === src/domain/memory/swapManager.h ===

#ifndef SWAP_MANAGER_H
#define SWAP_MANAGER_H

#include "../../utils/constants.h"

typedef struct SwapMetadata SwapMetadata;

// Propietario exclusivo de SwapMetadata
typedef struct SwapManager {
    int totalSwapSize;
    int usedSwapSize;
    SwapMetadata* metadata;
    int swapOperations;
} SwapManager;

SwapManager* swapManagerCreate(int swapSize);

void swapManagerDestroy(SwapManager* manager);

int swapManagerWrite(SwapManager* manager, const void* buffer, int size, int* outAddress);

int swapManagerRead(SwapManager* manager, int swapAddress, void* outBuffer, int bufferSize);

int swapManagerFree(SwapManager* manager, int swapAddress);

int swapManagerGetAvailableSwap(SwapManager* manager);

int swapManagerGetSwapOperationCount(SwapManager* manager);

#endif
