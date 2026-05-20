// === src/domain/scheduler/readyQueue.h ===

#ifndef READY_QUEUE_H
#define READY_QUEUE_H

#include "../../utils/constants.h"

struct Process;

// Propietario exclusivo de Process items
typedef struct ReadyQueue {
    struct Process* processes[tamColaListos];
    int head;
    int tail;
    int count;
} ReadyQueue;

ReadyQueue* readyQueueCreate(void);

void readyQueueDestroy(ReadyQueue* queue);

int readyQueueEnqueue(ReadyQueue* queue, struct Process* process);

struct Process* readyQueueDequeue(ReadyQueue* queue);

struct Process* readyQueuePeek(ReadyQueue* queue);

int readyQueueGetCount(ReadyQueue* queue);

int readyQueueIsFull(ReadyQueue* queue);

int readyQueueIsEmpty(ReadyQueue* queue);

void readyQueueClear(ReadyQueue* queue);

#endif
