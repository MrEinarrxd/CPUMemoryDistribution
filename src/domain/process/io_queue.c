#include "io_queue.h"

#include "process.h"
#include <string.h>

void io_queue_init(IoQueue* queue, const int multipliers[SIM_IO_DEVICES]) {
    if (!queue) return;
    memset(queue, 0, sizeof(*queue));
    for (int i = 0; i < SIM_IO_DEVICES; i++) {
        queue->devices[i].multiplier = multipliers ? multipliers[i] : 1;
    }
}

int io_queue_send(IoQueue* queue, Process* process, int device) {
    if (!queue || !process || device < 0 || device >= SIM_IO_DEVICES) return -1;
    IoDeviceQueue* dev = &queue->devices[device];
    if (dev->count >= SIM_READY_CAPACITY) return -1;
    dev->items[dev->tail] = process;
    dev->tail = (dev->tail + 1) % SIM_READY_CAPACITY;
    dev->count++;
    return 0;
}

void io_queue_tick(IoQueue* queue) {
    if (!queue) return;
    for (int d = 0; d < SIM_IO_DEVICES; d++) {
        IoDeviceQueue* dev = &queue->devices[d];
        for (int i = 0; i < dev->count; i++) {
            int idx = (dev->head + i) % SIM_READY_CAPACITY;
            Process* process = dev->items[idx];
            if (process && process->pcb.io_time_remaining > 0) process->pcb.io_time_remaining--;
        }
    }
}

Process* io_queue_pop_finished(IoQueue* queue, int device) {
    if (!queue || device < 0 || device >= SIM_IO_DEVICES) return NULL;
    IoDeviceQueue* dev = &queue->devices[device];
    for (int i = 0; i < dev->count; i++) {
        int idx = (dev->head + i) % SIM_READY_CAPACITY;
        Process* process = dev->items[idx];
        if (!process || process->pcb.io_time_remaining != 0) continue;
        for (int j = i; j < dev->count - 1; j++) {
            int cur = (dev->head + j) % SIM_READY_CAPACITY;
            int next = (dev->head + j + 1) % SIM_READY_CAPACITY;
            dev->items[cur] = dev->items[next];
        }
        int last = (dev->head + dev->count - 1) % SIM_READY_CAPACITY;
        dev->items[last] = NULL;
        dev->tail = (dev->tail - 1 + SIM_READY_CAPACITY) % SIM_READY_CAPACITY;
        dev->count--;
        return process;
    }
    return NULL;
}

int io_queue_total_waiting(const IoQueue* queue) {
    if (!queue) return 0;
    int total = 0;
    for (int d = 0; d < SIM_IO_DEVICES; d++) total += queue->devices[d].count;
    return total;
}
