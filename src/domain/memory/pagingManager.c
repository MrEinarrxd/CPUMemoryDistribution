#include "pagingManager.h"
#include "bitmapManager.h"
#include "memoria.h"
#include "fifoReplacement.h"
#include "swapManager.h"
#include <stdlib.h>
#include <string.h>

#define TOTAL_PAGES (procesosEnEjecucion * maxPaginasPorProceso)

// Función corregida: busca el índice en runningProcesses (0..procesosEnEjecucion-1)
static int findProcessIndexByFrame(PagingManager* manager, int frame) {
    if (!manager || !manager->pageDirectory) return -1;
    for (int procIdx = 0; procIdx < procesosEnEjecucion; procIdx++) {
        ProcessPageTable* pt = &manager->pageDirectory->processTables[procIdx];
        for (int i = 0; i < pt->pageCount; i++) {
            if (pt->pages[i] && pt->pages[i]->idMarco == frame)
                return procIdx;
        }
    }
    return -1;
}

static void clearPage(Pagina* page) {
    if (!page) return;
    page->enMemoria = 0;
    page->idMarco = -1;
    page->idProceso = -1;
    memset(page->words, 0, sizeof(page->words));
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

    // INICIALIZAR el reemplazo FIFO (evita page faults sin víctima)
    pm->fifoReplacement = fifoReplacementCreate(NULL, maxMarcos);
    if (!pm->fifoReplacement) {
        pageDirectoryDestroy(pm->pageDirectory);
        free(pm);
        return NULL;
    }

    memset(pm->frameCountPerProcess, 0, sizeof(pm->frameCountPerProcess));
    return pm;
}

void pagingManagerDestroy(PagingManager* manager) {
    if (manager) {
        if (manager->fifoReplacement)
            fifoReplacementDestroy(manager->fifoReplacement);
        pageDirectoryDestroy(manager->pageDirectory);
        free(manager);
    }
}

void pagingManagerSetBitmapManager(PagingManager* manager, BitmapManager* bitmapManager) { if (manager) manager->bitmapManager = bitmapManager; }

int pagingManagerHandlePageFault(PagingManager* manager, int processIndex, int pageNumber, const char* missingWord) {
    if (!manager || !manager->pageDirectory || !manager->bitmapManager) return -1;
    if (processIndex < 0 || processIndex >= procesosEnEjecucion) return -1;
    if (pageNumber < 0 || pageNumber >= manager->pageDirectory->totalPages) return -1;
    if (manager->pageDirectory->allPages[pageNumber].enMemoria &&
        manager->pageDirectory->allPages[pageNumber].idMarco >= 0)
        return 0;

    manager->totalPageFaults++;
    int newFrame = bitmapManagerAllocate(manager->bitmapManager, processIndex, 1);
    if (newFrame < 0 && manager->fifoReplacement) {
        int victimPage = fifoReplacementSelectVictim(manager->fifoReplacement);
        while (victimPage >= 0) {
            int frame = pageDirectoryGetPageFrame(manager->pageDirectory, victimPage);
            if (frame >= 0) {
                int victimProcess = findProcessIndexByFrame(manager, frame);
                bitmapManagerFreeFrame(manager->bitmapManager, frame);
                if (victimProcess >= 0 && victimProcess < procesosEnEjecucion &&
                    manager->frameCountPerProcess[victimProcess] > 0)
                    manager->frameCountPerProcess[victimProcess]--;
                pageDirectorySetPageFrame(manager->pageDirectory, victimPage, -1);
                newFrame = bitmapManagerAllocate(manager->bitmapManager, processIndex, 1);
                break;
            }
            victimPage = fifoReplacementSelectVictim(manager->fifoReplacement);
        }
    }
    if (newFrame < 0) return -1;
    pageDirectorySetPageFrame(manager->pageDirectory, pageNumber, newFrame);
    manager->pageDirectory->allPages[pageNumber].idProceso = processIndex;
    bitmapManagerSetFramePage(manager->bitmapManager, newFrame, pageNumber);
    manager->frameCountPerProcess[processIndex]++;
    if (manager->fifoReplacement) fifoReplacementAddPage(manager->fifoReplacement, pageNumber);
    (void)missingWord;
    return 0;
}

