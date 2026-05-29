#ifndef BITMAP_MANAGER_H
#define BITMAP_MANAGER_H

#include "../../utils/constants.h"

typedef struct BitmapManager {
    unsigned char* bitmap;
    int totalBlocks;
    int blockSize;
    int usedBlocks;
    int internalWaste;
    int externalWaste;
    int totalAllocations;
    int totalDeallocations;
    int processBlocks[procesosEnEjecucion];
    int processStartBlock[procesosEnEjecucion];
} BitmapManager;

BitmapManager* bitmapManagerCreate(int blockSize);
void bitmapManagerDestroy(BitmapManager* manager);
int bitmapManagerAllocate(BitmapManager* manager, int processIndex, int blocksNeeded);
void bitmapManagerFree(BitmapManager* manager, int processIndex);
int bitmapManagerGetUsedBlocks(BitmapManager* manager);
int bitmapManagerGetFreeBlocks(BitmapManager* manager);
int bitmapManagerGetInternalWaste(BitmapManager* manager);
int bitmapManagerGetExternalWaste(BitmapManager* manager);
void bitmapManagerPrint(BitmapManager* manager);

#endif