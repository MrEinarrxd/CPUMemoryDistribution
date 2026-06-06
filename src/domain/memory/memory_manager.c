#include "memory_manager.h"

#include <string.h>

static int page_id(int slot, int page_number) {
    return slot * SIM_MAX_PAGES_PER_PROCESS + page_number;
}

static PageRecord* page_by_id(MemoryManager* memory, int id) {
    if (!memory || id < 0) return NULL;
    int slot = id / SIM_MAX_PAGES_PER_PROCESS;
    int page = id % SIM_MAX_PAGES_PER_PROCESS;
    if (slot < 0 || slot >= SIM_ACTIVE_SLOTS || page < 0 || page >= SIM_MAX_PAGES_PER_PROCESS) return NULL;
    return &memory->pages[slot][page];
}

static unsigned int word_hash(const char* word) {
    unsigned int hash = 2166136261u;
    if (!word) return hash;
    while (*word) {
        hash ^= (unsigned char)*word++;
        hash *= 16777619u;
    }
    return hash;
}

int memory_manager_normalize_page_count(int page_count) {
    if (page_count < 8) page_count = 8;
    if (page_count > 20) page_count = 20;
    if ((page_count % 2) != 0) page_count++;
    if (page_count > 20) page_count = 20;
    return page_count;
}

void memory_manager_init(MemoryManager* memory) {
    if (!memory) return;
    memset(memory, 0, sizeof(*memory));
    for (int i = 0; i < SIM_TOTAL_FRAMES; i++) {
        memory->frame_page[i] = -1;
        memory->fifo[i] = -1;
    }
    for (int s = 0; s < SIM_ACTIVE_SLOTS; s++) {
        for (int p = 0; p < SIM_MAX_PAGES_PER_PROCESS; p++) {
            memory->pages[s][p].process_slot = s;
            memory->pages[s][p].page_number = p;
            memory->pages[s][p].frame = -1;
            memory->pages[s][p].swap_slot = -1;
        }
    }
}

static void fifo_remove(MemoryManager* memory, int id) {
    if (!memory || memory->fifo_count <= 0) return;
    for (int i = 0; i < memory->fifo_count; i++) {
        int idx = (memory->fifo_head + i) % SIM_TOTAL_FRAMES;
        if (memory->fifo[idx] != id) continue;
        for (int j = i; j < memory->fifo_count - 1; j++) {
            int cur = (memory->fifo_head + j) % SIM_TOTAL_FRAMES;
            int next = (memory->fifo_head + j + 1) % SIM_TOTAL_FRAMES;
            memory->fifo[cur] = memory->fifo[next];
        }
        memory->fifo_tail = (memory->fifo_tail - 1 + SIM_TOTAL_FRAMES) % SIM_TOTAL_FRAMES;
        memory->fifo[memory->fifo_tail] = -1;
        memory->fifo_count--;
        return;
    }
}

static void fifo_push(MemoryManager* memory, int id) {
    if (!memory) return;
    fifo_remove(memory, id);
    if (memory->fifo_count >= SIM_TOTAL_FRAMES) return;
    memory->fifo[memory->fifo_tail] = id;
    memory->fifo_tail = (memory->fifo_tail + 1) % SIM_TOTAL_FRAMES;
    memory->fifo_count++;
}

static int fifo_pop(MemoryManager* memory) {
    if (!memory || memory->fifo_count <= 0) return -1;
    int id = memory->fifo[memory->fifo_head];
    memory->fifo[memory->fifo_head] = -1;
    memory->fifo_head = (memory->fifo_head + 1) % SIM_TOTAL_FRAMES;
    memory->fifo_count--;
    return id;
}

static int find_free_frame(MemoryManager* memory) {
    if (!memory) return -1;
    for (int i = 0; i < SIM_TOTAL_FRAMES; i++) {
        if (!memory->bitmap[i]) return i;
    }
    return -1;
}

static int find_free_swap(MemoryManager* memory) {
    if (!memory) return -1;
    for (int i = 0; i < SIM_SWAP_SLOTS; i++) {
        if (!memory->swap[i].in_use) return i;
    }
    return -1;
}

static void clear_page(PageRecord* page) {
    if (!page) return;
    page->in_memory = 0;
    page->frame = -1;
    page->swap_slot = -1;
    memset(page->words, 0, sizeof(page->words));
}

static int swap_out(MemoryManager* memory, PageRecord* page) {
    int slot = find_free_swap(memory);
    if (slot < 0 || !page) return -1;
    memory->swap[slot].in_use = 1;
    memory->swap[slot].page_id = page_id(page->process_slot, page->page_number);
    memcpy(memory->swap[slot].words, page->words, sizeof(page->words));
    page->swap_slot = slot;
    memory->swap_operations++;
    return 0;
}

