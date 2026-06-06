#ifndef DISTRIBUTED_FAKE_BACKEND_H
#define DISTRIBUTED_FAKE_BACKEND_H

#include "backend.h"

int fake_backend_run_stats_task(DistributedBackend* backend, ProcessTable* table);
int fake_backend_run_aging_task(DistributedBackend* backend, ProcessTable* table);

#endif
