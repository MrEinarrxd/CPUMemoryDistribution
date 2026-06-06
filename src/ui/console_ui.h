#ifndef UI_CONSOLE_UI_H
#define UI_CONSOLE_UI_H

#include "../distributed/backend.h"
#include "../domain/process/io_queue.h"
#include "../domain/process/process_table.h"
#include "../domain/process/ready_queue.h"
#include "../domain/scheduling/scheduler.h"

void ui_init(void);
void ui_shutdown(void);
int ui_show_menu(void);
void ui_show_dashboard(const char* mode_name,
                       const ProcessTable* table,
                       const ReadyQueue* ready_queue,
                       const IoQueue* io_queue,
                       const Scheduler* scheduler);
int ui_read_command(void);
SchedulerAlgorithm ui_prompt_algorithm(void);
int ui_prompt_quantum(void);
int ui_prompt_process_id(char out_id[SIM_ID_LEN]);
void ui_show_rankings(const Scheduler* scheduler);
void ui_show_distributed_results(const DistributedBackend* backend);

#endif