static void swap_in(MemoryManager* memory, PageRecord* page) {
    if (!memory || !page || page->swap_slot < 0) return;
    int slot = page->swap_slot;
    if (slot >= SIM_SWAP_SLOTS || !memory->swap[slot].in_use) return;
    memcpy(page->words, memory->swap[slot].words, sizeof(page->words));
    memory->swap[slot].in_use = 0;
    memory->swap[slot].page_id = -1;
    memset(memory->swap[slot].words, 0, sizeof(memory->swap[slot].words));
    page->swap_slot = -1;
    memory->swap_operations++;
}

static void fill_page_words(PageRecord* page, TextLoader* loader, RandomSource* random) {
    if (!page) return;
    for (int i = 0; i < SIM_PAGE_WORDS; i++) {
        if (page->words[i][0] == '\0') {
            const char* word = text_loader_random_word(loader, random);
            strncpy(page->words[i], word, SIM_WORD_LEN - 1);
            page->words[i][SIM_WORD_LEN - 1] = '\0';
        }
    }
}

static void free_frame(MemoryManager* memory, int frame) {
    if (!memory || frame < 0 || frame >= SIM_TOTAL_FRAMES) return;
    memory->bitmap[frame] = 0;
    memory->frame_page[frame] = -1;
}

static int load_page(MemoryManager* memory, PageRecord* page, TextLoader* loader, RandomSource* random) {
    if (!memory || !page) return -1;
    if (page->in_memory && page->frame >= 0) return 0;
    int frame = find_free_frame(memory);
    if (frame < 0) {
        int victim_id = fifo_pop(memory);
        PageRecord* victim = page_by_id(memory, victim_id);
        if (!victim || victim->frame < 0 || swap_out(memory, victim) != 0) return -1;
        frame = victim->frame;
        memory->frame_page[frame] = -1;
        victim->in_memory = 0;
        victim->frame = -1;
        memset(victim->words, 0, sizeof(victim->words));
        memory->replacements++;
    }
    memory->bitmap[frame] = 1;
    memory->frame_page[frame] = page_id(page->process_slot, page->page_number);
    page->in_memory = 1;
    page->frame = frame;
    if (page->swap_slot >= 0) swap_in(memory, page);
    else fill_page_words(page, loader, random);
    fifo_push(memory, memory->frame_page[frame]);
    memory->page_faults++;
    return 0;
}

void memory_manager_allocate_process(MemoryManager* memory, Process* process, int slot) {
    if (!memory || !process || slot < 0 || slot >= SIM_ACTIVE_SLOTS) return;
    int count = memory_manager_normalize_page_count(process->pcb.page_count);
    process->pcb.page_count = count;
    process->pcb.memory_requested = count * SIM_PAGE_WORDS;
    process->pcb.memory_allocated = process->pcb.memory_requested;
    process->pcb.page_table_base = slot * SIM_MAX_PAGES_PER_PROCESS;
    process->pcb.active_slot = slot;
    for (int i = 0; i < SIM_MAX_PAGES_PER_PROCESS; i++) {
        clear_page(&memory->pages[slot][i]);
        memory->pages[slot][i].process_slot = slot;
        memory->pages[slot][i].page_number = i;
    }
}

void memory_manager_release_process(MemoryManager* memory, Process* process, int slot) {
    if (!memory || !process || slot < 0 || slot >= SIM_ACTIVE_SLOTS) return;
    for (int i = 0; i < SIM_MAX_PAGES_PER_PROCESS; i++) {
        PageRecord* page = &memory->pages[slot][i];
        if (page->frame >= 0) free_frame(memory, page->frame);
        if (page->swap_slot >= 0 && page->swap_slot < SIM_SWAP_SLOTS) {
            memory->swap[page->swap_slot].in_use = 0;
            memory->swap[page->swap_slot].page_id = -1;
        }
        fifo_remove(memory, page_id(slot, i));
        clear_page(page);
        page->process_slot = slot;
        page->page_number = i;
    }
    process->pcb.active_slot = -1;
}

