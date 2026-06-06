#include "memoryResize.h"
#include "pagingManager.h"
#include "../core/processTable.h"
#include <stdlib.h>

MemoryResizer* memoryResizerCreate(void) {
    MemoryResizer* mr = (MemoryResizer*)calloc(1, sizeof(MemoryResizer));
    return mr;
}
void memoryResizerDestroy(MemoryResizer* resizer) { free(resizer); }

static int normalizeEvenPageCount(int pageCount) {
    if (pageCount < marcosMin) pageCount = marcosMin;
    if (pageCount > marcosMax) pageCount = marcosMax;
    if ((pageCount % 2) != 0) pageCount++;
    if (pageCount > marcosMax) pageCount = marcosMax;
    return pageCount;
}

void memoryResizerExecute(MemoryResizer* resizer, ProcessTable* table, PagingManager* paging) {
    if (!resizer || !table || !paging) return;
    resizer->resizeAttempts++;
    int activeCount = 0;
    int activeIndices[procesosEnEjecucion];
    for (int i = 0; i < procesosEnEjecucion; i++) {
        Process* p = table->runningProcesses[i];
        if (p && p->bcp &&
            p->bcp->state != ProcessStateFinished &&
            p->bcp->state != ProcessStateWaitingIo) {
            activeIndices[activeCount++] = i;
        }
    }
    // Registrar procesos en newRequests para el conteo total (sin slot en pagingManager)
    int pendingCount = 0;
    for (int i = 0; i < procesosEnEspera; i++) {
        Process* p = table->newRequests[i];
        if (p && p->bcp && p->bcp->state != ProcessStateFinished) {
            pendingCount++;
        }
    }
    if (activeCount + pendingCount < 2) return;
    if (activeCount < 2) return;
    int half = activeCount / 2;
    for (int i = 0; i < half; i++) {
        int idx = activeIndices[i];
        Process* proc = table->runningProcesses[idx];
        int current = (proc && proc->bcp && proc->bcp->pageCount > 0) ? proc->bcp->pageCount : marcosMin;
        int newCount = normalizeEvenPageCount(current / 2);
        if (pagingManagerResizeFrames(paging, idx, newCount) == 0) {
            Process* p = table->runningProcesses[idx];
            if (p && p->bcp) p->bcp->pageCount = newCount;
            resizer->successfulResizes++;
        }
    }
    for (int i = half; i < activeCount; i++) {
        int idx = activeIndices[i];
        Process* proc = table->runningProcesses[idx];
        int current = (proc && proc->bcp && proc->bcp->pageCount > 0) ? proc->bcp->pageCount : marcosMin;
        int newCount = normalizeEvenPageCount(current * 2);
        if (pagingManagerResizeFrames(paging, idx, newCount) == 0) {
            Process* p = table->runningProcesses[idx];
            if (p && p->bcp) p->bcp->pageCount = newCount;
            resizer->successfulResizes++;
        }
    }
    resizer->lastResizeCycle = table->currentCycle;
}
int memoryResizerGetFrameCount(PagingManager* paging, int processIndex) { return paging ? paging->frameCountPerProcess[processIndex] : 0; }
int memoryResizerGetAttempts(MemoryResizer* resizer) { return resizer ? resizer->resizeAttempts : 0; }
int memoryResizerGetSuccessful(MemoryResizer* resizer) { return resizer ? resizer->successfulResizes : 0; }

//Parte 8 – Dominio: Estadísticas
