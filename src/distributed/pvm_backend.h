#ifndef DISTRIBUTED_PVM_BACKEND_H
#define DISTRIBUTED_PVM_BACKEND_H

#include "backend.h"

int pvm_backend_start(DistributedBackend* backend);
int pvm_backend_run_stats_task(DistributedBackend* backend, ProcessTable* table);
int pvm_backend_run_aging_task(DistributedBackend* backend, ProcessTable* table);
void pvm_backend_stop(DistributedBackend* backend);

#endif
