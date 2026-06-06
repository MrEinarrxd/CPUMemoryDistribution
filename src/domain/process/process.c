#include "process.h"

void process_init(Process* process, int index, int arrival, int creation, int total_cycles, int page_count) {
    if (!process) return;
    pcb_init(&process->pcb, index, arrival, creation, total_cycles, page_count);
    process->is_active = 1;
}

void process_activate(Process* process) {
    if (process) process->is_active = 1;
}

void process_deactivate(Process* process) {
    if (process) process->is_active = 0;
}
