// === src/domain/memory/memoria.h ===

#ifndef MEMORIA_H
#define MEMORIA_H

#include "../../utils/constants.h"

typedef struct Marco {
    int id;
    int size;
    int free;
    struct Pagina* page;
    char processId[idProcesoLen];  
} Marco;

typedef struct Pagina {
    int id;
    char words[palabrasPorPagina][maxCaracteresPalabra];
    int enMemoria;
    int idMarco;
    int idProceso;
} Pagina;

typedef struct ProcessPageTable {
    Pagina** pages;
    int pageCount;
} ProcessPageTable;

typedef struct PageDirectory {
    Pagina* allPages;
    int totalPages;
    ProcessPageTable processTables[procesosEnEjecucion];
} PageDirectory;

PageDirectory* pageDirectoryCreate(int totalPages);

void pageDirectoryDestroy(PageDirectory* pageDir);

ProcessPageTable* pageDirectoryGetProcessTable(PageDirectory* pageDir, int processIndex);

Pagina* pageDirectoryGetPage(PageDirectory* pageDir, int pageNumber);

void pageDirectorySetPageFrame(PageDirectory* pageDir, int pageNumber, int frameNumber);

int pageDirectoryGetPageFrame(PageDirectory* pageDir, int pageNumber);

void pageDirectoryMarkDirty(PageDirectory* pageDir, int pageNumber);

void pageDirectoryMarkClean(PageDirectory* pageDir, int pageNumber);

Marco* marcoCreate(int id);

void marcoDestroy(Marco* marco);

void marcoSetPage(Marco* marco, Pagina* pagina);

#endif
