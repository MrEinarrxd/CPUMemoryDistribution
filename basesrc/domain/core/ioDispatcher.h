// === src/domain/io/ioDispatcher.h ===

#ifndef IO_DISPATCHER_H
#define IO_DISPATCHER_H

#include "../../utils/constants.h"

struct IoQueue;
struct Process;

// Solo referencia, no destruye IoQueue
typedef struct IoDispatcher {
    struct IoQueue* ioQueue;
    int totalIoOperations;
    float avgIoTime;
} IoDispatcher;

IoDispatcher* ioDispatcherCreate(struct IoQueue* ioQueue);

void ioDispatcherDestroy(IoDispatcher* dispatcher);

int ioDispatcherDispatch(IoDispatcher* dispatcher, struct Process* process, int deviceId);

void ioDispatcherTick(IoDispatcher* dispatcher);

int ioDispatcherGetTotalIoOperations(IoDispatcher* dispatcher);

float ioDispatcherGetAvgIoTime(IoDispatcher* dispatcher);

#endif
