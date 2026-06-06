#include "readyQueue.h"

static int readyQueueContains(const ReadyQueue* queue, int processIndex) {
    if (!queue) return 0;
    for (int i = 0; i < queue->count; ++i) {
        int pos = (queue->head + i) % ReadyQueueCapacity;
        if (queue->items[pos] == processIndex) return 1;
    }
    return 0;
}

void readyQueueInit(ReadyQueue* queue) {
    if (!queue) return;
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
}

int readyQueuePush(ReadyQueue* queue, int processIndex) {
    if (!queue) return -1;
    if (readyQueueContains(queue, processIndex)) return 0;
    if (queue->count >= ReadyQueueCapacity) return -1;
    queue->items[queue->tail] = processIndex;
    queue->tail = (queue->tail + 1) % ReadyQueueCapacity;
    queue->count++;
    return 0;
}

int readyQueuePushFront(ReadyQueue* queue, int processIndex) {
    if (!queue) return -1;
    if (readyQueueContains(queue, processIndex)) return 0;
    if (queue->count >= ReadyQueueCapacity) return -1;
    queue->head = (queue->head - 1 + ReadyQueueCapacity) % ReadyQueueCapacity;
    queue->items[queue->head] = processIndex;
    queue->count++;
    return 0;
}

int readyQueuePop(ReadyQueue* queue) {
    int processIndex;
    if (!queue || queue->count <= 0) return -1;
    processIndex = queue->items[queue->head];
    queue->head = (queue->head + 1) % ReadyQueueCapacity;
    queue->count--;
    return processIndex;
}

int readyQueueExtractPrivileged(ReadyQueue* queue, Bcp processes[]) {
    int removed = -1;
    int originalCount;

    if (!queue || !processes || queue->count <= 0) return -1;
    originalCount = queue->count;
    for (int i = 0; i < originalCount; ++i) {
        int index = readyQueuePop(queue);
        if (removed < 0 && index >= 0 && processes[index].privileged &&
            processes[index].state == processStateReady) {
            removed = index;
        } else if (index >= 0) {
            readyQueuePush(queue, index);
        }
    }
    return removed;
}

int readyQueueIsEmpty(const ReadyQueue* queue) {
    return !queue || queue->count == 0;
}