int pagingManagerAllocatePageForProcess(PagingManager* manager, int processIndex, int pageCount) {
    if (!manager || processIndex < 0 || processIndex >= procesosEnEjecucion) return -1;
    if (pageCount < 1) pageCount = 1;
    if (pageCount > maxPaginasPorProceso) pageCount = maxPaginasPorProceso;
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
        pt->pages[i]->idMarco = -1;
        pt->pages[i]->enMemoria = 0;
        memset(pt->pages[i]->words, 0, sizeof(pt->pages[i]->words));
    }
    manager->frameCountPerProcess[processIndex] = 0;
    return 0;
}

int pagingManagerDeallocatePagesForProcess(PagingManager* manager, int processIndex) {
    if (!manager || processIndex < 0 || processIndex >= procesosEnEjecucion) return -1;
    ProcessPageTable* pt = pageDirectoryGetProcessTable(manager->pageDirectory, processIndex);
    if (!pt) return -1;
    for (int i = 0; i < pt->pageCount; i++) {
        Pagina* page = pt->pages ? pt->pages[i] : NULL;
        if (!page) continue;
        if (page->idMarco >= 0 && manager->bitmapManager)
            bitmapManagerFreeFrame(manager->bitmapManager, page->idMarco);
        if (manager->fifoReplacement)
            fifoReplacementRemovePage(manager->fifoReplacement, page->id);
        clearPage(page);
    }
    if (manager->bitmapManager) {
        bitmapManagerFree(manager->bitmapManager, processIndex);
    }
    free(pt->pages);
    pt->pages = NULL;
    pt->pageCount = 0;
    manager->frameCountPerProcess[processIndex] = 0;
    return 0;
}

int pagingManagerGetPageFaultCount(PagingManager* manager) { return manager ? manager->totalPageFaults : 0; }
int pagingManagerGetInternalWaste(PagingManager* manager) {
    if (!manager || !manager->bitmapManager) return manager ? manager->internalWaste : 0;
    manager->internalWaste = bitmapManagerGetInternalWaste(manager->bitmapManager);
    return manager->internalWaste;
}
int pagingManagerGetExternalWaste(PagingManager* manager) {
    if (!manager || !manager->bitmapManager) return manager ? manager->externalWaste : 0;
    manager->externalWaste = bitmapManagerGetExternalWaste(manager->bitmapManager);
    return manager->externalWaste;
}
float pagingManagerGetFragmentation(PagingManager* manager) {
    if (!manager || !manager->bitmapManager) return 0.0f;
    return bitmapManagerGetFragmentation(manager->bitmapManager);
}
void pagingManagerSetReplacement(PagingManager* manager, FifoReplacement* fifo) { if (manager) manager->fifoReplacement = fifo; }
int pagingManagerResizeFrames(PagingManager* manager, int processIndex, int newFrameCount) {
    if (!manager || processIndex < 0 || processIndex >= procesosEnEjecucion) return -1;
    if (newFrameCount < 1) newFrameCount = 1;
    if (newFrameCount > maxPaginasPorProceso) newFrameCount = maxPaginasPorProceso;
    ProcessPageTable* pt = pageDirectoryGetProcessTable(manager->pageDirectory, processIndex);
    if (!pt) return -1;

    int oldCount = pt->pageCount;
    if (oldCount == newFrameCount) return 0;

    if (newFrameCount < oldCount && pt->pages) {
        for (int i = newFrameCount; i < oldCount; i++) {
            Pagina* page = pt->pages[i];
            if (!page) continue;
            if (page->idMarco >= 0 && manager->bitmapManager) {
                bitmapManagerFreeFrame(manager->bitmapManager, page->idMarco);
                if (manager->frameCountPerProcess[processIndex] > 0)
                    manager->frameCountPerProcess[processIndex]--;
            }
            if (manager->fifoReplacement)
                fifoReplacementRemovePage(manager->fifoReplacement, page->id);
            clearPage(page);
        }
    }

    Pagina** resized = (Pagina**)realloc(pt->pages, sizeof(Pagina*) * newFrameCount);
    if (!resized) return -1;
    pt->pages = resized;

    for (int i = oldCount; i < newFrameCount; i++) {
        int pageId = processIndex * maxPaginasPorProceso + i;
        if (pageId >= manager->pageDirectory->totalPages) {
            pt->pages[i] = NULL;
            continue;
        }
        pt->pages[i] = &manager->pageDirectory->allPages[pageId];
        pt->pages[i]->id = pageId;
        pt->pages[i]->idProceso = processIndex;
        pt->pages[i]->idMarco = -1;
        pt->pages[i]->enMemoria = 0;
        memset(pt->pages[i]->words, 0, sizeof(pt->pages[i]->words));
    }

    pt->pageCount = newFrameCount;
    return 0;
}
