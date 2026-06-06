#ifndef DOMAIN_MEMORY_MEMORY_MANAGER_H
#define DOMAIN_MEMORY_MEMORY_MANAGER_H

#include "../../domain/process/process_table.h"
#include "../../infra/random.h"
#include "../../infra/text_loader.h"
#include "../../simulation/config.h"

typedef struct PageRecord {
    int process_slot;
    int page_number;
    int in_memory;
    int frame;
    int swap_slot;
    char words[SIM_PAGE_WORDS][SIM_WORD_LEN];
} PageRecord;

typedef struct SwapRecord {
    int in_use;
    int page_id;
    char words[SIM_PAGE_WORDS][SIM_WORD_LEN];
} SwapRecord;

typedef struct MemoryManager {
    unsigned char bitmap[SIM_TOTAL_FRAMES];
    int frame_page[SIM_TOTAL_FRAMES];
    PageRecord pages[SIM_ACTIVE_SLOTS][SIM_MAX_PAGES_PER_PROCESS];
    SwapRecord swap[SIM_SWAP_SLOTS];
    int fifo[SIM_TOTAL_FRAMES];
    int fifo_head;
    int fifo_tail;
    int fifo_count;
    int page_faults;
    int replacements;
    int swap_operations;
    int used_frames;
    int free_frames;
    int largest_free_run;
    int free_run_count;
    int internal_waste;
    int external_waste;
    float fragmentation;
} MemoryManager;

void memory_manager_init(MemoryManager* memory);
void memory_manager_allocate_process(MemoryManager* memory, Process* process, int slot);
void memory_manager_release_process(MemoryManager* memory, Process* process, int slot);
void memory_manager_resize_process(MemoryManager* memory, Process* process, int slot, int new_page_count);
void memory_manager_resize_half_and_double(MemoryManager* memory, ProcessTable* table);
void memory_manager_access_words(MemoryManager* memory,
                                 Process* process,
                                 int slot,
                                 char words[SIM_PHRASE_WORDS][SIM_WORD_LEN],
                                 int word_count,
                                 TextLoader* loader,
                                 RandomSource* random);
void memory_manager_update_stats(MemoryManager* memory, ProcessTable* table);
int memory_manager_normalize_page_count(int page_count);

#endif
