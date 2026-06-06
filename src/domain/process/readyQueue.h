#ifndef CpuMemoryReadyQueueH
#define CpuMemoryReadyQueueH

#include "bcp.h"

typedef struct ReadyQueue {
    int items[ReadyQueueCapacity];
    int head;
    int tail;
    int count;
} ReadyQueue;

void readyQueueInit(ReadyQueue* queue);
int readyQueuePush(ReadyQueue* queue, int processIndex);
int readyQueuePushFront(ReadyQueue* queue, int processIndex);
int readyQueuePop(ReadyQueue* queue);
int readyQueueExtractPrivileged(ReadyQueue* queue, Bcp processes[]);
int readyQueueIsEmpty(const ReadyQueue* queue);

#endif
