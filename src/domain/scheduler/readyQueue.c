#include "readyQueue.h"
#include "../process/process.h"
#include <stdlib.h>
#ifdef DEBUG_VERBOSE
#include <stdio.h>
static int readyQueueContains(ReadyQueue* queue, Process* process) {
    if (!queue || !process) return 0;
    for (int i = 0; i < queue->count; i++) {
        int idx = (queue->head + i) % tamColaListos;
        if (queue->processes[idx] == process) return 1;
    }
    return 0;
}
#endif

ReadyQueue* readyQueueCreate(void) {
    ReadyQueue* q = (ReadyQueue*)calloc(1, sizeof(ReadyQueue));
    if (q) { q->head = 0; q->tail = 0; q->count = 0; }
    return q;
}
void readyQueueDestroy(ReadyQueue* queue) { free(queue); }

int readyQueueEnqueue(ReadyQueue* queue, Process* process) {
    if (!queue || !process) return -1;
    if (queue->count >= tamColaListos) return -1;
#ifdef DEBUG_VERBOSE
    if (readyQueueContains(queue, process)) {
        fprintf(stderr, "[ReadyQueue] proceso duplicado ignorado: %p\n", (void*)process);
        return -1;
    }
#endif
    queue->processes[queue->tail] = process;
    queue->tail = (queue->tail + 1) % tamColaListos;
    queue->count++;
    return 0;
}

int readyQueueEnqueueFront(ReadyQueue* queue, Process* process) {
    if (!queue || !process) return -1;
    if (queue->count >= tamColaListos) return -1;
#ifdef DEBUG_VERBOSE
    if (readyQueueContains(queue, process)) {
        fprintf(stderr, "[ReadyQueue] proceso duplicado ignorado: %p\n", (void*)process);
        return -1;
    }
#endif
    queue->head = (queue->head - 1 + tamColaListos) % tamColaListos;
    queue->processes[queue->head] = process;
    queue->count++;
    return 0;
}

Process* readyQueueDequeue(ReadyQueue* queue) {
    if (!queue || queue->count == 0) return NULL;
    Process* p = queue->processes[queue->head];
    queue->head = (queue->head + 1) % tamColaListos;
    queue->count--;
    return p;
}

Process* readyQueuePeek(ReadyQueue* queue) {
    if (!queue || queue->count == 0) return NULL;
    return queue->processes[queue->head];
}

int readyQueueGetCount(ReadyQueue* queue) { return queue ? queue->count : 0; }
int readyQueueIsFull(ReadyQueue* queue) { return queue ? (queue->count >= tamColaListos) : 1; }
int readyQueueIsEmpty(ReadyQueue* queue) { return queue ? (queue->count == 0) : 1; }
void readyQueueClear(ReadyQueue* queue) { if (queue) { queue->head = 0; queue->tail = 0; queue->count = 0; } }
