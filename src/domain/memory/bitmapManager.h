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
    float fragmentation;
    int largestFreeRun;
    int freeRunCount;
    int totalAllocations;
    int totalDeallocations;
    int processBlocks[procesosEnEjecucion];
    int processStartBlock[procesosEnEjecucion];
    int frameProcess[maxMarcos];
    int framePage[maxMarcos];
} BitmapManager;

BitmapManager* bitmapManagerCreate(int blockSize);
void bitmapManagerDestroy(BitmapManager* manager);
int bitmapManagerAllocate(BitmapManager* manager, int processIndex, int blocksNeeded);
void bitmapManagerFree(BitmapManager* manager, int processIndex);
void bitmapManagerFreeFrame(BitmapManager* manager, int frameIndex);
void bitmapManagerSetFramePage(BitmapManager* manager, int frameIndex, int pageId);
void bitmapManagerUpdateMetrics(BitmapManager* manager);
int bitmapManagerGetUsedBlocks(BitmapManager* manager);
int bitmapManagerGetFreeBlocks(BitmapManager* manager);
int bitmapManagerGetInternalWaste(BitmapManager* manager);
int bitmapManagerGetExternalWaste(BitmapManager* manager);
float bitmapManagerGetFragmentation(BitmapManager* manager);
int bitmapManagerGetLargestFreeRun(BitmapManager* manager);
int bitmapManagerGetFreeRunCount(BitmapManager* manager);
void bitmapManagerPrint(BitmapManager* manager);

#endif
