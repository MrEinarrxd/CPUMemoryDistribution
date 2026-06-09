#ifndef CpuMemoryPagingH
#define CpuMemoryPagingH

#include "bitmapMemory.h"
#include "../process/bcp.h"
#include "../../data/textLoader.h"

typedef struct Page {
    int processIndex;
    int pageIndex;
    int inMemory;
    int frameIndex;
    int inSwap;
    int swapSlot;
    char words[WordsPerPage][WordLen];
} Page;

typedef struct SwapSlot {
    int used;
    char words[WordsPerPage][WordLen];
} SwapSlot;

typedef struct PageRef {
    int processIndex;
    int pageIndex;
} PageRef;

typedef struct PagingSystem {
    Bcp* processes;
    BitmapMemory bitmap;
    Page pages[TotalProcesses][MaxPagesPerProcess];
    SwapSlot swap[SwapPageCapacity];
    PageRef fifo[PhysicalFrameCount];
    int fifoHead;
    int fifoTail;
    int fifoCount;
    int pageFaults;
    int swapIns;
    int swapOuts;
    int replacements;
    int internalWaste;
} PagingSystem;

void pagingSystemInit(PagingSystem* paging, Bcp processes[]);
void pagingSystemTouchProcess(PagingSystem* paging, Bcp* bcp, int processIndex, TextRepository* repo);
int pagingSystemAccessPhrase(PagingSystem* paging, Bcp* bcp, int processIndex, const char* phrase, TextRepository* repo);
void pagingSystemResizeActive(PagingSystem* paging, int activeSlots[], Bcp processes[], int currentTime);
void pagingSystemDeallocateProcess(PagingSystem* paging, int processIndex);

#endif
