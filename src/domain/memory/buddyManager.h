// === src/domain/memory/buddyManager.h ===

#ifndef BUDDY_MANAGER_H
#define BUDDY_MANAGER_H

#include "../../utils/constants.h"

struct BuddyTree;

// Propietario exclusivo de BuddyTree
typedef struct BuddyManager {
    struct BuddyTree* buddyTree;
    int totalAllocations;
    int totalDeallocations;
    int externalFragmentation;
} BuddyManager;

BuddyManager* buddyManagerCreate(int memorySize);

void buddyManagerDestroy(BuddyManager* manager);

int buddyManagerAllocate(BuddyManager* manager, int size);

void buddyManagerFree(BuddyManager* manager, int address);

int buddyManagerGetAvailableMemory(BuddyManager* manager);

int buddyManagerGetFragmentation(BuddyManager* manager);

int buddyManagerGetAllocationCount(BuddyManager* manager);

#endif
