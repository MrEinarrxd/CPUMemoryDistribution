#include "process_table.h"

#include <stdlib.h>
#include <string.h>

typedef struct ArrivalPair {
    int arrival;
    int original_index;
} ArrivalPair;

static int compare_arrivals(const void* left, const void* right) {
    const ArrivalPair* a = (const ArrivalPair*)left;
    const ArrivalPair* b = (const ArrivalPair*)right;
    if (a->arrival < b->arrival) return -1;
    if (a->arrival > b->arrival) return 1;
    return a->original_index - b->original_index;
}

static int even_page_count(RandomSource* random, const SimulationConfig* config) {
    int min_pair = config->min_frames_per_process / 2;
    int max_pair = config->max_frames_per_process / 2;
    return random_int(random, min_pair, max_pair) * 2;
}

void process_table_init(ProcessTable* table) {
    if (!table) return;
    memset(table, 0, sizeof(*table));
    table->total_processes = SIM_TOTAL_PROCESSES;
}

void process_table_generate(ProcessTable* table, const SimulationConfig* config, RandomSource* random) {
    if (!table || !config || !random) return;
    int arrivals[SIM_TOTAL_PROCESSES];
    random_unique_arrivals(random, arrivals, SIM_TOTAL_PROCESSES, config->arrival_min, config->arrival_max);
    ArrivalPair pairs[SIM_TOTAL_PROCESSES];
    for (int i = 0; i < SIM_TOTAL_PROCESSES; i++) {
        pairs[i].arrival = arrivals[i];
        pairs[i].original_index = i;
    }
    qsort(pairs, SIM_TOTAL_PROCESSES, sizeof(pairs[0]), compare_arrivals);

    for (int i = 0; i < SIM_TOTAL_PROCESSES; i++) {
        int creation = pairs[i].arrival + random_int(random, config->creation_sleep_min, config->creation_sleep_max);
        int total_cycles = random_int(random, config->cpu_cycles_min, config->cpu_cycles_max);
        int pages = even_page_count(random, config);
        process_init(&table->processes[i], i, pairs[i].arrival, creation, total_cycles, pages);
        if (i < SIM_ACTIVE_SLOTS) {
            table->active_slots[i] = &table->processes[i];
            table->processes[i].pcb.active_slot = i;
            table->processes[i].pcb.page_table_base = i * SIM_MAX_PAGES_PER_PROCESS;
        } else {
            int new_index = i - SIM_ACTIVE_SLOTS;
            table->new_requests[new_index] = &table->processes[i];
        }
    }
}

int process_table_find_free_slot(ProcessTable* table) {
    if (!table) return -1;
    for (int i = 0; i < SIM_ACTIVE_SLOTS; i++) {
        if (!table->active_slots[i]) return i;
    }
    return -1;
}

int process_table_find_slot(ProcessTable* table, const Process* process) {
    if (!table || !process) return -1;
    for (int i = 0; i < SIM_ACTIVE_SLOTS; i++) {
        if (table->active_slots[i] == process) return i;
    }
    return -1;
}

int process_table_has_pending_new_requests(const ProcessTable* table) {
    if (!table) return 0;
    for (int i = 0; i < SIM_NEW_REQUESTS; i++) {
        if (table->new_requests[i]) return 1;
    }
    return 0;
}

int process_table_active_count(const ProcessTable* table) {
    if (!table) return 0;
    int count = 0;
    for (int i = 0; i < SIM_ACTIVE_SLOTS; i++) {
        if (table->active_slots[i] && table->active_slots[i]->pcb.state != PROCESS_FINISHED) count++;
    }
    return count;
}

Process* process_table_find_by_id(ProcessTable* table, const char* process_id) {
    if (!table || !process_id) return NULL;
    for (int i = 0; i < SIM_TOTAL_PROCESSES; i++) {
        if (strcmp(table->processes[i].pcb.process_id, process_id) == 0) return &table->processes[i];
    }
    return NULL;
}

void process_table_update_averages(ProcessTable* table) {
    if (!table) return;
    if (table->finished_processes > 0) {
        table->avg_waiting_time = (float)table->total_waiting_time_finished / (float)table->finished_processes;
        table->avg_turnaround_time = (float)table->total_turnaround_time_finished / (float)table->finished_processes;
        table->avg_time_in_execution = (float)table->total_execution_time_finished / (float)table->finished_processes;
    }
    if (table->current_cycle > 0) {
        float capacity = (float)table->current_cycle * 70.0f;
        table->cpu_utilization = capacity > 0.0f ? (float)table->total_cpu_cycles_executed / capacity : 0.0f;
        if (table->cpu_utilization > 1.0f) table->cpu_utilization = 1.0f;
        table->avg_processes_finished_per_cycle =
            (float)table->finished_processes / (float)table->current_cycle;
    }
    long accounted = table->total_cpu_cycles_executed + table->total_cpu_waste_cycles;
    table->cpu_waste_ratio = accounted > 0
        ? (float)table->total_cpu_waste_cycles / (float)accounted
        : 0.0f;
}
