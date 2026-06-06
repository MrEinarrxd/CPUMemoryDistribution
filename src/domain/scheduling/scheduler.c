#include "scheduler.h"

#include <string.h>

void scheduler_init(Scheduler* scheduler, int quantum) {
    if (!scheduler) return;
    memset(scheduler, 0, sizeof(*scheduler));
    scheduler->algorithm = SCHEDULER_FCFS;
    scheduler->current_quantum = quantum > 0 ? quantum : 20;
}

const char* scheduler_algorithm_name(SchedulerAlgorithm algorithm) {
    return algorithm == SCHEDULER_RR ? "Round Robin" : "FCFS";
}

void scheduler_set_algorithm(Scheduler* scheduler, SchedulerAlgorithm algorithm) {
    if (!scheduler) return;
    if (scheduler->algorithm != algorithm) scheduler->algorithm = algorithm;
}

static Process* remove_by_id(ReadyQueue* queue, const char* process_id) {
    if (!queue || !process_id || process_id[0] == '\0') return NULL;
    for (int i = 0; i < queue->count; i++) {
        int idx = (queue->head + i) % SIM_READY_CAPACITY;
        Process* process = queue->items[idx];
        if (!process || strcmp(process->pcb.process_id, process_id) != 0) continue;
        for (int j = i; j < queue->count - 1; j++) {
            int cur = (queue->head + j) % SIM_READY_CAPACITY;
            int next = (queue->head + j + 1) % SIM_READY_CAPACITY;
            queue->items[cur] = queue->items[next];
        }
        queue->tail = (queue->tail - 1 + SIM_READY_CAPACITY) % SIM_READY_CAPACITY;
        queue->items[queue->tail] = NULL;
        queue->count--;
        return process;
    }
    return NULL;
}

Process* scheduler_select_next(Scheduler* scheduler, ReadyQueue* ready_queue) {
    if (!scheduler || !ready_queue) return NULL;
    if (scheduler->algorithm == SCHEDULER_RR && scheduler->has_privileged_process) {
        Process* privileged = remove_by_id(ready_queue, scheduler->privileged_process_id);
        if (privileged) return privileged;
    }
    return ready_queue_pop(ready_queue);
}

static void ranking_insert(RankingEntry entries[SIM_TOP_N], int* count, const char* id, int primary, int secondary) {
    if (!entries || !count || !id) return;
    RankingEntry candidate;
    strncpy(candidate.process_id, id, SIM_ID_LEN - 1);
    candidate.process_id[SIM_ID_LEN - 1] = '\0';
    candidate.primary = primary;
    candidate.secondary = secondary;

    int limit = *count < SIM_TOP_N ? *count : SIM_TOP_N;
    int pos = limit;
    for (int i = 0; i < limit; i++) {
        if (primary > entries[i].primary ||
            (primary == entries[i].primary && secondary > entries[i].secondary)) {
            pos = i;
            break;
        }
    }
    if (pos >= SIM_TOP_N) return;
    if (*count < SIM_TOP_N) (*count)++;
    for (int i = *count - 1; i > pos; i--) entries[i] = entries[i - 1];
    entries[pos] = candidate;
}

void scheduler_update_rankings(Scheduler* scheduler, ProcessTable* table) {
    if (!scheduler || !table) return;
    scheduler->top_aged_count = 0;
    scheduler->top_wasters_count = 0;
    memset(scheduler->top_aged, 0, sizeof(scheduler->top_aged));
    memset(scheduler->top_wasters, 0, sizeof(scheduler->top_wasters));
    for (int i = 0; i < SIM_TOTAL_PROCESSES; i++) {
        Process* process = &table->processes[i];
        Pcb* pcb = &process->pcb;
        if (pcb->state == PROCESS_FINISHED || pcb->times_executed <= 0) continue;
        ranking_insert(scheduler->top_aged, &scheduler->top_aged_count,
                       pcb->process_id, pcb->times_returned_to_ready, pcb->remaining_cycles);
        ranking_insert(scheduler->top_wasters, &scheduler->top_wasters_count,
                       pcb->process_id, pcb->wasted_cpu_cycles, pcb->remaining_cycles);
    }
}

void scheduler_prioritize_process(Scheduler* scheduler, const char* process_id) {
    if (!scheduler || !process_id || process_id[0] == '\0') return;
    strncpy(scheduler->privileged_process_id, process_id, SIM_ID_LEN - 1);
    scheduler->privileged_process_id[SIM_ID_LEN - 1] = '\0';
    scheduler->has_privileged_process = 1;
}

void scheduler_clear_privilege_if_finished(Scheduler* scheduler, ProcessTable* table) {
    if (!scheduler || !table || !scheduler->has_privileged_process) return;
    Process* process = process_table_find_by_id(table, scheduler->privileged_process_id);
    if (!process || process->pcb.state == PROCESS_FINISHED) {
        scheduler->has_privileged_process = 0;
        scheduler->privileged_process_id[0] = '\0';
    }
}

void scheduler_rebalance_quantum(Scheduler* scheduler, ProcessTable* table, ReadyQueue* ready_queue, IoQueue* io_queue) {
    if (!scheduler || !table || !ready_queue || !io_queue) return;
    if (scheduler->algorithm != SCHEDULER_RR) return;
    if (table->dispatch_count - scheduler->last_rebalance_dispatch < 20) return;
    scheduler->last_rebalance_dispatch = table->dispatch_count;

    int ready = ready_queue_count(ready_queue);
    int waiting = io_queue_total_waiting(io_queue);
    int total = ready + waiting;
    table->proportion_ready = total > 0 ? (float)ready / (float)total : 0.0f;
    table->proportion_waiting = total > 0 ? (float)waiting / (float)total : 0.0f;

    if (table->proportion_ready >= 0.75f) {
        scheduler->current_quantum -= 5;
        if (scheduler->current_quantum < 5) scheduler->current_quantum = 5;
    } else if (table->proportion_waiting >= 0.75f) {
        scheduler->current_quantum += 5;
    }
    table->quantum_current = scheduler->current_quantum;
}

int scheduler_auto_switch(Scheduler* scheduler, ProcessTable* table) {
    if (!scheduler || !table || table->current_cycle < 50) return 0;
    if (table->current_cycle - scheduler->last_auto_switch_cycle < 200) return 0;
    int triggers = 0;
    if (table->avg_waiting_time > 500.0f) triggers++;
    if (table->cpu_utilization < 0.30f) triggers++;
    if (table->total_io_operations > 100) triggers++;

    long returns = 0;
    long waste = 0;
    long remaining = 0;
    int active = 0;
    for (int i = 0; i < SIM_TOTAL_PROCESSES; i++) {
        Pcb* pcb = &table->processes[i].pcb;
        if (pcb->state == PROCESS_FINISHED || pcb->state == PROCESS_NEW) continue;
        returns += pcb->times_returned_to_ready;
        waste += pcb->wasted_cpu_cycles;
        remaining += pcb->remaining_cycles;
        active++;
    }
    if (active > 0) {
        if (returns / active > 5) triggers++;
        if (waste / active > 40) triggers++;
        if (remaining / active > 50000) triggers++;
    }
    table->should_switch_algorithm = triggers >= 3;
    if (!table->should_switch_algorithm) return 0;
    scheduler->algorithm = scheduler->algorithm == SCHEDULER_FCFS ? SCHEDULER_RR : SCHEDULER_FCFS;
    scheduler->last_auto_switch_cycle = table->current_cycle;
    table->algorithm_change_count++;
    return 1;
}
