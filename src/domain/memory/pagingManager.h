// === src/domain/memory/pagingManager.h ===

#ifndef PAGING_MANAGER_H
#define PAGING_MANAGER_H

#include "../../utils/constants.h"

struct ProcessTable;
struct PageDirectory;
struct FifoReplacement;

// Propietario exclusivo de PageDirectory
typedef struct PagingManager {
    struct PageDirectory* pageDirectory;
    int totalPageFaults;
    int internalWaste;
    int externalWaste;
    struct FifoReplacement* fifoReplacement;            // FIFO replacement handler for swap
    int frameCountPerProcess[procesosEnEjecucion];      // Frame count per process for resizing
} PagingManager;

PagingManager* pagingManagerCreate(int totalPages);

void pagingManagerDestroy(PagingManager* manager);

int pagingManagerHandlePageFault(PagingManager* manager, int pageNumber);

int pagingManagerAllocatePageForProcess(PagingManager* manager, int processIndex, int pageCount);

int pagingManagerDeallocatePagesForProcess(PagingManager* manager, int processIndex);

int pagingManagerGetPageFaultCount(PagingManager* manager);

int pagingManagerGetInternalWaste(PagingManager* manager);

int pagingManagerGetExternalWaste(PagingManager* manager);

// Set FIFO replacement handler for swap operations
void pagingManagerSetReplacement(PagingManager* manager, struct FifoReplacement* fifo);

// Resize frame allocation for process
int pagingManagerResizeFrames(PagingManager* manager, int processIndex, int newFrameCount);

#endif
