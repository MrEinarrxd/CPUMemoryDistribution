#include "distributed_tasks.h"

#include "../domain/process/pcb.h"
#include <string.h>

typedef struct RankRow {
    char process_id[SIM_ID_LEN];
    int primary;
    int secondary;
} RankRow;

static void insert_rank(RankRow top[SIM_TOP_N], int* count, const char* id, int primary, int secondary) {
    RankRow row;
    strncpy(row.process_id, id, SIM_ID_LEN - 1);
    row.process_id[SIM_ID_LEN - 1] = '\0';
    row.primary = primary;
    row.secondary = secondary;
    int limit = *count < SIM_TOP_N ? *count : SIM_TOP_N;
    int pos = limit;
    for (int i = 0; i < limit; i++) {
        if (primary > top[i].primary ||
            (primary == top[i].primary && secondary > top[i].secondary)) {
            pos = i;
            break;
        }
    }
    if (pos >= SIM_TOP_N) return;
    if (*count < SIM_TOP_N) (*count)++;
    for (int i = *count - 1; i > pos; i--) top[i] = top[i - 1];
    top[pos] = row;
}

void distributed_calculate_stats(const BcpSummary* rows, int count, DistributedStats* out_stats) {
    if (!out_stats) return;
    memset(out_stats, 0, sizeof(*out_stats));
    if (!rows || count <= 0) return;
    RankRow wasters[SIM_TOP_N] = {0};
    int waster_count = 0;
    out_stats->process_count = count;
    for (int i = 0; i < count; i++) {
        out_stats->total_assigned_cycles += rows[i].total_cpu_cycles;
        out_stats->total_executed_cycles += rows[i].time_in_execution;
        out_stats->total_io_operations += rows[i].times_in_io;
        if (rows[i].state == PROCESS_FINISHED) {
            out_stats->finished_count++;
        } else {
            out_stats->active_count++;
            out_stats->total_remaining_cycles += rows[i].remaining_cycles;
            if (rows[i].state == PROCESS_WAITING_IO || rows[i].state == PROCESS_READY)
                out_stats->waiting_count++;
        }
        insert_rank(wasters, &waster_count, rows[i].process_id,
                    rows[i].wasted_cpu_cycles, rows[i].remaining_cycles);
    }
    out_stats->avg_remaining_cycles = out_stats->active_count > 0
        ? (int)(out_stats->total_remaining_cycles / out_stats->active_count)
        : 0;
    out_stats->avg_cpu_utilization = out_stats->total_assigned_cycles > 0
        ? (float)out_stats->total_executed_cycles / (float)out_stats->total_assigned_cycles
        : 0.0f;
    out_stats->top_wasters_count = waster_count;
    for (int i = 0; i < waster_count; i++) {
        strncpy(out_stats->top_wasters_ids[i], wasters[i].process_id, SIM_ID_LEN - 1);
        out_stats->top_wasters_waste[i] = wasters[i].primary;
    }
}

void distributed_calculate_aging(const RrProcessData* rows, int count, AgingResults* out_aging) {
    if (!out_aging) return;
    memset(out_aging, 0, sizeof(*out_aging));
    if (!rows || count <= 0) return;
    RankRow aged[SIM_TOP_N] = {0};
    RankRow wasters[SIM_TOP_N] = {0};
    int aged_count = 0;
    int waster_count = 0;
    float total_utilization = 0.0f;
    for (int i = 0; i < count; i++) {
        insert_rank(aged, &aged_count, rows[i].process_id,
                    rows[i].times_returned_to_ready, rows[i].remaining_cycles);
        insert_rank(wasters, &waster_count, rows[i].process_id,
                    rows[i].wasted_cpu_cycles, rows[i].remaining_cycles);
        out_aging->total_returns_to_ready += rows[i].times_returned_to_ready;
        total_utilization += 1.0f - rows[i].cpu_waste_ratio;
    }
    out_aging->avg_cpu_utilization = total_utilization / (float)count;
    out_aging->top_aged_count = aged_count;
    out_aging->top_wasters_count = waster_count;
    for (int i = 0; i < aged_count; i++) {
        strncpy(out_aging->top_aged_ids[i], aged[i].process_id, SIM_ID_LEN - 1);
        out_aging->top_aged_returns[i] = aged[i].primary;
        out_aging->top_aged_remaining[i] = aged[i].secondary;
    }
    for (int i = 0; i < waster_count; i++) {
        strncpy(out_aging->top_wasters_ids[i], wasters[i].process_id, SIM_ID_LEN - 1);
        out_aging->top_wasters_waste[i] = wasters[i].primary;
    }
}

