#include "fake_backend.h"

#include "distributed_tasks.h"
#include <string.h>

static int collect_summaries(ProcessTable* table, BcpSummary out[SIM_TOTAL_PROCESSES]) {
    int count = 0;
    for (int i = 0; i < SIM_TOTAL_PROCESSES; i++) {
        Pcb* pcb = &table->processes[i].pcb;
        out[count].pid = pcb->pid;
        strncpy(out[count].process_id, pcb->process_id, SIM_ID_LEN - 1);
        out[count].state = (int)pcb->state;
        out[count].remaining_cycles = pcb->remaining_cycles;
        out[count].total_cpu_cycles = pcb->total_cpu_cycles;
        out[count].time_in_execution = pcb->time_in_execution;
        out[count].times_in_io = pcb->times_in_io;
        out[count].wasted_cpu_cycles = pcb->wasted_cpu_cycles;
        count++;
    }
    return count;
}

static int collect_rr_rows(ProcessTable* table, RrProcessData out[SIM_TOTAL_PROCESSES]) {
    int count = 0;
    for (int i = 0; i < SIM_TOTAL_PROCESSES; i++) {
        Pcb* pcb = &table->processes[i].pcb;
        out[count].pid = pcb->pid;
        strncpy(out[count].process_id, pcb->process_id, SIM_ID_LEN - 1);
        out[count].remaining_cycles = pcb->remaining_cycles;
        out[count].total_cpu_cycles = pcb->total_cpu_cycles;
        out[count].time_in_execution = pcb->time_in_execution;
        out[count].quantum_assigned = pcb->quantum_assigned;
        out[count].quantum_used = pcb->quantum_used;
        out[count].times_returned_to_ready = pcb->times_returned_to_ready;
        out[count].wasted_cpu_cycles = pcb->wasted_cpu_cycles;
        out[count].cpu_waste_ratio = pcb->cpu_waste_ratio;
        count++;
    }
    return count;
}

int fake_backend_run_stats_task(DistributedBackend* backend, ProcessTable* table) {
    if (!backend || !table) return -1;
    BcpSummary rows[SIM_TOTAL_PROCESSES];
    int total = collect_summaries(table, rows);
    int base = total / SIM_DISTRIBUTED_SLAVES;
    int rem = total % SIM_DISTRIBUTED_SLAVES;
    int start = 0;
    for (int i = 0; i < SIM_DISTRIBUTED_SLAVES; i++) {
        int count = base + (i < rem ? 1 : 0);
        distributed_calculate_stats(&rows[start], count, &backend->partial_stats[i]);
        start += count;
    }
    distributed_integrate_stats(backend->partial_stats, SIM_DISTRIBUTED_SLAVES, &backend->stats);
    return 0;
}

int fake_backend_run_aging_task(DistributedBackend* backend, ProcessTable* table) {
    if (!backend || !table) return -1;
    RrProcessData rows[SIM_TOTAL_PROCESSES];
    int total = collect_rr_rows(table, rows);
    int base = total / SIM_DISTRIBUTED_SLAVES;
    int rem = total % SIM_DISTRIBUTED_SLAVES;
    int start = 0;
    for (int i = 0; i < SIM_DISTRIBUTED_SLAVES; i++) {
        int count = base + (i < rem ? 1 : 0);
        distributed_calculate_aging(&rows[start], count, &backend->partial_aging[i]);
        start += count;
    }
    distributed_integrate_aging(backend->partial_aging, SIM_DISTRIBUTED_SLAVES, &backend->aging);
    return 0;
}
