#include "paging.h"
#include "../../utils/randomUtils.h"

#include <ctype.h>
#include <string.h>

static int normalizePageCount(int count) {
    if (count < MinPageFrames) count = MinPageFrames;
    if (count > MaxPageFrames) count = MaxPageFrames;
    if ((count % 2) != 0) count++;
    if (count > MaxPageFrames) count = MaxPageFrames;
    return count;
}

static unsigned int wordHash(const char* word) {
    unsigned int hash = 5381u;
    if (!word) return hash;
    while (*word) {
        hash = ((hash << 5) + hash) ^ (unsigned char)*word++;
    }
    return hash;
}

static void pageFillRandom(Page* page, TextRepository* repo) {
    if (!page) return;
    for (int i = 0; i < WordsPerPage; ++i) {
        const char* word = textRepositoryRandomWord(repo);
        if (!word) {
            page->words[i][0] = '\0';
            continue;
        }
        strncpy(page->words[i], word, WordLen - 1);
        page->words[i][WordLen - 1] = '\0';
    }
}

static int pageContainsWord(const Page* page, const char* word) {
    if (!page || !word) return 0;
    for (int i = 0; i < WordsPerPage; ++i) {
        if (strcmp(page->words[i], word) == 0) return 1;
    }
    return 0;
}

static void pageStoreWord(Page* page, const char* word) {
    int slot;
    if (!page || !word || word[0] == '\0') return;
    slot = (int)(wordHash(word) % WordsPerPage);
    strncpy(page->words[slot], word, WordLen - 1);
    page->words[slot][WordLen - 1] = '\0';
}

static int swapAlloc(PagingSystem* paging) {
    if (!paging) return -1;
    for (int i = 0; i < SwapPageCapacity; ++i) {
        if (!paging->swap[i].used) {
            paging->swap[i].used = 1;
            return i;
        }
    }
    return -1;
}

static int fifoRefIsLive(PagingSystem* paging, PageRef ref) {
    Page* page;
    if (!paging) return 0;
    if (ref.processIndex < 0 || ref.processIndex >= TotalProcesses ||
        ref.pageIndex < 0 || ref.pageIndex >= MaxPagesPerProcess) {
        return 0;
    }
    page = &paging->pages[ref.processIndex][ref.pageIndex];
    return page->inMemory && page->frameIndex >= 0;
}

static void fifoCompact(PagingSystem* paging) {
    PageRef refs[PhysicalFrameCount];
    int originalCount;
    int kept = 0;

    if (!paging) return;
    originalCount = paging->fifoCount;
    for (int i = 0; i < originalCount; ++i) {
        PageRef ref;
        ref = paging->fifo[paging->fifoHead];
        paging->fifoHead = (paging->fifoHead + 1) % PhysicalFrameCount;
        paging->fifoCount--;
        if (fifoRefIsLive(paging, ref)) {
            refs[kept++] = ref;
        }
    }

    paging->fifoHead = 0;
    paging->fifoTail = 0;
    paging->fifoCount = 0;
    for (int i = 0; i < kept; ++i) {
        paging->fifo[paging->fifoTail] = refs[i];
        paging->fifoTail = (paging->fifoTail + 1) % PhysicalFrameCount;
        paging->fifoCount++;
    }
}

static void fifoRemove(PagingSystem* paging, int processIndex, int pageIndex) {
    PageRef refs[PhysicalFrameCount];
    int originalCount;
    int kept = 0;

    if (!paging) return;
    originalCount = paging->fifoCount;
    for (int i = 0; i < originalCount; ++i) {
        PageRef ref;
        ref = paging->fifo[paging->fifoHead];
        paging->fifoHead = (paging->fifoHead + 1) % PhysicalFrameCount;
        paging->fifoCount--;
        if (ref.processIndex == processIndex && ref.pageIndex == pageIndex) {
            continue;
        }
        if (fifoRefIsLive(paging, ref)) {
            refs[kept++] = ref;
        }
    }

    paging->fifoHead = 0;
    paging->fifoTail = 0;
    paging->fifoCount = 0;
    for (int i = 0; i < kept; ++i) {
        paging->fifo[paging->fifoTail] = refs[i];
        paging->fifoTail = (paging->fifoTail + 1) % PhysicalFrameCount;
        paging->fifoCount++;
    }
}

static int fifoPush(PagingSystem* paging, int processIndex, int pageIndex) {
    if (!paging) return -1;
    if (paging->fifoCount >= PhysicalFrameCount) fifoCompact(paging);
    if (paging->fifoCount >= PhysicalFrameCount) return -1;
    paging->fifo[paging->fifoTail].processIndex = processIndex;
    paging->fifo[paging->fifoTail].pageIndex = pageIndex;
    paging->fifoTail = (paging->fifoTail + 1) % PhysicalFrameCount;
    paging->fifoCount++;
    return 0;
}