void distributed_integrate_stats(const DistributedStats* partials, int count, DistributedStats* out_stats) {
    if (!out_stats) return;
    memset(out_stats, 0, sizeof(*out_stats));
    if (!partials || count <= 0) return;
    RankRow wasters[SIM_TOP_N] = {0};
    int waster_count = 0;
    for (int i = 0; i < count; i++) {
        out_stats->process_count += partials[i].process_count;
        out_stats->active_count += partials[i].active_count;
        out_stats->finished_count += partials[i].finished_count;
        out_stats->waiting_count += partials[i].waiting_count;
        out_stats->total_remaining_cycles += partials[i].total_remaining_cycles;
        out_stats->total_assigned_cycles += partials[i].total_assigned_cycles;
        out_stats->total_executed_cycles += partials[i].total_executed_cycles;
        out_stats->total_io_operations += partials[i].total_io_operations;
        for (int j = 0; j < partials[i].top_wasters_count; j++) {
            insert_rank(wasters, &waster_count, partials[i].top_wasters_ids[j],
                        partials[i].top_wasters_waste[j], 0);
        }
    }
    out_stats->avg_remaining_cycles = out_stats->active_count > 0
        ? (int)(out_stats->total_remaining_cycles / out_stats->active_count)
        : 0;
    out_stats->avg_cpu_utilization = out_stats->total_assigned_cycles > 0
        ? (float)out_stats->total_executed_cycles / (float)out_stats->total_assigned_cycles
        : 0.0f;
    out_stats->top_wasters_count = waster_count;
    for (int i = 0; i < waster_count; i++) {
        strncpy(out_stats->top_wasters_ids[i], wasters[i].process_id, SIM_ID_LEN - 1);
        out_stats->top_wasters_waste[i] = wasters[i].primary;
    }
}

void distributed_integrate_aging(const AgingResults* partials, int count, AgingResults* out_aging) {
    if (!out_aging) return;
    memset(out_aging, 0, sizeof(*out_aging));
    if (!partials || count <= 0) return;
    RankRow aged[SIM_TOP_N] = {0};
    RankRow wasters[SIM_TOP_N] = {0};
    int aged_count = 0;
    int waster_count = 0;
    for (int i = 0; i < count; i++) {
        out_aging->total_returns_to_ready += partials[i].total_returns_to_ready;
        out_aging->avg_cpu_utilization += partials[i].avg_cpu_utilization;
        for (int j = 0; j < partials[i].top_aged_count; j++) {
            insert_rank(aged, &aged_count, partials[i].top_aged_ids[j],
                        partials[i].top_aged_returns[j], partials[i].top_aged_remaining[j]);
        }
        for (int j = 0; j < partials[i].top_wasters_count; j++) {
            insert_rank(wasters, &waster_count, partials[i].top_wasters_ids[j],
                        partials[i].top_wasters_waste[j], 0);
        }
    }
    out_aging->avg_cpu_utilization /= (float)count;
    out_aging->top_aged_count = aged_count;
    out_aging->top_wasters_count = waster_count;
    for (int i = 0; i < aged_count; i++) {
        strncpy(out_aging->top_aged_ids[i], aged[i].process_id, SIM_ID_LEN - 1);
        out_aging->top_aged_returns[i] = aged[i].primary;
        out_aging->top_aged_remaining[i] = aged[i].secondary;
    }
    for (int i = 0; i < waster_count; i++) {
        strncpy(out_aging->top_wasters_ids[i], wasters[i].process_id, SIM_ID_LEN - 1);
        out_aging->top_wasters_waste[i] = wasters[i].primary;
    }
}
