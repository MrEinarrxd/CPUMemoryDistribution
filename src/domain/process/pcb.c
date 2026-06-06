#include "pcb.h"

#include <stdio.h>
#include <string.h>

static void generate_process_id(int index, char out[SIM_ID_LEN]) {
    int one_based = index + 1;
    char letter = (char)('A' + (one_based - 1) / 100);
    int number = ((one_based - 1) % 100) + 1;
    snprintf(out, SIM_ID_LEN, "%c-%d", letter, number);
}

void pcb_init(Pcb* pcb, int index, int arrival, int creation, int total_cycles, int page_count) {
    if (!pcb) return;
    memset(pcb, 0, sizeof(*pcb));
    generate_process_id(index, pcb->process_id);
    pcb->pid = index + 1;
    pcb->state = PROCESS_NEW;
    pcb->priority = 0;
    pcb->total_cpu_cycles = total_cycles;
    pcb->remaining_cycles = total_cycles;
    pcb->arrival_time = arrival;
    pcb->creation_time = creation;
    pcb->start_time = -1;
    pcb->finish_time = -1;
    pcb->page_count = page_count;
    pcb->memory_requested = page_count * SIM_PAGE_WORDS;
    pcb->memory_allocated = pcb->memory_requested;
    pcb->page_table_base = -1;
    pcb->swap_address = -1;
    pcb->active_slot = -1;
    pcb->io_device = -1;
}

void pcb_set_state(Pcb* pcb, ProcessState state) {
    if (pcb) pcb->state = state;
}

const char* pcb_state_name(ProcessState state) {
    switch (state) {
        case PROCESS_NEW: return "NEW";
        case PROCESS_READY: return "READY";
        case PROCESS_RUNNING: return "RUNNING";
        case PROCESS_WAITING_IO: return "WAITING_IO";
        case PROCESS_FINISHED: return "FINISHED";
        default: return "UNKNOWN";
    }
}

void pcb_update_waste_ratio(Pcb* pcb) {
    if (!pcb) return;
    int accounted = pcb->time_in_execution + pcb->wasted_cpu_cycles;
    pcb->cpu_waste_ratio = accounted > 0
        ? (float)pcb->wasted_cpu_cycles / (float)accounted
        : 0.0f;
}
