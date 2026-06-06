#include "ioQueue.h"

static int multipliers[IoDeviceCount] = {2, 4, 8, 12};

static int subqueuePush(IoSubQueue* queue, int processIndex) {
    if (!queue || queue->count >= ReadyQueueCapacity) return -1;
    queue->items[queue->tail] = processIndex;
    queue->tail = (queue->tail + 1) % ReadyQueueCapacity;
    queue->count++;
    return 0;
}

static int subqueuePop(IoSubQueue* queue) {
    int processIndex;
    if (!queue || queue->count <= 0) return -1;
    processIndex = queue->items[queue->head];
    queue->head = (queue->head + 1) % ReadyQueueCapacity;
    queue->count--;
    return processIndex;
}

void ioQueueInit(IoQueue* queue) {
    if (!queue) return;
    for (int i = 0; i < IoDeviceCount; ++i) {
        queue->devices[i].head = 0;
        queue->devices[i].tail = 0;
        queue->devices[i].count = 0;
        queue->devices[i].multiplier = multipliers[i];
    }
}

int ioQueueSend(IoQueue* queue, Bcp processes[], int processIndex, int device, int baseCycles) {
    if (!queue || !processes || processIndex < 0) return -1;
    if (device < 0 || device >= IoDeviceCount) device = 0;
    if (subqueuePush(&queue->devices[device], processIndex) != 0) return -1;
    processes[processIndex].state = processStateWaitingIo;
    processes[processIndex].ioDevice = device;
    processes[processIndex].ioTimeRemaining = baseCycles * queue->devices[device].multiplier;
    processes[processIndex].timesInIo++;
    processes[processIndex].ioOperationsPending++;
    return 0;
}

void ioQueueTick(IoQueue* queue, Bcp processes[], ReadyQueue* readyQueue, int currentTime) {
    if (!queue || !processes || !readyQueue) return;
    for (int device = 0; device < IoDeviceCount; ++device) {
        IoSubQueue* sub = &queue->devices[device];
        int originalCount = sub->count;
        for (int i = 0; i < originalCount; ++i) {
            int index = subqueuePop(sub);
            if (index < 0 || processes[index].state == processStateFinished) continue;
            processes[index].ioTimeRemaining--;
            if (processes[index].ioTimeRemaining <= 0) {
                if (readyQueuePush(readyQueue, index) == 0) {
                    processes[index].state = processStateReady;
                    processes[index].ioDevice = -1;
                    processes[index].ioTimeRemaining = 0;
                    processes[index].lastReadyTime = currentTime;
                    if (processes[index].ioOperationsPending > 0) {
                        processes[index].ioOperationsPending--;
                    }
                } else {
                    processes[index].ioTimeRemaining = 0;
                    subqueuePush(sub, index);
                }
            } else {
                subqueuePush(sub, index);
            }
        }
    }
}

int ioQueueTotalCount(const IoQueue* queue) {
    int total = 0;
    if (!queue) return 0;
    for (int i = 0; i < IoDeviceCount; ++i) {
        total += queue->devices[i].count;
    }
    return total;
}
