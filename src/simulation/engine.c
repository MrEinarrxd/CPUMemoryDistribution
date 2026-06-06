#include "engine.h"

#include "../infra/clock.h"
#include "../ui/console_ui.h"
#include <stdio.h>
#include <string.h>

static int min_int(int a, int b) {
    return a < b ? a : b;
}

static void log_pcb(Logger* logger, const Pcb* pcb) {
    if (!logger || !pcb) return;
    logger_info(logger,
        "BCP[1/2] id=%s pid=%d state=%s priority=%d totalCpu=%d remaining=%d arrival=%d creation=%d start=%d finish=%d exec=%d waiting=%d timesExec=%d",
        pcb->process_id, pcb->pid, pcb_state_name(pcb->state), pcb->priority,
        pcb->total_cpu_cycles, pcb->remaining_cycles, pcb->arrival_time,
        pcb->creation_time, pcb->start_time, pcb->finish_time,
        pcb->time_in_execution, pcb->time_in_waiting, pcb->times_executed);
    logger_info(logger,
        "BCP[2/2] id=%s ctxSwitches=%d ctxTime=%d quantumAssigned=%d quantumUsed=%d waste=%d wasteRatio=%.4f returns=%d ageingSlices=%d ioCount=%d ioRemain=%d pageCount=%d memReq=%d memAlloc=%d pageBase=%d swap=%d pageFaults=%d privileged=%d activeSlot=%d slicePending=%d",
        pcb->process_id, pcb->context_switch_count, pcb->context_switch_time,
        pcb->quantum_assigned, pcb->quantum_used, pcb->wasted_cpu_cycles,
        pcb->cpu_waste_ratio, pcb->times_returned_to_ready,
        pcb->ageing_time_slices, pcb->times_in_io, pcb->io_time_remaining,
        pcb->page_count, pcb->memory_requested, pcb->memory_allocated,
        pcb->page_table_base, pcb->swap_address, pcb->page_faults,
        pcb->privileged, pcb->active_slot, pcb->current_slice_remaining);
}

static void log_table(Logger* logger, const ProcessTable* table) {
    if (!logger || !table) return;
    logger_info(logger,
        "TABLA[1/2] cycle=%d dispatches=%d total=%d finished=%d active=%d ctxSwitches=%d ctxTime=%d algoChanges=%d ioOps=%d cpuExec=%ld cpuWaste=%ld",
        table->current_cycle, table->dispatch_count, table->total_processes,
        table->finished_processes, process_table_active_count(table),
        table->total_context_switches, table->total_context_switch_time,
        table->algorithm_change_count, table->total_io_operations,
        table->total_cpu_cycles_executed, table->total_cpu_waste_cycles);
    logger_info(logger,
        "TABLA[2/2] cpuUtil=%.4f cpuWasteRatio=%.4f avgWait=%.2f avgTurn=%.2f avgExec=%.2f avgFinishedCycle=%.4f memUsed=%d memFree=%d largestFree=%d freeRuns=%d intWaste=%d extWaste=%d pageFaults=%d fragmentation=%.4f quantum=%d readyProp=%.4f waitProp=%.4f shouldSwitch=%d",
        table->cpu_utilization, table->cpu_waste_ratio, table->avg_waiting_time,
        table->avg_turnaround_time, table->avg_time_in_execution,
        table->avg_processes_finished_per_cycle, table->memory_used_frames,
        table->memory_free_frames, table->largest_free_run, table->free_run_count,
        table->internal_waste, table->external_waste, table->page_faults,
        table->fragmentation, table->quantum_current, table->proportion_ready,
        table->proportion_waiting, table->should_switch_algorithm);
}

static void move_to_ready(SimulationEngine* engine, Process* process, int count_return) {
    if (!engine || !process || process->pcb.state == PROCESS_FINISHED) return;
    pcb_set_state(&process->pcb, PROCESS_READY);
    if (count_return) process->pcb.times_returned_to_ready++;
    ready_queue_push(&engine->ready_queue, process);
}

