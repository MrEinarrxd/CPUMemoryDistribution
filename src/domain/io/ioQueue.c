#include "ioQueue.h"
#include "../process/process.h"
#include <stdlib.h>
#include <string.h>

IoQueue* ioQueueCreate(const int multipliers[numColasEs]) {
    IoQueue* q = (IoQueue*)calloc(1, sizeof(IoQueue));
    if (!q) return NULL;
    for (int i = 0; i < numColasEs; i++) {
        q->devices[i].head = 0;
        q->devices[i].tail = 0;
        q->devices[i].size = 0;
        q->devices[i].capacity = tamColaListos;
        q->devices[i].multiplier = multipliers[i];
        memset(q->devices[i].items, 0, sizeof(q->devices[i].items));
    }
    return q;
}
void ioQueueDestroy(IoQueue* queue) { free(queue); }

int ioQueueSend(IoQueue* queue, Process* process, int deviceId) {
    if (!queue || !process || deviceId < 0 || deviceId >= numColasEs) return -1;
    IoSubQueue* dev = &queue->devices[deviceId];
    if (dev->size >= dev->capacity) return -1;
    dev->items[dev->tail] = process;
    dev->tail = (dev->tail + 1) % dev->capacity;
    dev->size++;
    return 0;
}

Process* ioQueueReceive(IoQueue* queue, int deviceId) {
    if (!queue || deviceId < 0 || deviceId >= numColasEs) return NULL;
    IoSubQueue* dev = &queue->devices[deviceId];
    if (dev->size == 0) return NULL;
    Process* p = dev->items[dev->head];
    dev->head = (dev->head + 1) % dev->capacity;
    dev->size--;
    return p;
}

void ioQueueTick(IoQueue* queue) {
    if (!queue) return;
    for (int d = 0; d < numColasEs; d++) {
        IoSubQueue* dev = &queue->devices[d];
        for (int i = 0; i < dev->size; i++) {
            int idx = (dev->head + i) % dev->capacity;
            Process* p = dev->items[idx];
            if (p && p->bcp && p->bcp->ioTimeRemaining > 0)
                p->bcp->ioTimeRemaining--;
        }
    }
}

Process* ioQueueGetFinished(IoQueue* queue, int deviceId) {
    if (!queue || deviceId < 0 || deviceId >= numColasEs) return NULL;
    IoSubQueue* dev = &queue->devices[deviceId];
    for (int i = 0; i < dev->size; i++) {
        int idx = (dev->head + i) % dev->capacity;
        Process* p = dev->items[idx];
        if (p && p->bcp && p->bcp->ioTimeRemaining == 0) {
            int lastIdx = (dev->head + dev->size - 1) % dev->capacity;
            dev->items[idx] = dev->items[lastIdx];
            dev->items[lastIdx] = NULL;
            dev->size--;
            if (lastIdx == dev->tail)
                dev->tail = (dev->tail - 1 + dev->capacity) % dev->capacity;
            return p;
        }
    }
    return NULL;
}

int ioQueueHasFinished(IoQueue* queue, int deviceId) {
    if (!queue || deviceId < 0 || deviceId >= numColasEs) return 0;
    IoSubQueue* dev = &queue->devices[deviceId];
    for (int i = 0; i < dev->size; i++) {
        int idx = (dev->head + i) % dev->capacity;
        Process* p = dev->items[idx];
        if (p && p->bcp && p->bcp->ioTimeRemaining == 0) return 1;
    }
    return 0;
}