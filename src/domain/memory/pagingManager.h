#ifndef PAGING_MANAGER_H
#define PAGING_MANAGER_H

#include "../../utils/constants.h"

struct ProcessTable;
struct PageDirectory;
struct FifoReplacement;
struct BitmapManager;

typedef struct PagingManager {
    struct PageDirectory* pageDirectory;
    struct BitmapManager* bitmapManager;
    int totalPageFaults;
    int internalWaste;
    int externalWaste;
    struct FifoReplacement* fifoReplacement;
    int frameCountPerProcess[procesosEnEjecucion];
} PagingManager;

PagingManager* pagingManagerCreate(int totalPages, struct BitmapManager* bitmapManager);
void pagingManagerDestroy(PagingManager* manager);
void pagingManagerSetBitmapManager(PagingManager* manager, struct BitmapManager* bitmapManager);
int pagingManagerHandlePageFault(PagingManager* manager, int processIndex, int pageNumber, const char* missingWord);
int pagingManagerAllocatePageForProcess(PagingManager* manager, int processIndex, int pageCount);
int pagingManagerDeallocatePagesForProcess(PagingManager* manager, int processIndex);
int pagingManagerGetPageFaultCount(PagingManager* manager);
int pagingManagerGetInternalWaste(PagingManager* manager);
int pagingManagerGetExternalWaste(PagingManager* manager);
float pagingManagerGetFragmentation(PagingManager* manager);
void pagingManagerSetReplacement(PagingManager* manager, struct FifoReplacement* fifo);
int pagingManagerResizeFrames(PagingManager* manager, int processIndex, int newFrameCount);

#endif