static void activate_process(SimulationEngine* engine, Process* process, int slot) {
    if (!engine || !process || process->pcb.state != PROCESS_NEW) return;
    if (process->pcb.creation_time > engine->table.current_cycle) return;
    memory_manager_allocate_process(&engine->memory, process, slot);
    move_to_ready(engine, process, 0);
    logger_info(&engine->simulation_log, "Proceso creado: %s slot=%d", process->pcb.process_id, slot);
    log_pcb(&engine->bcp_log, &process->pcb);
}

static void admit_created_processes(SimulationEngine* engine) {
    for (int i = 0; i < SIM_ACTIVE_SLOTS; i++) {
        Process* process = engine->table.active_slots[i];
        if (process && process->pcb.state == PROCESS_NEW) activate_process(engine, process, i);
    }
}

static void admit_new_requests(SimulationEngine* engine) {
    for (int i = 0; i < SIM_NEW_REQUESTS; i++) {
        Process* process = engine->table.new_requests[i];
        if (!process || process->pcb.creation_time > engine->table.current_cycle) continue;
        int slot = process_table_find_free_slot(&engine->table);
        if (slot < 0) return;
        engine->table.new_requests[i] = NULL;
        engine->table.active_slots[slot] = process;
        process->pcb.active_slot = slot;
        process->pcb.page_table_base = slot * SIM_MAX_PAGES_PER_PROCESS;
        activate_process(engine, process, slot);
    }
}

static void process_io_completions(SimulationEngine* engine) {
    io_queue_tick(&engine->io_queue);
    for (int d = 0; d < SIM_IO_DEVICES; d++) {
        Process* process = io_queue_pop_finished(&engine->io_queue, d);
        while (process) {
            int slot = process_table_find_slot(&engine->table, process);
            if (slot >= 0) {
                memory_manager_access_words(&engine->memory, process, slot,
                                            process->pcb.required_words,
                                            process->pcb.required_words_count,
                                            &engine->text_loader, &engine->random);
            }
            process->pcb.io_device = -1;
            move_to_ready(engine, process, 1);
            process = io_queue_pop_finished(&engine->io_queue, d);
        }
    }
}

static void increment_waiting_times(SimulationEngine* engine) {
    for (int i = 0; i < SIM_TOTAL_PROCESSES; i++) {
        if (engine->table.processes[i].pcb.state == PROCESS_READY)
            engine->table.processes[i].pcb.time_in_waiting++;
    }
}

static void close_partial_quantum(SimulationEngine* engine, Pcb* pcb) {
    if (!engine || !pcb || engine->scheduler.algorithm != SCHEDULER_RR) return;
    int quantum = engine->scheduler.current_quantum;
    if (pcb->quantum_used > 0 && pcb->quantum_used < quantum) {
        int waste = quantum - pcb->quantum_used;
        pcb->wasted_cpu_cycles += waste;
        engine->table.total_cpu_waste_cycles += waste;
        pcb_update_waste_ratio(pcb);
    }
    pcb->quantum_used = 0;
}

static void set_scheduler_algorithm(SimulationEngine* engine, SchedulerAlgorithm next) {
    if (!engine || next == 0) return;
    if (next == SCHEDULER_RR &&
        engine->current_process &&
        engine->current_process->pcb.state == PROCESS_RUNNING) {
        move_to_ready(engine, engine->current_process, 1);
        engine->current_process = NULL;
    }
    scheduler_set_algorithm(&engine->scheduler, next);
}

static void finish_process(SimulationEngine* engine, Process* process, int slot) {
    if (!engine || !process) return;
    Pcb* pcb = &process->pcb;
    pcb->finish_time = engine->table.current_cycle;
    pcb->current_slice_remaining = 0;
    close_partial_quantum(engine, pcb);
    pcb_set_state(pcb, PROCESS_FINISHED);
    engine->table.finished_processes++;
    engine->table.total_waiting_time_finished += pcb->time_in_waiting;
    engine->table.total_turnaround_time_finished += pcb->finish_time - pcb->arrival_time;
    engine->table.total_execution_time_finished += pcb->time_in_execution;
    memory_manager_release_process(&engine->memory, process, slot);
    engine->table.active_slots[slot] = NULL;
    process_deactivate(process);
    logger_info(&engine->simulation_log, "Proceso terminado: %s", pcb->process_id);
    log_pcb(&engine->bcp_log, pcb);
    if (engine->current_process == process) engine->current_process = NULL;
}

