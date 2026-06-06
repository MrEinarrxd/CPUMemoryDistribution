#include "config.h"

#include <stdlib.h>

static int read_positive_env(const char* name, int fallback) {
    const char* raw = getenv(name);
    if (!raw || raw[0] == '\0') return fallback;
    int value = atoi(raw);
    return value >= 0 ? value : fallback;
}

SimulationConfig simulation_config_default(void) {
    SimulationConfig config;
    config.total_processes = SIM_TOTAL_PROCESSES;
    config.active_slots = SIM_ACTIVE_SLOTS;
    config.new_requests = SIM_NEW_REQUESTS;
    config.ready_capacity = SIM_READY_CAPACITY;
    config.cpu_cycles_min = 0;
    config.cpu_cycles_max = 85000;
    config.cpu_instance_min = 10;
    config.cpu_instance_max = 70;
    config.arrival_min = 0;
    config.arrival_max = 800;
    config.creation_sleep_min = 1;
    config.creation_sleep_max = 50;
    config.context_switch_min = 10;
    config.context_switch_max = 30;
    config.io_time_min = 1;
    config.io_time_max = 100;
    config.io_multipliers[0] = 2;
    config.io_multipliers[1] = 4;
    config.io_multipliers[2] = 8;
    config.io_multipliers[3] = 12;
    config.quantum_default = 20;
    config.rebalance_interval = 20;
    config.rebalance_threshold = 0.75f;
    config.page_words = SIM_PAGE_WORDS;
    config.min_frames_per_process = 8;
    config.max_frames_per_process = 20;
    config.memory_resize_interval = 20;
    config.distributed_interval = 20;
    config.delay_ms = 25;
    config.max_cycles = 0;
    config.words_path = "libro1.odt";
    config.phrases_path = "frases.odt";
    return config;
}

void simulation_config_apply_env(SimulationConfig* config) {
    if (!config) return;
    config->delay_ms = read_positive_env("SIM_DELAY_MS", config->delay_ms);
    config->max_cycles = read_positive_env("SIM_MAX_CYCLES", config->max_cycles);
}
