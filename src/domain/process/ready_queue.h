#ifndef DOMAIN_PROCESS_READY_QUEUE_H
#define DOMAIN_PROCESS_READY_QUEUE_H

#include "../../simulation/config.h"

struct Process;

typedef struct ReadyQueue {
    struct Process* items[SIM_READY_CAPACITY];
    int head;
    int tail;
    int count;
} ReadyQueue;

void ready_queue_init(ReadyQueue* queue);
int ready_queue_push(ReadyQueue* queue, struct Process* process);
int ready_queue_push_front(ReadyQueue* queue, struct Process* process);
struct Process* ready_queue_pop(ReadyQueue* queue);
int ready_queue_count(const ReadyQueue* queue);
int ready_queue_is_empty(const ReadyQueue* queue);
int ready_queue_contains(const ReadyQueue* queue, const struct Process* process);

#endif