void memory_manager_resize_process(MemoryManager* memory, Process* process, int slot, int new_page_count) {
    if (!memory || !process || slot < 0 || slot >= SIM_ACTIVE_SLOTS) return;
    int old_count = process->pcb.page_count;
    int count = memory_manager_normalize_page_count(new_page_count);
    if (count < old_count) {
        for (int i = count; i < old_count && i < SIM_MAX_PAGES_PER_PROCESS; i++) {
            PageRecord* page = &memory->pages[slot][i];
            if (page->frame >= 0) free_frame(memory, page->frame);
            if (page->swap_slot >= 0 && page->swap_slot < SIM_SWAP_SLOTS) memory->swap[page->swap_slot].in_use = 0;
            fifo_remove(memory, page_id(slot, i));
            clear_page(page);
        }
    }
    process->pcb.page_count = count;
    process->pcb.memory_allocated = count * SIM_PAGE_WORDS;
    if (process->pcb.memory_requested > process->pcb.memory_allocated)
        process->pcb.memory_requested = process->pcb.memory_allocated;
}

void memory_manager_resize_half_and_double(MemoryManager* memory, ProcessTable* table) {
    if (!memory || !table) return;
    int slots[SIM_ACTIVE_SLOTS];
    int count = 0;
    for (int i = 0; i < SIM_ACTIVE_SLOTS; i++) {
        Process* process = table->active_slots[i];
        if (process && process->pcb.state != PROCESS_FINISHED && process->pcb.state != PROCESS_NEW) {
            slots[count++] = i;
        }
    }
    if (count < 2) return;
    int half = count / 2;
    for (int i = 0; i < count; i++) {
        Process* process = table->active_slots[slots[i]];
        int current = process->pcb.page_count > 0 ? process->pcb.page_count : 8;
        int next = i < half ? current / 2 : current * 2;
        memory_manager_resize_process(memory, process, slots[i], next);
    }
}

static void add_word_to_page(PageRecord* page, const char* word) {
    if (!page || !word) return;
    for (int i = 0; i < SIM_PAGE_WORDS; i++) {
        if (strncmp(page->words[i], word, SIM_WORD_LEN) == 0) return;
    }
    for (int i = 0; i < SIM_PAGE_WORDS; i++) {
        if (page->words[i][0] == '\0') {
            strncpy(page->words[i], word, SIM_WORD_LEN - 1);
            page->words[i][SIM_WORD_LEN - 1] = '\0';
            return;
        }
    }
}

void memory_manager_access_words(MemoryManager* memory,
                                 Process* process,
                                 int slot,
                                 char words[SIM_PHRASE_WORDS][SIM_WORD_LEN],
                                 int word_count,
                                 TextLoader* loader,
                                 RandomSource* random) {
    if (!memory || !process || slot < 0 || slot >= SIM_ACTIVE_SLOTS) return;
    int page_count = process->pcb.page_count > 0 ? process->pcb.page_count : 1;
    for (int i = 0; i < word_count; i++) {
        if (words[i][0] == '\0') continue;
        int page_number = (int)(word_hash(words[i]) % (unsigned int)page_count);
        PageRecord* page = &memory->pages[slot][page_number];
        if (!page->in_memory && load_page(memory, page, loader, random) == 0) {
            process->pcb.page_faults++;
        }
        add_word_to_page(page, words[i]);
    }
}

void memory_manager_update_stats(MemoryManager* memory, ProcessTable* table) {
    if (!memory || !table) return;
    int used = 0;
    int current_free_run = 0;
    int largest = 0;
    int runs = 0;
    int in_run = 0;
    for (int i = 0; i < SIM_TOTAL_FRAMES; i++) {
        if (memory->bitmap[i]) {
            used++;
            current_free_run = 0;
            in_run = 0;
        } else {
            current_free_run++;
            if (!in_run) {
                runs++;
                in_run = 1;
            }
            if (current_free_run > largest) largest = current_free_run;
        }
    }

    int internal = 0;
    for (int i = 0; i < SIM_ACTIVE_SLOTS; i++) {
        Process* process = table->active_slots[i];
        if (!process || process->pcb.state == PROCESS_FINISHED) continue;
        if (process->pcb.memory_allocated > process->pcb.memory_requested)
            internal += process->pcb.memory_allocated - process->pcb.memory_requested;
    }

    memory->used_frames = used;
    memory->free_frames = SIM_TOTAL_FRAMES - used;
    memory->largest_free_run = largest;
    memory->free_run_count = runs;
    memory->internal_waste = internal;
    memory->external_waste = memory->free_frames - largest;
    memory->fragmentation = memory->free_frames > 0
        ? 1.0f - ((float)largest / (float)memory->free_frames)
        : 0.0f;

    table->memory_used_frames = memory->used_frames;
    table->memory_free_frames = memory->free_frames;
    table->largest_free_run = memory->largest_free_run;
    table->free_run_count = memory->free_run_count;
    table->internal_waste = memory->internal_waste;
    table->external_waste = memory->external_waste;
    table->page_faults = memory->page_faults;
    table->fragmentation = memory->fragmentation;
}
