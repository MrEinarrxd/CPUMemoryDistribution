#ifndef CpuMemoryIoQueueH
#define CpuMemoryIoQueueH

#include "bcp.h"
#include "readyQueue.h"

typedef struct IoSubQueue {
    int items[ReadyQueueCapacity];
    int head;
    int tail;
    int count;
    int multiplier;
} IoSubQueue;

typedef struct IoQueue {
    IoSubQueue devices[IoDeviceCount];
} IoQueue;

void ioQueueInit(IoQueue* queue);
int ioQueueSend(IoQueue* queue, Bcp processes[], int processIndex, int device, int baseCycles);
void ioQueueTick(IoQueue* queue, Bcp processes[], ReadyQueue* readyQueue, int currentTime);
int ioQueueTotalCount(const IoQueue* queue);

#endif
