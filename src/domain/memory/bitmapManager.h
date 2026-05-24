// === src/domain/memory/bitmapManager.h ===

#ifndef BITMAP_MANAGER_H
#define BITMAP_MANAGER_H

#include "../../utils/constants.h"

// Bitmap algorithm for memory WITHOUT swap
// Each bit represents one memory block: 0=free, 1=used
typedef struct BitmapManager {
    unsigned char* bitmap;                              // Bitmap array, size maxMarcos/8
    int totalBlocks;                                    // = maxMarcos
    int blockSize;                                      // Size of each block in bytes
    int usedBlocks;                                     // Number of blocks currently in use
    int internalWaste;                                  // Internal fragmentation
    int externalWaste;                                  // External fragmentation
    int totalAllocations;                               // Counter of allocation operations
    int totalDeallocations;                             // Counter of deallocation operations
    int processBlocks[procesosEnEjecucion];             // Blocks allocated per process
} BitmapManager;

// Create bitmap manager with specified block size
BitmapManager* bitmapManagerCreate(int blockSize);

// Destroy and free bitmap manager resources
void bitmapManagerDestroy(BitmapManager* manager);

// Allocate blocks for process, returns starting block index or -1 on failure
int bitmapManagerAllocate(BitmapManager* manager, int processIndex, int blocksNeeded);

// Free blocks for process
void bitmapManagerFree(BitmapManager* manager, int processIndex);

// Get current number of used blocks
int bitmapManagerGetUsedBlocks(BitmapManager* manager);

// Get current number of free blocks
int bitmapManagerGetFreeBlocks(BitmapManager* manager);

// Get internal fragmentation value
int bitmapManagerGetInternalWaste(BitmapManager* manager);

// Get external fragmentation value
int bitmapManagerGetExternalWaste(BitmapManager* manager);

// Print current state of bitmap manager
void bitmapManagerPrint(BitmapManager* manager);

#endif