static PageRef fifoPop(PagingSystem* paging) {
    PageRef ref = {-1, -1};
    if (!paging || paging->fifoCount <= 0) return ref;
    ref = paging->fifo[paging->fifoHead];
    paging->fifoHead = (paging->fifoHead + 1) % PhysicalFrameCount;
    paging->fifoCount--;
    return ref;
}

static int evictFifoPage(PagingSystem* paging, Bcp* processes) {
    PageRef ref;
    Page* page;
    int slot;

    if (!paging) return -1;
    while (paging->fifoCount > 0) {
        ref = fifoPop(paging);
        if (ref.processIndex < 0 || ref.processIndex >= TotalProcesses ||
            ref.pageIndex < 0 || ref.pageIndex >= MaxPagesPerProcess) {
            continue;
        }
        page = &paging->pages[ref.processIndex][ref.pageIndex];
        if (!page->inMemory || page->frameIndex < 0) continue;
        slot = swapAlloc(paging);
        if (slot < 0) return -1;
        memcpy(paging->swap[slot].words, page->words, sizeof(page->words));
        page->inSwap = 1;
        page->swapSlot = slot;
        page->inMemory = 0;
        bitmapMemoryFreeFrame(&paging->bitmap, page->frameIndex);
        page->frameIndex = -1;
        /* Reemplazo FIFO: se expulsa la pagina mas antigua registrada en memoria. */
        paging->swapOuts++;
        paging->replacements++;
        if (processes) processes[ref.processIndex].swapOuts++;
        return 0;
    }
    return -1;
}

static int loadPage(PagingSystem* paging, Bcp* bcp, int processIndex, int pageIndex, TextRepository* repo) {
    Page* page;
    int frame;

    if (!paging || !bcp) return -1;
    if (processIndex < 0 || processIndex >= TotalProcesses) return -1;
    if (pageIndex < 0 || pageIndex >= bcp->pageCount) return -1;

    page = &paging->pages[processIndex][pageIndex];
    if (page->inMemory) return 0;

    paging->pageFaults++;
    bcp->pageFaults++;
    frame = bitmapMemoryAllocateFrame(&paging->bitmap, processIndex, pageIndex);
    if (frame < 0) {
        if (evictFifoPage(paging, paging->processes) != 0) return -1;
        frame = bitmapMemoryAllocateFrame(&paging->bitmap, processIndex, pageIndex);
        if (frame < 0) return -1;
    }

    if (page->inSwap && page->swapSlot >= 0) {
        memcpy(page->words, paging->swap[page->swapSlot].words, sizeof(page->words));
        paging->swap[page->swapSlot].used = 0;
        page->inSwap = 0;
        page->swapSlot = -1;
        paging->swapIns++;
        bcp->swapIns++;
    } else {
        pageFillRandom(page, repo);
    }

    page->inMemory = 1;
    page->frameIndex = frame;
    if (fifoPush(paging, processIndex, pageIndex) != 0) {
        bitmapMemoryFreeFrame(&paging->bitmap, frame);
        page->inMemory = 0;
        page->frameIndex = -1;
        return -1;
    }
    return 0;
}

static void clearPage(PagingSystem* paging, Page* page) {
    if (!paging || !page) return;
    fifoRemove(paging, page->processIndex, page->pageIndex);
    if (page->inMemory && page->frameIndex >= 0) {
        bitmapMemoryFreeFrame(&paging->bitmap, page->frameIndex);
    }
    if (page->inSwap && page->swapSlot >= 0) {
        paging->swap[page->swapSlot].used = 0;
    }
    page->inMemory = 0;
    page->frameIndex = -1;
    page->inSwap = 0;
    page->swapSlot = -1;
    memset(page->words, 0, sizeof(page->words));
}

static void normalizeWord(char* token) {
    int readPos = 0;
    int writePos = 0;
    while (token[readPos] != '\0') {
        unsigned char ch = (unsigned char)token[readPos++];
        if (isalnum(ch) || ch == '_' || ch == '-') {
            token[writePos++] = (char)tolower(ch);
        }
    }
    token[writePos] = '\0';
}

static void pagingSystemInitProcess(PagingSystem* paging, int processIndex);

void pagingSystemInit(PagingSystem* paging, Bcp processes[]) {
    if (!paging) return;
    memset(paging, 0, sizeof(*paging));
    paging->processes = processes;
    bitmapMemoryInit(&paging->bitmap);
    for (int p = 0; p < TotalProcesses; ++p) {
        pagingSystemInitProcess(paging, p);
    }
}

