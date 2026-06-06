#ifndef DOMAIN_PROCESS_PCB_H
#define DOMAIN_PROCESS_PCB_H

#include "../../simulation/config.h"

typedef enum ProcessState {
    PROCESS_NEW = 0,
    PROCESS_READY = 1,
    PROCESS_RUNNING = 2,
    PROCESS_WAITING_IO = 3,
    PROCESS_FINISHED = 4
} ProcessState;

typedef struct Pcb {
    char process_id[SIM_ID_LEN];
    int pid;
    ProcessState state;
    int priority;
    int total_cpu_cycles;
    int remaining_cycles;
    int arrival_time;
    int creation_time;
    int start_time;
    int finish_time;
    int current_slice_remaining;
    int context_switch_time;
    int context_switch_count;
    int time_in_execution;
    int time_in_waiting;
    int times_executed;
    int times_returned_to_ready;
    int ageing_time_slices;
    int quantum_assigned;
    int quantum_used;
    int wasted_cpu_cycles;
    float cpu_waste_ratio;
    int times_in_io;
    int io_time_remaining;
    int io_device;
    char io_phrase[SIM_PHRASE_LEN];
    char required_words[SIM_PHRASE_WORDS][SIM_WORD_LEN];
    int required_words_count;
    int page_count;
    int memory_requested;
    int memory_allocated;
    int page_table_base;
    int swap_address;
    int page_faults;
    int privileged;
    int active_slot;
} Pcb;

void pcb_init(Pcb* pcb, int index, int arrival, int creation, int total_cycles, int page_count);
void pcb_set_state(Pcb* pcb, ProcessState state);
const char* pcb_state_name(ProcessState state);
void pcb_update_waste_ratio(Pcb* pcb);

#endif
