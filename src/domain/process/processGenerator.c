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
        return;
    }
    g_bcpArray = (Bcp**)calloc(totalProcesos, sizeof(Bcp*));
    if (!g_bcpArray) {
        g_nextIndex = 0;
        return;
    }
    g_arrivalTimes = (int*)calloc(totalProcesos, sizeof(int));
    if (!g_arrivalTimes) {
        free(g_bcpArray);
        g_bcpArray = NULL;
        g_nextIndex = 0;
        return;
    }
    int* tempArrivals = (int*)calloc(totalProcesos, sizeof(int));
    if (!tempArrivals) {
        free(g_arrivalTimes);
        free(g_bcpArray);
        g_arrivalTimes = NULL;
        g_bcpArray = NULL;
        g_nextIndex = 0;
        return;
    }
    generateUniqueArrivalTimes(tempArrivals, totalProcesos);
    ArrivalPair* pairs = (ArrivalPair*)malloc(sizeof(ArrivalPair) * totalProcesos);
    if (!pairs) {
        free(tempArrivals);
        free(g_arrivalTimes);
        free(g_bcpArray);
        g_arrivalTimes = NULL;
        g_bcpArray = NULL;
        g_nextIndex = 0;
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
            for (int j = 0; j < i; ++j) {
                if (g_bcpArray[j]) bcpDestroy(g_bcpArray[j]);
            }
            free(pairs);
            free(tempArrivals);
            free(g_arrivalTimes);
            free(g_bcpArray);
            g_bcpArray = NULL;
            g_arrivalTimes = NULL;
            g_nextIndex = 0;
            return;
        }
        bcp->pid = i + 1;
        bcp->arrivalTime = pairs[i].arrival;
        bcp->totalCpuCycles = randomCpuCycles();
        bcp->remainingCycles = bcp->totalCpuCycles;
        bcp->priority = 0;
        bcp->pageCount = randomInt(1, maxPaginasPorProceso);
        bcp->state = ProcessStateNew;
        g_bcpArray[i] = bcp;
        g_arrivalTimes[i] = bcp->arrivalTime;
    }
    free(pairs);
    free(tempArrivals);
    g_nextIndex = 0;
}

void processGeneratorRun(struct ProcessTable* table) {
    if (!g_bcpArray) processGeneratorInit();
    while (processGeneratorHasMore()) {
        (void)processGeneratorGetNext();
    }
    (void)table;
}

struct Bcp* processGeneratorGetNext(void) {
    if (!g_bcpArray) return NULL;
    if (g_nextIndex >= totalProcesos) return NULL;
    Bcp* b = g_bcpArray[g_nextIndex];
    g_nextIndex++;
    return b;
}

int processGeneratorHasMore(void) {
    if (!g_bcpArray) return 0;
    return g_nextIndex < totalProcesos;
}

void processGeneratorReset(void) { g_nextIndex = 0; }
int processGeneratorGetNextSleepTime(void) { return randomInt(sleepCreacionMin, sleepCreacionMax); }
void processGeneratorAssignUniqueIds(struct ProcessTable* table, int count) { (void)table; (void)count; }
void processGeneratorAssignUniqueArrivalTimes(struct ProcessTable* table, int count) { (void)table; (void)count; }
void processGeneratorAssignCpuCycles(struct ProcessTable* table, int count) { (void)table; (void)count; }
void processGeneratorAssignMemory(struct ProcessTable* table, int count) { (void)table; (void)count; }
void processGeneratorCleanup(void) {
    if (g_bcpArray) {
        for (int i = 0; i < totalProcesos; ++i) {
            if (g_bcpArray[i]) {
                bcpDestroy(g_bcpArray[i]);
                g_bcpArray[i] = NULL;
            }
        }
        free(g_bcpArray);
        g_bcpArray = NULL;
    }
    if (g_arrivalTimes) {
        free(g_arrivalTimes);
        g_arrivalTimes = NULL;
    }
    g_nextIndex = 0;
}

//Parte 5 – Dominio: Planificación (Schedulers)