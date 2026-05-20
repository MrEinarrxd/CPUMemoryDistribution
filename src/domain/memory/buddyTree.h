// === src/domain/memory/buddyTree.h ===

#ifndef BUDDY_TREE_H
#define BUDDY_TREE_H

#include "../../utils/constants.h"

// Propietario exclusivo de nodos del árbol
typedef struct BuddyNode {
    int offset;
    int size;
    int isFree;
    struct BuddyNode* left;
    struct BuddyNode* right;
    struct BuddyNode* parent;
} BuddyNode;

typedef struct BuddyTree {
    BuddyNode* root;
    int totalSize;
    int allocatedSize;
} BuddyTree;

BuddyTree* buddyTreeCreate(int totalSize);

void buddyTreeDestroy(BuddyTree* tree);

BuddyNode* buddyTreeAllocate(BuddyTree* tree, int size);

void buddyTreeFree(BuddyTree* tree, BuddyNode* node);

int buddyTreeGetFragmentation(BuddyTree* tree);

int buddyTreeGetAllocatedSize(BuddyTree* tree);

int buddyTreeGetFreeSize(BuddyTree* tree);

#endif
