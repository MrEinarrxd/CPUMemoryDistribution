#include "memoria.h"
#include <stdlib.h>
#include <string.h>

PageDirectory* pageDirectoryCreate(int totalPages) {
    PageDirectory* pd = (PageDirectory*)calloc(1, sizeof(PageDirectory));
    if (!pd) return NULL;
    pd->totalPages = totalPages;
    pd->allPages = (Pagina*)calloc(totalPages, sizeof(Pagina));
    if (!pd->allPages) { free(pd); return NULL; }
    for (int i = 0; i < totalPages; i++) {
        pd->allPages[i].id = i;
        pd->allPages[i].enMemoria = 0;
        pd->allPages[i].idMarco = -1;
        pd->allPages[i].idProceso = -1;
    }
    for (int i = 0; i < procesosEnEjecucion; i++)
        pd->processTables[i].pages = NULL, pd->processTables[i].pageCount = 0;
    return pd;
}

void pageDirectoryDestroy(PageDirectory* pageDir) {
    if (pageDir) {
        free(pageDir->allPages);
        for (int i = 0; i < procesosEnEjecucion; i++)
            if (pageDir->processTables[i].pages) free(pageDir->processTables[i].pages);
        free(pageDir);
    }
}

ProcessPageTable* pageDirectoryGetProcessTable(PageDirectory* pageDir, int processIndex) {
    if (!pageDir || processIndex < 0 || processIndex >= procesosEnEjecucion) return NULL;
    return &pageDir->processTables[processIndex];
}

Pagina* pageDirectoryGetPage(PageDirectory* pageDir, int pageNumber) {
    if (!pageDir || pageNumber < 0 || pageNumber >= pageDir->totalPages) return NULL;
    return &pageDir->allPages[pageNumber];
}

void pageDirectorySetPageFrame(PageDirectory* pageDir, int pageNumber, int frameNumber) {
    if (!pageDir || pageNumber < 0 || pageNumber >= pageDir->totalPages) return;
    pageDir->allPages[pageNumber].idMarco = frameNumber;
    pageDir->allPages[pageNumber].enMemoria = (frameNumber != -1);
}
int pageDirectoryGetPageFrame(PageDirectory* pageDir, int pageNumber) {
    if (!pageDir || pageNumber < 0 || pageNumber >= pageDir->totalPages) return -1;
    return pageDir->allPages[pageNumber].idMarco;
}
void pageDirectoryMarkDirty(PageDirectory* pageDir, int pageNumber) { (void)pageDir; (void)pageNumber; }
void pageDirectoryMarkClean(PageDirectory* pageDir, int pageNumber) { (void)pageDir; (void)pageNumber; }

Marco* marcoCreate(int id) {
    Marco* m = (Marco*)calloc(1, sizeof(Marco));
    if (m) m->id = id, m->free = 1;
    return m;
}
void marcoDestroy(Marco* marco) { free(marco); }
void marcoSetPage(Marco* marco, Pagina* pagina) {
    if (marco) { marco->page = pagina; marco->free = (pagina == NULL); }
}