static int all_done(SimulationEngine* engine) {
    return engine->table.finished_processes >= engine->table.total_processes &&
           !process_table_has_pending_new_requests(&engine->table) &&
           ready_queue_is_empty(&engine->ready_queue) &&
           io_queue_total_waiting(&engine->io_queue) == 0 &&
           engine->current_process == NULL;
}

static void send_to_io(SimulationEngine* engine, Process* process, int slot) {
    Pcb* pcb = &process->pcb;
    const char* phrase = text_loader_random_phrase(&engine->text_loader, &engine->random);
    strncpy(pcb->io_phrase, phrase, SIM_PHRASE_LEN - 1);
    pcb->io_phrase[SIM_PHRASE_LEN - 1] = '\0';
    pcb->required_words_count = text_loader_extract_words(pcb->io_phrase, pcb->required_words, SIM_PHRASE_WORDS);
    int device = pcb->times_in_io % SIM_IO_DEVICES;
    int base_time = random_int(&engine->random, engine->config.io_time_min, engine->config.io_time_max);
    pcb->io_time_remaining = base_time * engine->config.io_multipliers[device];
    pcb->io_device = device;
    pcb->times_in_io++;
    pcb_set_state(pcb, PROCESS_WAITING_IO);
    close_partial_quantum(engine, pcb);
    io_queue_send(&engine->io_queue, process, device);
    engine->table.total_io_operations++;
    if (engine->current_process == process) engine->current_process = NULL;
    logger_info(&engine->simulation_log, "Proceso %s enviado a E/S device=%d slot=%d", pcb->process_id, device + 1, slot);
}

static Process* select_process(SimulationEngine* engine, int* is_context_switch) {
    *is_context_switch = 0;
    if (engine->scheduler.algorithm == SCHEDULER_FCFS &&
        engine->current_process &&
        engine->current_process->pcb.state == PROCESS_RUNNING) {
        return engine->current_process;
    }
    Process* process = scheduler_select_next(&engine->scheduler, &engine->ready_queue);
    if (!process) return NULL;
    pcb_set_state(&process->pcb, PROCESS_RUNNING);
    engine->current_process = process;
    *is_context_switch = 1;
    return process;
}

static void maybe_grow_memory(SimulationEngine* engine, Process* process, int slot) {
    int growth = random_memory_growth(&engine->random);
    if (growth <= 0) return;
    Pcb* pcb = &process->pcb;
    int max_words = engine->config.max_frames_per_process * SIM_PAGE_WORDS;
    pcb->memory_requested += growth;
    if (pcb->memory_requested > max_words) pcb->memory_requested = max_words;
    int pages = (pcb->memory_requested + SIM_PAGE_WORDS - 1) / SIM_PAGE_WORDS;
    if (pages > pcb->page_count) memory_manager_resize_process(&engine->memory, process, slot, pages);
}

