#ifndef DISTRIBUTED_PROTOCOL_H
#define DISTRIBUTED_PROTOCOL_H

#include "../simulation/config.h"

typedef enum WireMessageType {
    WIRE_STATS_REQUEST = 1,
    WIRE_STATS_RESPONSE = 2,
    WIRE_AGING_REQUEST = 3,
    WIRE_AGING_RESPONSE = 4,
    WIRE_FINISH = 5
} WireMessageType;

typedef struct BcpSummary {
    int pid;
    char process_id[SIM_ID_LEN];
    int state;
    int remaining_cycles;
    int total_cpu_cycles;
    int time_in_execution;
    int times_in_io;
    int wasted_cpu_cycles;
} BcpSummary;

typedef struct RrProcessData {
    int pid;
    char process_id[SIM_ID_LEN];
    int remaining_cycles;
    int total_cpu_cycles;
    int time_in_execution;
    int quantum_assigned;
    int quantum_used;
    int times_returned_to_ready;
    int wasted_cpu_cycles;
    float cpu_waste_ratio;
} RrProcessData;

typedef struct DistributedStats {
    int process_count;
    int active_count;
    int finished_count;
    int waiting_count;
    long total_remaining_cycles;
    long total_assigned_cycles;
    long total_executed_cycles;
    int avg_remaining_cycles;
    int total_io_operations;
    float avg_cpu_utilization;
    char top_wasters_ids[SIM_TOP_N][SIM_ID_LEN];
    int top_wasters_waste[SIM_TOP_N];
    int top_wasters_count;
} DistributedStats;

typedef struct AgingResults {
    char top_aged_ids[SIM_TOP_N][SIM_ID_LEN];
    int top_aged_returns[SIM_TOP_N];
    int top_aged_remaining[SIM_TOP_N];
    int top_aged_count;
    char top_wasters_ids[SIM_TOP_N][SIM_ID_LEN];
    int top_wasters_waste[SIM_TOP_N];
    int top_wasters_count;
    int total_returns_to_ready;
    float avg_cpu_utilization;
} AgingResults;

typedef struct WireMessage {
    int type;
    int payload_size;
    char payload[SIM_WIRE_PAYLOAD_SIZE];
} WireMessage;

void wire_message_init(WireMessage* message, WireMessageType type);
int wire_message_set_payload(WireMessage* message, const void* payload, int payload_size);

#endif
