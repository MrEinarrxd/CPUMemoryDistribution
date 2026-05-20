// === src/domain/memory/pagingManager.h ===

#ifndef PAGING_MANAGER_H
#define PAGING_MANAGER_H

#include "../../utils/constants.h"

struct ProcessTable;
struct PageDirectory;

// Propietario exclusivo de PageDirectory
typedef struct PagingManager {
    struct PageDirectory* pageDirectory;
    int totalPageFaults;
    int internalWaste;
    int externalWaste;
} PagingManager;

PagingManager* pagingManagerCreate(int totalPages);

void pagingManagerDestroy(PagingManager* manager);

int pagingManagerHandlePageFault(PagingManager* manager, int pageNumber);

int pagingManagerAllocatePageForProcess(PagingManager* manager, int processIndex, int pageCount);

int pagingManagerDeallocatePagesForProcess(PagingManager* manager, int processIndex);

int pagingManagerGetPageFaultCount(PagingManager* manager);

int pagingManagerGetInternalWaste(PagingManager* manager);

int pagingManagerGetExternalWaste(PagingManager* manager);

#endif
