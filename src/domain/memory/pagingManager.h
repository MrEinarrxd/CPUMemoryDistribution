// === src/domain/memory/pagingManager.h ===

#ifndef PAGING_MANAGER_H
#define PAGING_MANAGER_H

#include "../../utils/constants.h"

struct ProcessTable;
struct PageDirectory;
struct FifoReplacement;
struct BitmapManager;

// Propietario exclusivo de PageDirectory
typedef struct PagingManager {
    struct PageDirectory* pageDirectory;
    struct BitmapManager* bitmapManager;
    int totalPageFaults;
    int internalWaste;
    int externalWaste;
    struct FifoReplacement* fifoReplacement;            // Manejador de reemplazo FIFO para SWAP
    int frameCountPerProcess[procesosEnEjecucion];      // Contador de marcos por proceso para redimensionar
} PagingManager;

PagingManager* pagingManagerCreate(int totalPages, struct BitmapManager* bitmapManager);

void pagingManagerDestroy(PagingManager* manager);

void pagingManagerSetBitmapManager(PagingManager* manager, struct BitmapManager* bitmapManager);

int pagingManagerHandlePageFault(PagingManager* manager, int processIndex, int pageNumber, const char* missingWord);
// Maneja fallo de página: trae página faltante de SWAP usando FIFO

int pagingManagerAllocatePageForProcess(PagingManager* manager, int processIndex, int pageCount);

int pagingManagerDeallocatePagesForProcess(PagingManager* manager, int processIndex);

int pagingManagerGetPageFaultCount(PagingManager* manager);

int pagingManagerGetInternalWaste(PagingManager* manager);

int pagingManagerGetExternalWaste(PagingManager* manager);

// Establece el manejador FIFO de reemplazo para operaciones de SWAP
void pagingManagerSetReplacement(PagingManager* manager, struct FifoReplacement* fifo);

// Redimensiona la asignación de marcos para un proceso
int pagingManagerResizeFrames(PagingManager* manager, int processIndex, int newFrameCount);

#endif
