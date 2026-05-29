#include "fifoReplacement.h"
#include <stdlib.h>

FifoReplacement* fifoReplacementCreate(Marco** frames, int totalFrames) {
    FifoReplacement* fifo = (FifoReplacement*)calloc(1, sizeof(FifoReplacement));
    if (!fifo) return NULL;
    fifo->frames = frames;
    fifo->totalFrames = totalFrames;
    fifo->fifoQueue.pageIds = (int*)calloc(totalFrames, sizeof(int));
    fifo->fifoQueue.capacity = totalFrames;
    fifo->fifoQueue.head = 0;
    fifo->fifoQueue.tail = 0;
    fifo->fifoQueue.count = 0;
    fifo->totalReplacements = 0;
    return fifo;
}
void fifoReplacementDestroy(FifoReplacement* fifo) {
    if (fifo) { free(fifo->fifoQueue.pageIds); free(fifo); }
}
int fifoReplacementSelectVictim(FifoReplacement* fifo) {
    if (!fifo || fifo->fifoQueue.count == 0) return -1;
    int victimPage = fifo->fifoQueue.pageIds[fifo->fifoQueue.head];
    fifo->fifoQueue.head = (fifo->fifoQueue.head + 1) % fifo->fifoQueue.capacity;
    fifo->fifoQueue.count--;
    fifo->totalReplacements++;
    return victimPage;
}
void fifoReplacementAddPage(FifoReplacement* fifo, int pageId) {
    if (!fifo || fifo->fifoQueue.count >= fifo->fifoQueue.capacity) return;
    for (int i = 0; i < fifo->fifoQueue.count; i++) {
        int idx = (fifo->fifoQueue.head + i) % fifo->fifoQueue.capacity;
        if (fifo->fifoQueue.pageIds[idx] == pageId) return;
    }
    fifo->fifoQueue.pageIds[fifo->fifoQueue.tail] = pageId;
    fifo->fifoQueue.tail = (fifo->fifoQueue.tail + 1) % fifo->fifoQueue.capacity;
    fifo->fifoQueue.count++;
}
void fifoReplacementRemovePage(FifoReplacement* fifo, int pageId) {
    if (!fifo || fifo->fifoQueue.count == 0) return;
    for (int i = 0; i < fifo->fifoQueue.count; i++) {
        int idx = (fifo->fifoQueue.head + i) % fifo->fifoQueue.capacity;
        if (fifo->fifoQueue.pageIds[idx] != pageId) continue;
        for (int j = i; j < fifo->fifoQueue.count - 1; j++) {
            int cur = (fifo->fifoQueue.head + j) % fifo->fifoQueue.capacity;
            int next = (fifo->fifoQueue.head + j + 1) % fifo->fifoQueue.capacity;
            fifo->fifoQueue.pageIds[cur] = fifo->fifoQueue.pageIds[next];
        }
        fifo->fifoQueue.tail = (fifo->fifoQueue.tail - 1 + fifo->fifoQueue.capacity) % fifo->fifoQueue.capacity;
        fifo->fifoQueue.pageIds[fifo->fifoQueue.tail] = 0;
        fifo->fifoQueue.count--;
        return;
    }
}
int fifoReplacementGetTotalReplacements(FifoReplacement* fifo) { return fifo ? fifo->totalReplacements : 0; }
