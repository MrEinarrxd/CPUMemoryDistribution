#include "ready_queue.h"

#include "process.h"
#include <string.h>

void ready_queue_init(ReadyQueue* queue) {
    if (!queue) return;
    memset(queue, 0, sizeof(*queue));
}

int ready_queue_contains(const ReadyQueue* queue, const Process* process) {
    if (!queue || !process) return 0;
    for (int i = 0; i < queue->count; i++) {
        int idx = (queue->head + i) % SIM_READY_CAPACITY;
        if (queue->items[idx] == process) return 1;
    }
    return 0;
}

int ready_queue_push(ReadyQueue* queue, Process* process) {
    if (!queue || !process || queue->count >= SIM_READY_CAPACITY) return -1;
    if (ready_queue_contains(queue, process)) return 0;
    queue->items[queue->tail] = process;
    queue->tail = (queue->tail + 1) % SIM_READY_CAPACITY;
    queue->count++;
    return 0;
}

int ready_queue_push_front(ReadyQueue* queue, Process* process) {
    if (!queue || !process || queue->count >= SIM_READY_CAPACITY) return -1;
    if (ready_queue_contains(queue, process)) return 0;
    queue->head = (queue->head - 1 + SIM_READY_CAPACITY) % SIM_READY_CAPACITY;
    queue->items[queue->head] = process;
    queue->count++;
    return 0;
}

Process* ready_queue_pop(ReadyQueue* queue) {
    if (!queue || queue->count <= 0) return NULL;
    Process* process = queue->items[queue->head];
    queue->items[queue->head] = NULL;
    queue->head = (queue->head + 1) % SIM_READY_CAPACITY;
    queue->count--;
    return process;
}

int ready_queue_count(const ReadyQueue* queue) {
    return queue ? queue->count : 0;
}

int ready_queue_is_empty(const ReadyQueue* queue) {
    return !queue || queue->count == 0;
}
