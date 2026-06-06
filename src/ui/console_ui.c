#include "console_ui.h"

#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

static struct termios g_original_termios;
static int g_raw_enabled = 0;

static void set_normal_mode(void) {
    if (g_raw_enabled) {
        tcsetattr(STDIN_FILENO, TCSANOW, &g_original_termios);
        g_raw_enabled = 0;
    }
}

static void set_raw_mode(void) {
    if (!isatty(STDIN_FILENO) || g_raw_enabled) return;
    tcgetattr(STDIN_FILENO, &g_original_termios);
    struct termios raw = g_original_termios;
    raw.c_lflag &= (tcflag_t)~(ICANON | ECHO);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    g_raw_enabled = 1;
}

void ui_init(void) {
    set_raw_mode();
}

void ui_shutdown(void) {
    set_normal_mode();
}

int ui_show_menu(void) {
    set_normal_mode();
    printf("\033[2J\033[H");
    printf("============================================================\n");
    printf("          Simulador CPU - Memoria - Distribucion PVM\n");
    printf("============================================================\n\n");
    printf("  1. Simulacion completa con PVM real\n");
    printf("  2. Simulacion con PVM simulado\n");
    printf("  3. Prueba PVM real\n\n");
    printf("Seleccione opcion: ");
    fflush(stdout);
    int option = 0;
    if (scanf("%d", &option) != 1) option = 0;
    int ch = 0;
    while ((ch = getchar()) != '\n' && ch != EOF) {}
    set_raw_mode();
    return option;
}

static void print_bar(const char* label, float value) {
    if (value < 0.0f) value = 0.0f;
    if (value > 1.0f) value = 1.0f;
    int width = 28;
    int filled = (int)(value * (float)width);
    printf("%-28s [", label);
    for (int i = 0; i < width; i++) putchar(i < filled ? '#' : '.');
    printf("] %3d%%\n", (int)(value * 100.0f));
}

void ui_show_dashboard(const char* mode_name,
                       const ProcessTable* table,
                       const ReadyQueue* ready_queue,
                       const IoQueue* io_queue,
                       const Scheduler* scheduler) {
    if (!table || !scheduler) return;
    printf("\033[2J\033[H");
    printf("CPUMemoryDistribution | %s | %s | Quantum %d\n",
           mode_name ? mode_name : "Simulacion",
           scheduler_algorithm_name(scheduler->algorithm),
           scheduler->current_quantum);
    printf("Controles: X cambiar algoritmo | A ranking/privilegio | P pausa | Q salir\n\n");
    print_bar("Progreso finalizados", table->total_processes > 0
              ? (float)table->finished_processes / (float)table->total_processes : 0.0f);
    print_bar("Aprovechamiento CPU", table->cpu_utilization);
    print_bar("Desperdicio CPU", table->cpu_waste_ratio);
    printf("\nProcesos y CPU\n");
    printf("  Ciclo actual:                 %d\n", table->current_cycle);
    printf("  Despachos CPU:                %d\n", table->dispatch_count);
    printf("  Procesos finalizados:         %d / %d\n", table->finished_processes, table->total_processes);
    printf("  Procesos activos:             %d\n", process_table_active_count(table));
    printf("  Cola de listos:               %d\n", ready_queue_count(ready_queue));
    printf("  Procesos en E/S:              %d\n", io_queue_total_waiting(io_queue));
    printf("  Prom. finalizados/ciclo:      %.3f\n", table->avg_processes_finished_per_cycle);
    printf("  Prom. espera:                 %.2f\n", table->avg_waiting_time);
    printf("  Prom. ejecucion:              %.2f\n", table->avg_time_in_execution);
    printf("  Cambios contexto:             %d\n", table->total_context_switches);
    printf("  Cambios algoritmo:            %d\n", table->algorithm_change_count);
    printf("  Operaciones E/S:              %d\n", table->total_io_operations);
    printf("\nMemoria\n");
    printf("  Marcos usados/libres:         %d / %d\n", table->memory_used_frames, table->memory_free_frames);
    printf("  Mayor hueco libre:            %d\n", table->largest_free_run);
    printf("  Huecos libres:                %d\n", table->free_run_count);
    printf("  Desperdicio interno:          %d\n", table->internal_waste);
    printf("  Desperdicio externo:          %d\n", table->external_waste);
    printf("  Fallos pagina:                %d\n", table->page_faults);
    printf("  Fragmentacion externa:        %.2f%%\n", table->fragmentation * 100.0f);
    fflush(stdout);
}

int ui_read_command(void) {
    if (!isatty(STDIN_FILENO)) return 0;
    fd_set set;
    struct timeval timeout;
    FD_ZERO(&set);
    FD_SET(STDIN_FILENO, &set);
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    int ready = select(STDIN_FILENO + 1, &set, NULL, NULL, &timeout);
    if (ready <= 0) return 0;
    unsigned char ch = 0;
    if (read(STDIN_FILENO, &ch, 1) != 1) return 0;
    return ch;
}

SchedulerAlgorithm ui_prompt_algorithm(void) {
    set_normal_mode();
    printf("\nSeleccione algoritmo:\n");
    printf("  1. FCFS\n");
    printf("  2. Round Robin\n");
    printf("  0. Cancelar\n");
    printf("Opcion: ");
    fflush(stdout);
    int option = 0;
    if (scanf("%d", &option) != 1) option = 0;
    int ch = 0;
    while ((ch = getchar()) != '\n' && ch != EOF) {}
    set_raw_mode();
    if (option == 1) return SCHEDULER_FCFS;
    if (option == 2) return SCHEDULER_RR;
    return 0;
}

int ui_prompt_quantum(void) {
    set_normal_mode();
    printf("\nIngrese quantum para Round Robin: ");
    fflush(stdout);
    int quantum = 0;
    if (scanf("%d", &quantum) != 1) quantum = 20;
    int ch = 0;
    while ((ch = getchar()) != '\n' && ch != EOF) {}
    set_raw_mode();
    return quantum;
}

int ui_prompt_process_id(char out_id[SIM_ID_LEN]) {
    if (!out_id) return -1;
    set_normal_mode();
    printf("\nIngrese ID del proceso a privilegiar: ");
    fflush(stdout);
    if (!fgets(out_id, SIM_ID_LEN, stdin)) {
        out_id[0] = '\0';
        set_raw_mode();
        return -1;
    }
    out_id[strcspn(out_id, "\r\n")] = '\0';
    set_raw_mode();
    return out_id[0] != '\0' ? 0 : -1;
}

void ui_show_rankings(const Scheduler* scheduler) {
    if (!scheduler) return;
    set_normal_mode();
    printf("\nTop 5 procesos envejecidos\n");
    for (int i = 0; i < scheduler->top_aged_count; i++) {
        printf("  %d. %s (retornos: %d, pendientes: %d)\n", i + 1,
               scheduler->top_aged[i].process_id,
               scheduler->top_aged[i].primary,
               scheduler->top_aged[i].secondary);
    }
    printf("Top 5 procesos con desperdicio CPU\n");
    for (int i = 0; i < scheduler->top_wasters_count; i++) {
        printf("  %d. %s (%d ciclos)\n", i + 1,
               scheduler->top_wasters[i].process_id,
               scheduler->top_wasters[i].primary);
    }
    set_raw_mode();
}

void ui_show_distributed_results(const DistributedBackend* backend) {
    set_normal_mode();
    distributed_backend_print_results(backend);
    set_raw_mode();
}
