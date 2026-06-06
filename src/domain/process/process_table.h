#ifndef DOMAIN_PROCESS_PROCESS_TABLE_H
#define DOMAIN_PROCESS_PROCESS_TABLE_H

#include "process.h"
#include "../../infra/random.h"
#include "../../simulation/config.h"

typedef struct ProcessTable {
    Process processes[SIM_TOTAL_PROCESSES];
    Process* active_slots[SIM_ACTIVE_SLOTS];
    Process* new_requests[SIM_NEW_REQUESTS];
    int total_processes;
    int finished_processes;
    int current_cycle;
    int dispatch_count;
    int simulation_start_time;
    long total_cpu_cycles_executed;
    long total_cpu_waste_cycles;
    float cpu_utilization;
    float cpu_waste_ratio;
    int total_context_switches;
    int total_context_switch_time;
    int total_io_operations;
    int algorithm_change_count;
    int total_waiting_time_finished;
    int total_turnaround_time_finished;
    int total_execution_time_finished;
    float avg_waiting_time;
    float avg_turnaround_time;
    float avg_time_in_execution;
    float avg_processes_finished_per_cycle;
    int memory_used_frames;
    int memory_free_frames;
    int largest_free_run;
    int free_run_count;
    int internal_waste;
    int external_waste;
    int page_faults;
    float fragmentation;
    int quantum_current;
    float proportion_ready;
    float proportion_waiting;
    int should_switch_algorithm;
} ProcessTable;

void process_table_init(ProcessTable* table);
void process_table_generate(ProcessTable* table, const SimulationConfig* config, RandomSource* random);
int process_table_find_free_slot(ProcessTable* table);
int process_table_find_slot(ProcessTable* table, const Process* process);
int process_table_has_pending_new_requests(const ProcessTable* table);
int process_table_active_count(const ProcessTable* table);
Process* process_table_find_by_id(ProcessTable* table, const char* process_id);
void process_table_update_averages(ProcessTable* table);

#endif