static void execute_process(SimulationEngine* engine, Process* process, int context_switch) {
    if (!engine || !process) return;
    Pcb* pcb = &process->pcb;
    int slot = process_table_find_slot(&engine->table, process);
    if (slot < 0) return;
    if (context_switch) {
        pcb->context_switch_time = random_int(&engine->random,
            engine->config.context_switch_min, engine->config.context_switch_max);
        pcb->context_switch_count++;
        engine->table.total_context_switches++;
        engine->table.total_context_switch_time += pcb->context_switch_time;
        engine->table.dispatch_count++;
        log_pcb(&engine->bcp_log, pcb);
    }
    if (pcb->start_time < 0) pcb->start_time = engine->table.current_cycle;
    if (pcb->remaining_cycles <= 0) {
        finish_process(engine, process, slot);
        return;
    }
    if (pcb->current_slice_remaining <= 0) {
        pcb->current_slice_remaining = random_int(&engine->random,
            engine->config.cpu_instance_min, engine->config.cpu_instance_max);
    }
    int cycles = min_int(pcb->current_slice_remaining, pcb->remaining_cycles);
    if (engine->scheduler.algorithm == SCHEDULER_RR) {
        pcb->quantum_assigned = engine->scheduler.current_quantum;
        int quantum_left = engine->scheduler.current_quantum - pcb->quantum_used;
        if (quantum_left <= 0) quantum_left = engine->scheduler.current_quantum;
        cycles = min_int(cycles, quantum_left);
    }
    pcb->remaining_cycles -= cycles;
    pcb->current_slice_remaining -= cycles;
    pcb->time_in_execution += cycles;
    pcb->times_executed++;
    if (engine->scheduler.algorithm == SCHEDULER_RR) pcb->quantum_used += cycles;
    engine->table.total_cpu_cycles_executed += cycles;
    maybe_grow_memory(engine, process, slot);

    if (pcb->remaining_cycles <= 0) {
        finish_process(engine, process, slot);
        return;
    }

    if (random_int(&engine->random, 0, 99) < 30) {
        send_to_io(engine, process, slot);
        return;
    }

    if (engine->scheduler.algorithm == SCHEDULER_RR) {
        if (pcb->quantum_used >= engine->scheduler.current_quantum) {
            pcb->quantum_used = 0;
        } else if (pcb->current_slice_remaining == 0) {
            close_partial_quantum(engine, pcb);
        }
        move_to_ready(engine, process, 1);
        engine->current_process = NULL;
    } else {
        pcb_set_state(pcb, PROCESS_RUNNING);
        engine->current_process = process;
    }
}

static void handle_command(SimulationEngine* engine, int command) {
    if (!engine || command == 0) return;
    if (command == 'q' || command == 'Q' || command == 27) {
        engine->running = 0;
        return;
    }
    if (command == 'p' || command == 'P') {
        while (engine->running) {
            int next = ui_read_command();
            if (next == 'p' || next == 'P') break;
            clock_sleep_ms(50);
        }
        return;
    }
    if (command == 'x' || command == 'X') {
        SchedulerAlgorithm next = ui_prompt_algorithm();
        if (next == 0) return;
        if (next == SCHEDULER_RR) {
            int quantum = ui_prompt_quantum();
            if (quantum > 0) engine->scheduler.current_quantum = quantum;
        }
        set_scheduler_algorithm(engine, next);
        engine->table.algorithm_change_count++;
        logger_info(&engine->simulation_log, "Cambio manual de algoritmo a %s", scheduler_algorithm_name(next));
        return;
    }
    if (command == 'a' || command == 'A') {
        scheduler_update_rankings(&engine->scheduler, &engine->table);
        ui_show_rankings(&engine->scheduler);
        char process_id[SIM_ID_LEN];
        if (ui_prompt_process_id(process_id) == 0) {
            Process* process = process_table_find_by_id(&engine->table, process_id);
            if (process && process->pcb.state != PROCESS_FINISHED) {
                scheduler_prioritize_process(&engine->scheduler, process_id);
                process->pcb.privileged = 1;
                logger_info(&engine->simulation_log, "Proceso privilegiado: %s", process_id);
            }
        }
    }
}

int simulation_engine_init(SimulationEngine* engine, DistributedMode distributed_mode, const char* mode_name) {
    if (!engine) return -1;
    memset(engine, 0, sizeof(*engine));
    engine->config = simulation_config_default();
    simulation_config_apply_env(&engine->config);
    strncpy(engine->mode_name, mode_name ? mode_name : "Simulacion", sizeof(engine->mode_name) - 1);
    random_source_init(&engine->random, 0);
    text_loader_init(&engine->text_loader);
    text_loader_load_words(&engine->text_loader, engine->config.words_path);
    text_loader_load_phrases(&engine->text_loader, engine->config.phrases_path);
    logger_open(&engine->simulation_log, "simulation.log");
    logger_open(&engine->bcp_log, "bcp.log");
    process_table_init(&engine->table);
    process_table_generate(&engine->table, &engine->config, &engine->random);
    ready_queue_init(&engine->ready_queue);
    io_queue_init(&engine->io_queue, engine->config.io_multipliers);
    scheduler_init(&engine->scheduler, engine->config.quantum_default);
    engine->table.quantum_current = engine->scheduler.current_quantum;
    memory_manager_init(&engine->memory);
    distributed_backend_init(&engine->distributed, distributed_mode);
    if (distributed_backend_start(&engine->distributed) != 0) {
        logger_info(&engine->simulation_log, "No se pudo iniciar backend distribuido");
        return -1;
    }
    engine->running = 1;
    logger_info(&engine->simulation_log, "SimulationEngine inicializado en modo %s", engine->mode_name);
    return 0;
}

