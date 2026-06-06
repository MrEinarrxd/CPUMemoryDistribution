#include "pvm_backend.h"

#include "distributed_tasks.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef pvmModeEnabled
#define pvmModeEnabled 1
#endif

#if pvmModeEnabled
#include <pvm3.h>
#include <sys/time.h>
#endif

static void build_summary_rows(ProcessTable* table, BcpSummary rows[SIM_TOTAL_PROCESSES]) {
    for (int i = 0; i < SIM_TOTAL_PROCESSES; i++) {
        Pcb* pcb = &table->processes[i].pcb;
        rows[i].pid = pcb->pid;
        strncpy(rows[i].process_id, pcb->process_id, SIM_ID_LEN - 1);
        rows[i].state = (int)pcb->state;
        rows[i].remaining_cycles = pcb->remaining_cycles;
        rows[i].total_cpu_cycles = pcb->total_cpu_cycles;
        rows[i].time_in_execution = pcb->time_in_execution;
        rows[i].times_in_io = pcb->times_in_io;
        rows[i].wasted_cpu_cycles = pcb->wasted_cpu_cycles;
    }
}

static void build_rr_rows(ProcessTable* table, RrProcessData rows[SIM_TOTAL_PROCESSES]) {
    for (int i = 0; i < SIM_TOTAL_PROCESSES; i++) {
        Pcb* pcb = &table->processes[i].pcb;
        rows[i].pid = pcb->pid;
        strncpy(rows[i].process_id, pcb->process_id, SIM_ID_LEN - 1);
        rows[i].remaining_cycles = pcb->remaining_cycles;
        rows[i].total_cpu_cycles = pcb->total_cpu_cycles;
        rows[i].time_in_execution = pcb->time_in_execution;
        rows[i].quantum_assigned = pcb->quantum_assigned;
        rows[i].quantum_used = pcb->quantum_used;
        rows[i].times_returned_to_ready = pcb->times_returned_to_ready;
        rows[i].wasted_cpu_cycles = pcb->wasted_cpu_cycles;
        rows[i].cpu_waste_ratio = pcb->cpu_waste_ratio;
    }
}

#if pvmModeEnabled
static int send_wire(int tid, const WireMessage* message) {
    pvm_initsend(PvmDataDefault);
    pvm_pkbyte((char*)message, (int)sizeof(*message), 1);
    return pvm_send(tid, message->type);
}

static int receive_wire(WireMessage* out_message) {
    struct timeval timeout = {5, 0};
    int received = pvm_trecv(-1, -1, &timeout);
    if (received <= 0) return -1;
    pvm_upkbyte((char*)out_message, (int)sizeof(*out_message), 1);
    return 0;
}
#endif

int pvm_backend_start(DistributedBackend* backend) {
    if (!backend) return -1;
#if pvmModeEnabled
    backend->master_tid = pvm_mytid();
    if (backend->master_tid < 0) return -1;
    const char* slave_exec = getenv("PVM_SLAVE_EXEC");
    if (!slave_exec || slave_exec[0] == '\0') slave_exec = "simSlave";
    const char* hosts = getenv("PVM_SLAVE_HOSTS");
    int spawned = 0;
    if (!hosts || hosts[0] == '\0') {
        spawned = pvm_spawn((char*)slave_exec, NULL, 0, "", SIM_DISTRIBUTED_SLAVES, backend->slave_tids);
    } else {
        char copy[256];
        strncpy(copy, hosts, sizeof(copy) - 1);
        copy[sizeof(copy) - 1] = '\0';
        char* host = strtok(copy, ",");
        while (host && spawned < SIM_DISTRIBUTED_SLAVES) {
            while (*host == ' ' || *host == '\t') host++;
            int tid = 0;
            if (pvm_spawn((char*)slave_exec, NULL, PvmTaskHost, host, 1, &tid) == 1)
                backend->slave_tids[spawned++] = tid;
            host = strtok(NULL, ",");
        }
    }
    backend->started = spawned == SIM_DISTRIBUTED_SLAVES;
    return backend->started ? 0 : -1;
#else
    (void)backend;
    return -1;
#endif
}

int pvm_backend_run_stats_task(DistributedBackend* backend, ProcessTable* table) {
    if (!backend || !table) return -1;
#if pvmModeEnabled
    BcpSummary rows[SIM_TOTAL_PROCESSES];
    build_summary_rows(table, rows);
    int base = SIM_TOTAL_PROCESSES / SIM_DISTRIBUTED_SLAVES;
    int rem = SIM_TOTAL_PROCESSES % SIM_DISTRIBUTED_SLAVES;
    int start = 0;
    for (int i = 0; i < SIM_DISTRIBUTED_SLAVES; i++) {
        int count = base + (i < rem ? 1 : 0);
        WireMessage message;
        wire_message_init(&message, WIRE_STATS_REQUEST);
        wire_message_set_payload(&message, &rows[start], (int)(sizeof(BcpSummary) * (size_t)count));
        send_wire(backend->slave_tids[i], &message);
        start += count;
    }
    for (int i = 0; i < SIM_DISTRIBUTED_SLAVES; i++) {
        WireMessage response;
        if (receive_wire(&response) == 0 && response.type == WIRE_STATS_RESPONSE)
            memcpy(&backend->partial_stats[i], response.payload, sizeof(DistributedStats));
    }
    distributed_integrate_stats(backend->partial_stats, SIM_DISTRIBUTED_SLAVES, &backend->stats);
    return 0;
#else
    (void)backend;
    (void)table;
    return -1;
#endif
}

int pvm_backend_run_aging_task(DistributedBackend* backend, ProcessTable* table) {
    if (!backend || !table) return -1;
#if pvmModeEnabled
    RrProcessData rows[SIM_TOTAL_PROCESSES];
    build_rr_rows(table, rows);
    int base = SIM_TOTAL_PROCESSES / SIM_DISTRIBUTED_SLAVES;
    int rem = SIM_TOTAL_PROCESSES % SIM_DISTRIBUTED_SLAVES;
    int start = 0;
    for (int i = 0; i < SIM_DISTRIBUTED_SLAVES; i++) {
        int count = base + (i < rem ? 1 : 0);
        WireMessage message;
        wire_message_init(&message, WIRE_AGING_REQUEST);
        wire_message_set_payload(&message, &rows[start], (int)(sizeof(RrProcessData) * (size_t)count));
        send_wire(backend->slave_tids[i], &message);
        start += count;
    }
    for (int i = 0; i < SIM_DISTRIBUTED_SLAVES; i++) {
        WireMessage response;
        if (receive_wire(&response) == 0 && response.type == WIRE_AGING_RESPONSE)
            memcpy(&backend->partial_aging[i], response.payload, sizeof(AgingResults));
    }
    distributed_integrate_aging(backend->partial_aging, SIM_DISTRIBUTED_SLAVES, &backend->aging);
    return 0;
#else
    (void)backend;
    (void)table;
    return -1;
#endif
}

void pvm_backend_stop(DistributedBackend* backend) {
    if (!backend) return;
#if pvmModeEnabled
    WireMessage message;
    wire_message_init(&message, WIRE_FINISH);
    for (int i = 0; i < SIM_DISTRIBUTED_SLAVES; i++) {
        if (backend->slave_tids[i] > 0) send_wire(backend->slave_tids[i], &message);
    }
    pvm_exit();
#else
    (void)backend;
#endif
}
