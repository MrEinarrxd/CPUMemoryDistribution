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
    bm->totalAllocations = 0;
    bm->totalDeallocations = 0;
    memset(bm->processBlocks, 0, sizeof(bm->processBlocks));
    memset(bm->processStartBlock, 0, sizeof(bm->processStartBlock));
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
    manager->usedBlocks += blocksNeeded;
    manager->totalAllocations++;
    manager->processBlocks[processIndex] = blocksNeeded;
    manager->processStartBlock[processIndex] = start;
    manager->internalWaste = 0;
    return start;
}

void bitmapManagerFree(BitmapManager* manager, int processIndex) {
    if (!manager || processIndex < 0 || processIndex >= procesosEnEjecucion) return;
    int blocks = manager->processBlocks[processIndex];
    int start = manager->processStartBlock[processIndex];
    if (blocks <= 0) return;
    for (int i = start; i < start + blocks; i++)
        setBit(manager->bitmap, i, 0);
    manager->usedBlocks -= blocks;
    manager->totalDeallocations++;
    manager->processBlocks[processIndex] = 0;
    manager->processStartBlock[processIndex] = 0;
}

int bitmapManagerGetUsedBlocks(BitmapManager* manager) { return manager ? manager->usedBlocks : 0; }
int bitmapManagerGetFreeBlocks(BitmapManager* manager) { return manager ? manager->totalBlocks - manager->usedBlocks : 0; }
int bitmapManagerGetInternalWaste(BitmapManager* manager) { return manager ? manager->internalWaste : 0; }
int bitmapManagerGetExternalWaste(BitmapManager* manager) { return manager ? manager->externalWaste : 0; }
void bitmapManagerPrint(BitmapManager* manager) { (void)manager; }