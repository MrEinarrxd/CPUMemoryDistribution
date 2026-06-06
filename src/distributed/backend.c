#include "backend.h"

#include "fake_backend.h"
#include "pvm_backend.h"
#include <stdio.h>
#include <string.h>

void distributed_backend_init(DistributedBackend* backend, DistributedMode mode) {
    if (!backend) return;
    memset(backend, 0, sizeof(*backend));
    backend->mode = mode;
}

int distributed_backend_start(DistributedBackend* backend) {
    if (!backend) return -1;
    if (backend->mode == DISTRIBUTED_FAKE) {
        backend->started = 1;
        return 0;
    }
    return pvm_backend_start(backend);
}

int distributed_backend_run_stats_task(DistributedBackend* backend, ProcessTable* table) {
    if (!backend || !table) return -1;
    if (backend->mode == DISTRIBUTED_FAKE) return fake_backend_run_stats_task(backend, table);
    return pvm_backend_run_stats_task(backend, table);
}

int distributed_backend_run_aging_task(DistributedBackend* backend, ProcessTable* table) {
    if (!backend || !table) return -1;
    if (backend->mode == DISTRIBUTED_FAKE) return fake_backend_run_aging_task(backend, table);
    return pvm_backend_run_aging_task(backend, table);
}

void distributed_backend_print_results(const DistributedBackend* backend) {
    if (!backend) return;
    printf("\n=== RESULTADOS DISTRIBUIDOS: TAREA 1 ===\n");
    printf("Procesos analizados: %d\n", backend->stats.process_count);
    printf("Procesos finalizados: %d\n", backend->stats.finished_count);
    printf("Procesos en espera/E/S: %d\n", backend->stats.waiting_count);
    printf("Promedio ciclos pendientes: %d\n", backend->stats.avg_remaining_cycles);
    printf("Operaciones E/S: %d\n", backend->stats.total_io_operations);
    printf("Aprovechamiento CPU promedio: %.2f\n", backend->stats.avg_cpu_utilization);
    printf("Top desperdicio CPU:\n");
    for (int i = 0; i < backend->stats.top_wasters_count; i++) {
        printf("  %d. %s (%d ciclos)\n", i + 1,
               backend->stats.top_wasters_ids[i],
               backend->stats.top_wasters_waste[i]);
    }
    printf("=== RESULTADOS DISTRIBUIDOS: TAREA 2 ===\n");
    printf("Top envejecidos:\n");
    for (int i = 0; i < backend->aging.top_aged_count; i++) {
        printf("  %d. %s (retornos: %d, pendientes: %d)\n", i + 1,
               backend->aging.top_aged_ids[i],
               backend->aging.top_aged_returns[i],
               backend->aging.top_aged_remaining[i]);
    }
    printf("Top desperdicio RR:\n");
    for (int i = 0; i < backend->aging.top_wasters_count; i++) {
        printf("  %d. %s (%d ciclos)\n", i + 1,
               backend->aging.top_wasters_ids[i],
               backend->aging.top_wasters_waste[i]);
    }
    printf("Retornos totales a Listos: %d\n", backend->aging.total_returns_to_ready);
    printf("Aprovechamiento CPU promedio RR: %.2f\n\n", backend->aging.avg_cpu_utilization);
    fflush(stdout);
}

void distributed_backend_stop(DistributedBackend* backend) {
    if (!backend || !backend->started) return;
    if (backend->mode == DISTRIBUTED_REAL_PVM) pvm_backend_stop(backend);
    backend->started = 0;
}