static void pagingSystemInitProcess(PagingSystem* paging, int processIndex) {
    if (!paging || processIndex < 0 || processIndex >= TotalProcesses) return;
    for (int page = 0; page < MaxPagesPerProcess; ++page) {
        paging->pages[processIndex][page].processIndex = processIndex;
        paging->pages[processIndex][page].pageIndex = page;
        paging->pages[processIndex][page].inMemory = 0;
        paging->pages[processIndex][page].frameIndex = -1;
        paging->pages[processIndex][page].inSwap = 0;
        paging->pages[processIndex][page].swapSlot = -1;
        memset(paging->pages[processIndex][page].words, 0,
               sizeof(paging->pages[processIndex][page].words));
    }
}

void pagingSystemTouchProcess(PagingSystem* paging, Bcp* bcp, int processIndex, TextRepository* repo) {
    int growth;
    int oldWords;
    int maxWords;
    int requiredPages;

    if (!paging || !bcp || bcp->state == processStateFinished) return;
    growth = randomMemoryGrowth();
    bcp->memoryRequested = growth;
    oldWords = bcp->totalMemoryAllocated;

    if (growth <= 0) return;
    maxWords = bcp->pageCount * WordsPerPage;
    bcp->totalMemoryAllocated += growth;
    if (bcp->totalMemoryAllocated > maxWords) {
        bcp->totalMemoryAllocated = maxWords;
    }
    if (bcp->totalMemoryAllocated <= oldWords) return;
    requiredPages = (bcp->totalMemoryAllocated + WordsPerPage - 1) / WordsPerPage;
    if (requiredPages > bcp->pageCount) requiredPages = bcp->pageCount;
    for (int page = oldWords / WordsPerPage; page < requiredPages; ++page) {
        if (loadPage(paging, bcp, processIndex, page, repo) == 0) {
            pageStoreWord(&paging->pages[processIndex][page], textRepositoryRandomWord(repo));
        }
    }
}

int pagingSystemAccessPhrase(PagingSystem* paging, Bcp* bcp, int processIndex, const char* phrase, TextRepository* repo) {
    char local[PhraseLen];
    char* token;
    int faultsBefore;

    if (!paging || !bcp || !phrase) return 0;
    strncpy(local, phrase, sizeof(local) - 1);
    local[sizeof(local) - 1] = '\0';
    strncpy(bcp->ioPhrase, phrase, sizeof(bcp->ioPhrase) - 1);
    bcp->ioPhrase[sizeof(bcp->ioPhrase) - 1] = '\0';

    faultsBefore = paging->pageFaults;
    token = strtok(local, " \t\r\n,.!?;:");
    while (token) {
        int found = 0;
        char word[WordLen];
        strncpy(word, token, sizeof(word) - 1);
        word[sizeof(word) - 1] = '\0';
        normalizeWord(word);
        if (word[0] != '\0') {
            for (int page = 0; page < bcp->pageCount; ++page) {
                Page* current = &paging->pages[processIndex][page];
                if (current->inMemory && pageContainsWord(current, word)) {
                    found = 1;
                    break;
                }
            }
            if (!found) {
                int targetPage = (int)(wordHash(word) % (unsigned int)bcp->pageCount);
                if (loadPage(paging, bcp, processIndex, targetPage, repo) == 0) {
                    pageStoreWord(&paging->pages[processIndex][targetPage], word);
                }
            }
        }
        token = strtok(NULL, " \t\r\n,.!?;:");
    }

    return paging->pageFaults - faultsBefore;
}

void pagingSystemResizeActive(PagingSystem* paging, int activeSlots[], Bcp processes[], int currentTime) {
    int active[ActiveProcessCount];
    int activeCount = 0;

    if (!paging || !activeSlots || !processes) return;
    for (int i = 0; i < ActiveProcessCount; ++i) {
        int index = activeSlots[i];
        if (index >= 0 && processes[index].state != processStateFinished) {
            active[activeCount++] = index;
        }
    }
    if (activeCount < 2) return;
    for (int i = 0; i < activeCount; ++i) {
        int index = active[i];
        int oldCount = processes[index].pageCount;
        int newCount = (i < activeCount / 2)
            ? normalizePageCount(oldCount / 2)
            : normalizePageCount(oldCount * 2);
        if (newCount < oldCount) {
            for (int page = newCount; page < oldCount; ++page) {
                clearPage(paging, &paging->pages[index][page]);
            }
        }
        processes[index].pageCount = newCount;
        if (processes[index].totalMemoryAllocated > newCount * WordsPerPage) {
            processes[index].totalMemoryAllocated = newCount * WordsPerPage;
        }
        (void)currentTime;
    }
}

void pagingSystemDeallocateProcess(PagingSystem* paging, int processIndex) {
    if (!paging || processIndex < 0 || processIndex >= TotalProcesses) return;
    for (int page = 0; page < MaxPagesPerProcess; ++page) {
        clearPage(paging, &paging->pages[processIndex][page]);
    }
}
