#include "processGenerator.h"
#include "bcp.h"
#include "../../utils/random.h"
#include "../../utils/uniqueId.h"
#include "../../utils/constants.h"
#include "processTable.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct ArrivalPair {
    int arrival;
    int idx;
} ArrivalPair;

static Bcp** g_bcpArray = NULL;
static int* g_arrivalTimes = NULL;
static int g_nextIndex = 0;

static int arrivalPairCmp(const void* a, const void* b) {
    const ArrivalPair* A = (const ArrivalPair*)a;
    const ArrivalPair* B = (const ArrivalPair*)b;
    if (A->arrival < B->arrival) return -1;
    if (A->arrival > B->arrival) return 1;
    return 0;
}

void processGeneratorInit(void) {
    if (g_bcpArray != NULL) {
        g_nextIndex = 0;
        fprintf(stderr, "[Generator] Reutilizando generador existente.\n");
        return;
    }

    fprintf(stderr, "[Generator] Inicializando generador de %d procesos...\n", totalProcesos);

    g_bcpArray = (Bcp**)calloc(totalProcesos, sizeof(Bcp*));
    if (!g_bcpArray) {
        fprintf(stderr, "[Generator] ERROR: no se pudo asignar g_bcpArray\n");
        return;
    }

    g_arrivalTimes = (int*)calloc(totalProcesos, sizeof(int));
    if (!g_arrivalTimes) {
        fprintf(stderr, "[Generator] ERROR: no se pudo asignar g_arrivalTimes\n");
        free(g_bcpArray);
        g_bcpArray = NULL;
        return;
    }

    int* tempArrivals = (int*)calloc(totalProcesos, sizeof(int));
    if (!tempArrivals) {
        fprintf(stderr, "[Generator] ERROR: no se pudo asignar tempArrivals\n");
        free(g_arrivalTimes);
        free(g_bcpArray);
        g_bcpArray = NULL;
        g_arrivalTimes = NULL;
        return;
    }

    generateUniqueArrivalTimes(tempArrivals, totalProcesos);

    ArrivalPair* pairs = (ArrivalPair*)malloc(sizeof(ArrivalPair) * totalProcesos);
    if (!pairs) {
        fprintf(stderr, "[Generator] ERROR: no se pudo asignar pairs\n");
        free(tempArrivals);
        free(g_arrivalTimes);
        free(g_bcpArray);
        g_bcpArray = NULL;
        g_arrivalTimes = NULL;
        return;
    }

    for (int i = 0; i < totalProcesos; ++i) {
        pairs[i].arrival = tempArrivals[i];
        pairs[i].idx = i;
    }
    qsort(pairs, totalProcesos, sizeof(ArrivalPair), arrivalPairCmp);

    for (int i = 0; i < totalProcesos; ++i) {
        char pidStr[idProcesoLen];
        generateProcessId(i + 1, pidStr, (size_t)idProcesoLen);
        Bcp* bcp = bcpCreate(pidStr, i + 1);
        if (!bcp) {
            fprintf(stderr, "[Generator] ERROR: no se pudo crear BCP para %s\n", pidStr);
            for (int j = 0; j < i; ++j) {
                if (g_bcpArray[j]) bcpDestroy(g_bcpArray[j]);
            }
            free(pairs);
            free(tempArrivals);
            free(g_arrivalTimes);
            free(g_bcpArray);
            g_bcpArray = NULL;
            g_arrivalTimes = NULL;
            return;
        }
        bcp->pid = i + 1;
        bcp->arrivalTime = pairs[i].arrival;
        bcp->totalCpuCycles = randomCpuCycles();
        bcp->remainingCycles = bcp->totalCpuCycles;
        bcp->priority = 0;
        bcp->pageCount = randomInt(marcosMin, marcosMax);
        bcp->memoryRequested = bcp->pageCount * palabrasPorPagina;
        bcp->totalMemoryAllocated = bcp->memoryRequested;
        bcp->pageTableBase = (i % procesosEnEjecucion) * maxPaginasPorProceso;
        bcp->creationTime = pairs[i].arrival + processGeneratorGetNextSleepTime();
        bcp->state = ProcessStateNew;
        g_bcpArray[i] = bcp;
        g_arrivalTimes[i] = bcp->arrivalTime;
    }

    free(pairs);
    free(tempArrivals);
    g_nextIndex = 0;

    fprintf(stderr, "[Generator] %d procesos generados exitosamente.\n", totalProcesos);
    fprintf(stderr, "[Generator] Primeros tiempos de llegada: ");
    for (int i = 0; i < (totalProcesos < 5 ? totalProcesos : 5); i++)
        fprintf(stderr, "%d ", g_bcpArray[i]->arrivalTime);
    fprintf(stderr, "\n");
}

struct Bcp* processGeneratorGetNext(void) {
    if (!g_bcpArray) return NULL;
    if (g_nextIndex >= totalProcesos) return NULL;
    return g_bcpArray[g_nextIndex++];
}

struct Bcp* processGeneratorPeekNext(void) {
    if (!g_bcpArray) return NULL;
    if (g_nextIndex >= totalProcesos) return NULL;
    return g_bcpArray[g_nextIndex];
}

int processGeneratorHasMore(void) {
    return (g_bcpArray && g_nextIndex < totalProcesos);
}

void processGeneratorReset(void) {
    g_nextIndex = 0;
}

void processGeneratorCleanup(void) {
    if (g_bcpArray) {
        for (int i = 0; i < totalProcesos; ++i) {
            if (g_bcpArray[i]) bcpDestroy(g_bcpArray[i]);
        }
        free(g_bcpArray);
        g_bcpArray = NULL;
    }
    if (g_arrivalTimes) {
        free(g_arrivalTimes);
        g_arrivalTimes = NULL;
    }
    g_nextIndex = 0;
    fprintf(stderr, "[Generator] Limpieza completada.\n");
}

// Las siguientes funciones no se usan realmente pero se dejan por compatibilidad
void processGeneratorRun(struct ProcessTable* table) { (void)table; }
int processGeneratorGetNextSleepTime(void) { return randomInt(sleepCreacionMin, sleepCreacionMax); }
void processGeneratorAssignUniqueIds(struct ProcessTable* table, int count) { (void)table; (void)count; }
void processGeneratorAssignUniqueArrivalTimes(struct ProcessTable* table, int count) { (void)table; (void)count; }
void processGeneratorAssignCpuCycles(struct ProcessTable* table, int count) { (void)table; (void)count; }
void processGeneratorAssignMemory(struct ProcessTable* table, int count) { (void)table; (void)count; }
