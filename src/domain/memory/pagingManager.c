#include "pagingManager.h"
#include "bitmapManager.h"
#include "memoria.h"
#include "fifoReplacement.h"
#include "swapManager.h"
#include <stdlib.h>
#include <string.h>

#define TOTAL_PAGES (procesosEnEjecucion * maxPaginasPorProceso)

static int findProcessIndexByFrame(PagingManager* manager, int frame) {
    if (!manager || !manager->pageDirectory) return -1;
    for (int i = 0; i < manager->pageDirectory->totalPages; i++) {
        if (manager->pageDirectory->allPages[i].idMarco == frame)
            return manager->pageDirectory->allPages[i].idProceso;
    }
    return -1;
}

PagingManager* pagingManagerCreate(int totalPages, BitmapManager* bitmapManager) {
    (void)totalPages;
    PagingManager* pm = (PagingManager*)calloc(1, sizeof(PagingManager));
    if (!pm) return NULL;
    pm->pageDirectory = pageDirectoryCreate(TOTAL_PAGES);
    if (!pm->pageDirectory) { free(pm); return NULL; }
    pm->bitmapManager = bitmapManager;
    pm->totalPageFaults = 0;
    pm->internalWaste = 0;
    pm->externalWaste = 0;
    pm->fifoReplacement = NULL;
    memset(pm->frameCountPerProcess, 0, sizeof(pm->frameCountPerProcess));
    return pm;
}

void pagingManagerDestroy(PagingManager* manager) {
    if (manager) {
        pageDirectoryDestroy(manager->pageDirectory);
        free(manager);
    }
}

void pagingManagerSetBitmapManager(PagingManager* manager, BitmapManager* bitmapManager) { if (manager) manager->bitmapManager = bitmapManager; }

int pagingManagerHandlePageFault(PagingManager* manager, int processIndex, int pageNumber, const char* missingWord) {
    if (!manager) return -1;
    manager->totalPageFaults++;
    int victimPage = -1;
    if (manager->fifoReplacement) {
        victimPage = fifoReplacementSelectVictim(manager->fifoReplacement);
        if (victimPage >= 0) {
            int frame = pageDirectoryGetPageFrame(manager->pageDirectory, victimPage);
            if (frame >= 0) {
                int victimProcess = findProcessIndexByFrame(manager, frame);
                if (victimProcess >= 0 && manager->bitmapManager)
                    bitmapManagerFree(manager->bitmapManager, victimProcess);
                pageDirectorySetPageFrame(manager->pageDirectory, victimPage, -1);
            }
        }
    }
    int newFrame = bitmapManagerAllocate(manager->bitmapManager, processIndex, 1);
    if (newFrame < 0) return -1;
    pageDirectorySetPageFrame(manager->pageDirectory, pageNumber, newFrame);
    manager->pageDirectory->allPages[pageNumber].idProceso = processIndex;
    if (manager->fifoReplacement) fifoReplacementAddPage(manager->fifoReplacement, pageNumber);
    (void)missingWord;
    return 0;
}

int pagingManagerAllocatePageForProcess(PagingManager* manager, int processIndex, int pageCount) {
    if (!manager || processIndex < 0) return -1;
    ProcessPageTable* pt = pageDirectoryGetProcessTable(manager->pageDirectory, processIndex);
    if (!pt) return -1;
    if (pt->pages) free(pt->pages);
    pt->pages = (Pagina**)calloc(pageCount, sizeof(Pagina*));
    if (!pt->pages) return -1;
    pt->pageCount = pageCount;
    for (int i = 0; i < pageCount; i++) {
        int pageId = processIndex * maxPaginasPorProceso + i;
        if (pageId >= manager->pageDirectory->totalPages) continue;
        pt->pages[i] = &manager->pageDirectory->allPages[pageId];
        pt->pages[i]->idProceso = processIndex;
        pt->pages[i]->id = pageId;
    }
    manager->frameCountPerProcess[processIndex] = pageCount;
    return 0;
}

int pagingManagerDeallocatePagesForProcess(PagingManager* manager, int processIndex) {
    if (!manager || processIndex < 0) return -1;
    ProcessPageTable* pt = pageDirectoryGetProcessTable(manager->pageDirectory, processIndex);
    if (!pt) return -1;
    for (int i = 0; i < pt->pageCount; i++) {
        if (pt->pages[i] && pt->pages[i]->enMemoria && manager->bitmapManager) {
            bitmapManagerFree(manager->bitmapManager, processIndex);
        }
    }
    free(pt->pages);
    pt->pages = NULL;
    pt->pageCount = 0;
    manager->frameCountPerProcess[processIndex] = 0;
    return 0;
}

int pagingManagerGetPageFaultCount(PagingManager* manager) { return manager ? manager->totalPageFaults : 0; }
int pagingManagerGetInternalWaste(PagingManager* manager) { return manager ? manager->internalWaste : 0; }
int pagingManagerGetExternalWaste(PagingManager* manager) { return manager ? manager->externalWaste : 0; }
void pagingManagerSetReplacement(PagingManager* manager, FifoReplacement* fifo) { if (manager) manager->fifoReplacement = fifo; }
int pagingManagerResizeFrames(PagingManager* manager, int processIndex, int newFrameCount) {
    if (!manager || processIndex < 0) return -1;
    pagingManagerDeallocatePagesForProcess(manager, processIndex);
    return pagingManagerAllocatePageForProcess(manager, processIndex, newFrameCount);
}