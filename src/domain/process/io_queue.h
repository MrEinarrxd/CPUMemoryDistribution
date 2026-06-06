#ifndef DOMAIN_PROCESS_IO_QUEUE_H
#define DOMAIN_PROCESS_IO_QUEUE_H

#include "../../simulation/config.h"

struct Process;

typedef struct IoDeviceQueue {
    struct Process* items[SIM_READY_CAPACITY];
    int head;
    int tail;
    int count;
    int multiplier;
} IoDeviceQueue;

typedef struct IoQueue {
    IoDeviceQueue devices[SIM_IO_DEVICES];
} IoQueue;

void io_queue_init(IoQueue* queue, const int multipliers[SIM_IO_DEVICES]);
int io_queue_send(IoQueue* queue, struct Process* process, int device);
void io_queue_tick(IoQueue* queue);
struct Process* io_queue_pop_finished(IoQueue* queue, int device);
int io_queue_total_waiting(const IoQueue* queue);

#endif
