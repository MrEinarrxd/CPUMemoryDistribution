#include "bitmapManager.h"
#include <stdlib.h>
#include <string.h>

BitmapManager* bitmapManagerCreate(int blockSize) {
    BitmapManager* bm = (BitmapManager*)calloc(1, sizeof(BitmapManager));
    if (!bm) return NULL;
    bm->totalBlocks = maxMarcos;
    bm->blockSize = blockSize;
    int bytes = (bm->totalBlocks + 7) / 8;
    bm->bitmap = (unsigned char*)calloc(bytes, 1);
    if (!bm->bitmap) { free(bm); return NULL; }
    bm->usedBlocks = 0;
    bm->internalWaste = 0;
    bm->externalWaste = 0;
    bm->fragmentation = 0.0f;
    bm->largestFreeRun = maxMarcos;
    bm->freeRunCount = 1;
    bm->totalAllocations = 0;
    bm->totalDeallocations = 0;
    memset(bm->processBlocks, 0, sizeof(bm->processBlocks));
    memset(bm->processStartBlock, 0, sizeof(bm->processStartBlock));
    for (int i = 0; i < maxMarcos; i++) {
        bm->frameProcess[i] = -1;
        bm->framePage[i] = -1;
    }
    bitmapManagerUpdateMetrics(bm);
    return bm;
}

void bitmapManagerDestroy(BitmapManager* manager) {
    if (manager) { free(manager->bitmap); free(manager); }
}

static void setBit(unsigned char* bitmap, int bit, int val) {
    int byte = bit / 8;
    int mask = 1 << (7 - (bit % 8));
    if (val) bitmap[byte] |= mask;
    else bitmap[byte] &= ~mask;
}
static int getBit(unsigned char* bitmap, int bit) {
    int byte = bit / 8;
    int mask = 1 << (7 - (bit % 8));
    return (bitmap[byte] & mask) ? 1 : 0;
}

int bitmapManagerAllocate(BitmapManager* manager, int processIndex, int blocksNeeded) {
    if (!manager || processIndex < 0 || processIndex >= procesosEnEjecucion) return -1;
    if (blocksNeeded <= 0) return -1;
    int cont = 0, start = -1;
    for (int i = 0; i < manager->totalBlocks; i++) {
        if (getBit(manager->bitmap, i) == 0) {
            if (cont == 0) start = i;
            cont++;
            if (cont >= blocksNeeded) break;
        } else { cont = 0; start = -1; }
    }
    if (cont < blocksNeeded) return -1;
    for (int i = start; i < start + blocksNeeded; i++)
        setBit(manager->bitmap, i, 1);
    for (int i = start; i < start + blocksNeeded; i++) {
        manager->frameProcess[i] = processIndex;
        manager->framePage[i] = -1;
    }
    manager->usedBlocks += blocksNeeded;
    manager->totalAllocations++;
    manager->processBlocks[processIndex] += blocksNeeded;
    if (manager->processStartBlock[processIndex] == 0 && manager->processBlocks[processIndex] == blocksNeeded)
        manager->processStartBlock[processIndex] = start;
    bitmapManagerUpdateMetrics(manager);
    return start;
}

void bitmapManagerFree(BitmapManager* manager, int processIndex) {
    if (!manager || processIndex < 0 || processIndex >= procesosEnEjecucion) return;
    int freed = 0;
    for (int i = 0; i < manager->totalBlocks; i++) {
        if (manager->frameProcess[i] == processIndex) {
            setBit(manager->bitmap, i, 0);
            manager->frameProcess[i] = -1;
            manager->framePage[i] = -1;
            freed++;
        }
    }
    if (freed <= 0) return;
    manager->usedBlocks -= freed;
    manager->totalDeallocations += freed;
    manager->processBlocks[processIndex] = 0;
    manager->processStartBlock[processIndex] = 0;
    bitmapManagerUpdateMetrics(manager);
}

void bitmapManagerFreeFrame(BitmapManager* manager, int frameIndex) {
    if (!manager || frameIndex < 0 || frameIndex >= manager->totalBlocks) return;
    if (getBit(manager->bitmap, frameIndex) == 0) return;
    int processIndex = manager->frameProcess[frameIndex];
    setBit(manager->bitmap, frameIndex, 0);
    manager->frameProcess[frameIndex] = -1;
    manager->framePage[frameIndex] = -1;
    manager->usedBlocks--;
    manager->totalDeallocations++;
    if (processIndex >= 0 && processIndex < procesosEnEjecucion && manager->processBlocks[processIndex] > 0)
        manager->processBlocks[processIndex]--;
    bitmapManagerUpdateMetrics(manager);
}

void bitmapManagerSetFramePage(BitmapManager* manager, int frameIndex, int pageId) {
    if (!manager || frameIndex < 0 || frameIndex >= manager->totalBlocks) return;
    manager->framePage[frameIndex] = pageId;
}

void bitmapManagerUpdateMetrics(BitmapManager* manager) {
    if (!manager) return;
    int freeBlocks = 0;
    int largestRun = 0;
    int currentRun = 0;
    int runCount = 0;
    int inRun = 0;

    for (int i = 0; i < manager->totalBlocks; i++) {
        if (getBit(manager->bitmap, i) == 0) {
            freeBlocks++;
            currentRun++;
            if (!inRun) {
                runCount++;
                inRun = 1;
            }
            if (currentRun > largestRun) largestRun = currentRun;
        } else {
            currentRun = 0;
            inRun = 0;
        }
    }

    manager->internalWaste = 0;
    manager->largestFreeRun = largestRun;
    manager->freeRunCount = runCount;
    manager->externalWaste = freeBlocks > largestRun ? freeBlocks - largestRun : 0;
    manager->fragmentation = freeBlocks > 0
        ? (float)manager->externalWaste / (float)freeBlocks
        : 0.0f;
}

int bitmapManagerGetUsedBlocks(BitmapManager* manager) { return manager ? manager->usedBlocks : 0; }
int bitmapManagerGetFreeBlocks(BitmapManager* manager) { return manager ? manager->totalBlocks - manager->usedBlocks : 0; }
int bitmapManagerGetInternalWaste(BitmapManager* manager) { if (manager) bitmapManagerUpdateMetrics(manager); return manager ? manager->internalWaste : 0; }
int bitmapManagerGetExternalWaste(BitmapManager* manager) { if (manager) bitmapManagerUpdateMetrics(manager); return manager ? manager->externalWaste : 0; }
float bitmapManagerGetFragmentation(BitmapManager* manager) { if (manager) bitmapManagerUpdateMetrics(manager); return manager ? manager->fragmentation : 0.0f; }
int bitmapManagerGetLargestFreeRun(BitmapManager* manager) { if (manager) bitmapManagerUpdateMetrics(manager); return manager ? manager->largestFreeRun : 0; }
int bitmapManagerGetFreeRunCount(BitmapManager* manager) { if (manager) bitmapManagerUpdateMetrics(manager); return manager ? manager->freeRunCount : 0; }
void bitmapManagerPrint(BitmapManager* manager) { (void)manager; }