void simulation_engine_destroy(SimulationEngine* engine) {
    if (!engine) return;
    distributed_backend_stop(&engine->distributed);
    logger_flush(&engine->simulation_log);
    logger_flush(&engine->bcp_log);
    logger_close(&engine->simulation_log);
    logger_close(&engine->bcp_log);
}

int simulation_engine_cycle(SimulationEngine* engine) {
    if (!engine || !engine->running) return -1;
    engine->table.current_cycle++;
    admit_created_processes(engine);
    admit_new_requests(engine);
    process_io_completions(engine);
    increment_waiting_times(engine);

    int context_switch = 0;
    Process* selected = select_process(engine, &context_switch);
    if (selected) execute_process(engine, selected, context_switch);

    if (engine->table.current_cycle - engine->last_resize_cycle >= engine->config.memory_resize_interval) {
        memory_manager_resize_half_and_double(&engine->memory, &engine->table);
        engine->last_resize_cycle = engine->table.current_cycle;
    }

    scheduler_rebalance_quantum(&engine->scheduler, &engine->table, &engine->ready_queue, &engine->io_queue);
    SchedulerAlgorithm before_algorithm = engine->scheduler.algorithm;
    if (scheduler_auto_switch(&engine->scheduler, &engine->table) &&
        before_algorithm == SCHEDULER_FCFS &&
        engine->scheduler.algorithm == SCHEDULER_RR &&
        engine->current_process &&
        engine->current_process->pcb.state == PROCESS_RUNNING) {
        move_to_ready(engine, engine->current_process, 1);
        engine->current_process = NULL;
    }
    scheduler_update_rankings(&engine->scheduler, &engine->table);
    scheduler_clear_privilege_if_finished(&engine->scheduler, &engine->table);
    memory_manager_update_stats(&engine->memory, &engine->table);
    process_table_update_averages(&engine->table);

    if (engine->table.dispatch_count > 0 &&
        engine->table.dispatch_count % engine->config.distributed_interval == 0 &&
        engine->table.dispatch_count != engine->last_distributed_dispatch) {
        distributed_backend_run_stats_task(&engine->distributed, &engine->table);
        distributed_backend_run_aging_task(&engine->distributed, &engine->table);
        engine->last_distributed_dispatch = engine->table.dispatch_count;
    }

    if (engine->table.current_cycle % 20 == 0) log_table(&engine->simulation_log, &engine->table);
    if (engine->config.max_cycles > 0 && engine->table.current_cycle >= engine->config.max_cycles)
        engine->running = 0;
    if (all_done(engine)) engine->running = 0;
    return 0;
}

int simulation_engine_run(SimulationEngine* engine) {
    if (!engine) return -1;
    memory_manager_update_stats(&engine->memory, &engine->table);
    process_table_update_averages(&engine->table);
    ui_show_dashboard(engine->mode_name, &engine->table, &engine->ready_queue, &engine->io_queue, &engine->scheduler);
    while (engine->running) {
        simulation_engine_cycle(engine);
        if (engine->table.current_cycle % 10 == 0) {
            ui_show_dashboard(engine->mode_name, &engine->table, &engine->ready_queue, &engine->io_queue, &engine->scheduler);
        }
        handle_command(engine, ui_read_command());
        clock_sleep_ms(engine->config.delay_ms);
    }
    distributed_backend_run_stats_task(&engine->distributed, &engine->table);
    distributed_backend_run_aging_task(&engine->distributed, &engine->table);
    log_table(&engine->simulation_log, &engine->table);
    ui_show_dashboard(engine->mode_name, &engine->table, &engine->ready_queue, &engine->io_queue, &engine->scheduler);
    ui_show_distributed_results(&engine->distributed);
    return 0;
}
