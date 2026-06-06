#ifndef DISTRIBUTED_BACKEND_H
#define DISTRIBUTED_BACKEND_H

#include "../domain/process/process_table.h"
#include "protocol.h"

typedef enum DistributedMode {
    DISTRIBUTED_FAKE = 1,
    DISTRIBUTED_REAL_PVM = 2
} DistributedMode;

typedef struct DistributedBackend {
    DistributedMode mode;
    int master_tid;
    int slave_tids[SIM_DISTRIBUTED_SLAVES];
    DistributedStats partial_stats[SIM_DISTRIBUTED_SLAVES];
    AgingResults partial_aging[SIM_DISTRIBUTED_SLAVES];
    DistributedStats stats;
    AgingResults aging;
    int started;
} DistributedBackend;

void distributed_backend_init(DistributedBackend* backend, DistributedMode mode);
int distributed_backend_start(DistributedBackend* backend);
int distributed_backend_run_stats_task(DistributedBackend* backend, ProcessTable* table);
int distributed_backend_run_aging_task(DistributedBackend* backend, ProcessTable* table);
void distributed_backend_print_results(const DistributedBackend* backend);
void distributed_backend_stop(DistributedBackend* backend);

#endif
