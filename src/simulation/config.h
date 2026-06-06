#ifndef SIMULATION_CONFIG_H
#define SIMULATION_CONFIG_H

#define SIM_TOTAL_PROCESSES 250
#define SIM_ACTIVE_SLOTS 150
#define SIM_NEW_REQUESTS 100
#define SIM_READY_CAPACITY 300
#define SIM_IO_DEVICES 4
#define SIM_TOP_N 5
#define SIM_ID_LEN 16
#define SIM_WORD_LEN 32
#define SIM_PHRASE_WORDS 5
#define SIM_PHRASE_LEN 256
#define SIM_PAGE_WORDS 20
#define SIM_MAX_PAGES_PER_PROCESS 32
#define SIM_TOTAL_FRAMES 512
#define SIM_SWAP_SLOTS (SIM_TOTAL_FRAMES * 4)
#define SIM_GROWTH_LIST_SIZE 20
#define SIM_DISTRIBUTED_SLAVES 2
#define SIM_WIRE_PAYLOAD_SIZE 32768

typedef struct SimulationConfig {
    int total_processes;
    int active_slots;
    int new_requests;
    int ready_capacity;
    int cpu_cycles_min;
    int cpu_cycles_max;
    int cpu_instance_min;
    int cpu_instance_max;
    int arrival_min;
    int arrival_max;
    int creation_sleep_min;
    int creation_sleep_max;
    int context_switch_min;
    int context_switch_max;
    int io_time_min;
    int io_time_max;
    int io_multipliers[SIM_IO_DEVICES];
    int quantum_default;
    int rebalance_interval;
    float rebalance_threshold;
    int page_words;
    int min_frames_per_process;
    int max_frames_per_process;
    int memory_resize_interval;
    int distributed_interval;
    int delay_ms;
    int max_cycles;
    const char* words_path;
    const char* phrases_path;
} SimulationConfig;

SimulationConfig simulation_config_default(void);
void simulation_config_apply_env(SimulationConfig* config);

#endif
