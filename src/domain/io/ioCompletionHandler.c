#include "ioCompletionHandler.h"
#include "ioQueue.h"
#include "../scheduler/readyQueue.h"
#include "../process/bcp.h"
#include "../process/process.h"
#include "../process/processTable.h"
#include "../memory/pagingManager.h"
#include "../memory/memoria.h"
#include "../../utils/constants.h"
#include <stdlib.h>
#include <string.h>

IoCompletionHandler* ioCompletionHandlerCreate(void) {
    IoCompletionHandler* h = (IoCompletionHandler*)calloc(1, sizeof(IoCompletionHandler));
    return h;
}
void ioCompletionHandlerDestroy(IoCompletionHandler* handler) { free(handler); }

int ioCompletionHandlerProcess(IoCompletionHandler* handler, IoQueue* ioQueue,
                                ReadyQueue* readyQueue, PagingManager* pagingManager,
                                ProcessTable* processTable) {
    if (!handler || !ioQueue || !readyQueue) return 0;
    int completed = 0;
    for (int d = 0; d < numColasEs; d++) {
        Process* p = ioQueueGetFinished(ioQueue, d);
        while (p) {
            if (p->bcp) {
                Bcp* bcp = p->bcp;

                // ---------------------------------------------------------------
                // Verificación real de palabras vs. páginas en memoria principal.
                // Para cada palabra requerida, calculamos la página objetivo usando
                // un hash simple (misma lógica que la carga de palabras).
                // Si esa página no está en memoria (idMarco == -1), se genera un
                // fallo de página llamando a pagingManagerHandlePageFault.
                // ---------------------------------------------------------------
                if (pagingManager && processTable) {
                    // Encontrar el índice de proceso en la tabla
                    int procIdx = -1;
                    for (int i = 0; i < procesosEnEjecucion; i++) {
                        if (processTable->runningProcesses[i] == p) { procIdx = i; break; }
                    }
                    if (procIdx < 0) {
                        for (int i = 0; i < procesosEnEspera; i++) {
                            if (processTable->newRequests[i] == p) { procIdx = i; break; }
                        }
                    }

                    if (procIdx >= 0) {
                        int pageCount = bcp->pageCount > 0 ? bcp->pageCount : 1;
                        for (int w = 0; w < bcp->requiredWordsCount; w++) {
                            const char* word = bcp->requiredWords[w];
                            if (!word || word[0] == '\0') continue;

                            // Determinar página objetivo mediante hash
                            unsigned int hash = 0;
                            for (const char* c = word; *c; c++)
                                hash = hash * 31 + (unsigned char)*c;
                            int targetPage = procIdx * maxPaginasPorProceso
                                             + (int)(hash % (unsigned int)pageCount);
                            if (targetPage >= pagingManager->pageDirectory->totalPages)
                                targetPage = procIdx * maxPaginasPorProceso;

                            // Buscar la palabra en las palabras almacenadas en esa página
                            Pagina* pg = &pagingManager->pageDirectory->allPages[targetPage];
                            int found = 0;

                            if (pg->enMemoria && pg->idMarco >= 0) {
                                // Página en memoria: buscar la palabra en su arreglo
                                for (int slot = 0; slot < palabrasPorPagina; slot++) {
                                    if (strncmp(pg->words[slot], word, maxCaracteresPalabra) == 0) {
                                        found = 1;
                                        break;
                                    }
                                }
                                if (!found) {
                                    // La palabra no está en esta página: cargarla en el
                                    // primer slot libre de la página
                                    for (int slot = 0; slot < palabrasPorPagina; slot++) {
                                        if (pg->words[slot][0] == '\0') {
                                            strncpy(pg->words[slot], word, maxCaracteresPalabra - 1);
                                            pg->words[slot][maxCaracteresPalabra - 1] = '\0';
                                            found = 1;
                                            break;
                                        }
                                    }
                                }
                            } else {
                                // Página NO está en memoria → fallo de página
                                int faultResult = pagingManagerHandlePageFault(
                                    pagingManager, procIdx, targetPage, word);
                                if (faultResult == 0) {
                                    // Página traída a memoria: cargar la palabra
                                    pg->enMemoria = 1;
                                    for (int slot = 0; slot < palabrasPorPagina; slot++) {
                                        if (pg->words[slot][0] == '\0') {
                                            strncpy(pg->words[slot], word, maxCaracteresPalabra - 1);
                                            pg->words[slot][maxCaracteresPalabra - 1] = '\0';
                                            break;
                                        }
                                    }
                                    // Actualizar contador en la tabla de procesos
                                    if (processTable) processTable->totalPageFaults++;
                                }
                            }
                            (void)found;
                        }
                    }
                }

                bcp->ioOperationsPending = 0;
                bcp->state = ProcessStateReady;
            }
            readyQueueEnqueue(readyQueue, p);
            completed++;
            p = ioQueueGetFinished(ioQueue, d);
        }
    }
    handler->completionsHandled += completed;
    return completed;
}
int ioCompletionHandlerGetCompletionsHandled(IoCompletionHandler* handler) { return handler ? handler->completionsHandled : 0; }

//Parte 7 – Dominio: Memoria