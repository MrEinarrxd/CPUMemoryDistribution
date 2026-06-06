#ifndef DOMAIN_SCHEDULING_SCHEDULER_H
#define DOMAIN_SCHEDULING_SCHEDULER_H

#include "../process/io_queue.h"
#include "../process/process_table.h"
#include "../process/ready_queue.h"
#include "../../simulation/config.h"

typedef enum SchedulerAlgorithm {
    SCHEDULER_FCFS = 1,
    SCHEDULER_RR = 2
} SchedulerAlgorithm;

typedef struct RankingEntry {
    char process_id[SIM_ID_LEN];
    int primary;
    int secondary;
} RankingEntry;

typedef struct Scheduler {
    SchedulerAlgorithm algorithm;
    int current_quantum;
    int last_rebalance_dispatch;
    int last_auto_switch_cycle;
    char privileged_process_id[SIM_ID_LEN];
    int has_privileged_process;
    RankingEntry top_aged[SIM_TOP_N];
    int top_aged_count;
    RankingEntry top_wasters[SIM_TOP_N];
    int top_wasters_count;
} Scheduler;

void scheduler_init(Scheduler* scheduler, int quantum);
const char* scheduler_algorithm_name(SchedulerAlgorithm algorithm);
void scheduler_set_algorithm(Scheduler* scheduler, SchedulerAlgorithm algorithm);
Process* scheduler_select_next(Scheduler* scheduler, ReadyQueue* ready_queue);
void scheduler_update_rankings(Scheduler* scheduler, ProcessTable* table);
void scheduler_prioritize_process(Scheduler* scheduler, const char* process_id);
void scheduler_clear_privilege_if_finished(Scheduler* scheduler, ProcessTable* table);
void scheduler_rebalance_quantum(Scheduler* scheduler, ProcessTable* table, ReadyQueue* ready_queue, IoQueue* io_queue);
int scheduler_auto_switch(Scheduler* scheduler, ProcessTable* table);

#endif
