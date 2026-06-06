#ifndef SIMULATION_ENGINE_H
#define SIMULATION_ENGINE_H

#include "../distributed/backend.h"
#include "../domain/memory/memory_manager.h"
#include "../domain/process/io_queue.h"
#include "../domain/process/process_table.h"
#include "../domain/process/ready_queue.h"
#include "../domain/scheduling/scheduler.h"
#include "../infra/logger.h"
#include "../infra/random.h"
#include "../infra/text_loader.h"
#include "config.h"

typedef struct SimulationEngine {
    SimulationConfig config;
    RandomSource random;
    TextLoader text_loader;
    ProcessTable table;
    ReadyQueue ready_queue;
    IoQueue io_queue;
    Scheduler scheduler;
    MemoryManager memory;
    DistributedBackend distributed;
    Logger simulation_log;
    Logger bcp_log;
    Process* current_process;
    int running;
    int last_distributed_dispatch;
    int last_resize_cycle;
    char mode_name[64];
} SimulationEngine;

int simulation_engine_init(SimulationEngine* engine, DistributedMode distributed_mode, const char* mode_name);
void simulation_engine_destroy(SimulationEngine* engine);
int simulation_engine_run(SimulationEngine* engine);
int simulation_engine_cycle(SimulationEngine* engine);

#endif
