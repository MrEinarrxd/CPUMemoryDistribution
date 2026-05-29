#ifndef IO_QUEUE_H
#define IO_QUEUE_H

#include "../../utils/constants.h"

struct Process;

typedef struct IoSubQueue {
    struct Process* items[tamColaListos];
    int capacity;
    int head;
    int tail;
    int size;
    int multiplier;
} IoSubQueue;

typedef struct IoQueue {
    IoSubQueue devices[numColasEs];
} IoQueue;

IoQueue* ioQueueCreate(const int multipliers[numColasEs]);
void ioQueueDestroy(IoQueue* queue);
int ioQueueSend(IoQueue* queue, struct Process* process, int deviceId);
struct Process* ioQueueReceive(IoQueue* queue, int deviceId);
void ioQueueTick(IoQueue* queue);
struct Process* ioQueueGetFinished(IoQueue* queue, int deviceId);
int ioQueueHasFinished(IoQueue* queue, int deviceId);

#endif
