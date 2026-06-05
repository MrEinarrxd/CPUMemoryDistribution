#include "rrScheduler.h"
#include "readyQueue.h"
#include "ioQueue.h"
#include "processTable.h"
#include "bcp.h"
#include <stdlib.h>
#include <string.h>

RrScheduler* rrSchedulerCreate(int quantum) {
    RrScheduler* s = (RrScheduler*)calloc(1, sizeof(RrScheduler));
    if (s) {
        s->currentQuantum = quantum;
        s->historyIndex = 0;
        s->proportionReady = 0.0f;
        s->proportionWaiting = 0.0f;
        s->iterationsSinceBalance = 0;
        s->hasPrivilegedProcess = 0;
        memset(s->privilegedProcessId, 0, idProcesoLen);
        memset(s->quantumHistory, 0, sizeof(s->quantumHistory));
        memset(&s->agingRanking, 0, sizeof(s->agingRanking));
    }
    return s;
}
void rrSchedulerDestroy(RrScheduler* scheduler) { free(scheduler); }

Process* rrSchedulerSelectNext(RrScheduler* scheduler, ProcessTable* table) {
    if (!scheduler || !table) return NULL;
    ReadyQueue* rq = table->readyQueue;
    if (!rq || readyQueueIsEmpty(rq)) return NULL;
    
    if (scheduler->hasPrivilegedProcess) {
        int size = readyQueueGetCount(rq);
        for (int i = 0; i < size; i++) {
            Process* p = rq->processes[(rq->head + i) % tamColaListos];
            if (p && p->bcp && strcmp(p->bcp->processId, scheduler->privilegedProcessId) == 0) {
                // Extraerlo desplazando elementos a la izquierda
                for (int j = i; j < size - 1; j++) {
                    int cur = (rq->head + j) % tamColaListos;
                    int next = (rq->head + j + 1) % tamColaListos;
                    rq->processes[cur] = rq->processes[next];
                }
                rq->tail = (rq->tail - 1 + tamColaListos) % tamColaListos;
                rq->processes[rq->tail] = NULL;
                rq->count--;
                scheduler->hasPrivilegedProcess = 0;
                memset(scheduler->privilegedProcessId, 0, idProcesoLen);
                return p;
            }
        }
        scheduler->hasPrivilegedProcess = 0;
    }
    
    return readyQueueDequeue(rq);
}

void rrSchedulerOnQuantumExpired(RrScheduler* scheduler) { (void)scheduler; }

void rrSchedulerUpdateProportions(RrScheduler* scheduler, ProcessTable* table) {
    if (!scheduler || !table) return;
    int ready = readyQueueGetCount(table->readyQueue);
    int waiting = 0;
    if (table->ioQueue) {
        for (int d = 0; d < numColasEs; d++) {
            waiting += table->ioQueue->devices[d].size;
        }
    }
    int total = ready + waiting;
    if (total > 0) {
        scheduler->proportionReady = (float)ready / total;
        scheduler->proportionWaiting = (float)waiting / total;
    } else {
        scheduler->proportionReady = 0;
        scheduler->proportionWaiting = 0;
    }
}

void rrSchedulerUpdateAgingRanking(RrScheduler* scheduler, ProcessTable* table) {
    if (!scheduler || !table) return;
    typedef struct {
        char id[idProcesoLen];
        int primary;
        int secondary;
    } Entry;
    Entry aged[totalProcesos];
    Entry wasters[totalProcesos];
    int agedCount = 0;
    int wasterCount = 0;
    
    for (int i = 0; i < procesosEnEjecucion; i++) {
        Process* p = table->runningProcesses[i];
        if (p && p->bcp && p->bcp->state != ProcessStateFinished && p->bcp->timesExecuted > 0) {
            strncpy(aged[agedCount].id, p->bcp->processId, idProcesoLen - 1);
            aged[agedCount].id[idProcesoLen - 1] = '\0';
            aged[agedCount].primary = p->bcp->timesReturnedToReady;
            aged[agedCount].secondary = p->bcp->remainingCycles;
            agedCount++;

            strncpy(wasters[wasterCount].id, p->bcp->processId, idProcesoLen - 1);
            wasters[wasterCount].id[idProcesoLen - 1] = '\0';
            wasters[wasterCount].primary = p->bcp->wastedCpuCycles;
            wasters[wasterCount].secondary = p->bcp->remainingCycles;
            wasterCount++;
        }
    }
    for (int i = 0; i < procesosEnEspera; i++) {
        Process* p = table->newRequests[i];
        if (p && p->bcp && p->bcp->state != ProcessStateFinished && p->bcp->timesExecuted > 0) {
            strncpy(aged[agedCount].id, p->bcp->processId, idProcesoLen - 1);
            aged[agedCount].id[idProcesoLen - 1] = '\0';
            aged[agedCount].primary = p->bcp->timesReturnedToReady;
            aged[agedCount].secondary = p->bcp->remainingCycles;
            agedCount++;

            strncpy(wasters[wasterCount].id, p->bcp->processId, idProcesoLen - 1);
            wasters[wasterCount].id[idProcesoLen - 1] = '\0';
            wasters[wasterCount].primary = p->bcp->wastedCpuCycles;
            wasters[wasterCount].secondary = p->bcp->remainingCycles;
            wasterCount++;
        }
    }
    
    for (int i = 0; i < agedCount - 1; i++)
        for (int j = i + 1; j < agedCount; j++)
            if (aged[j].primary > aged[i].primary ||
                (aged[j].primary == aged[i].primary && aged[j].secondary > aged[i].secondary)) {
                Entry tmp = aged[i]; aged[i] = aged[j]; aged[j] = tmp;
            }

    for (int i = 0; i < wasterCount - 1; i++)
        for (int j = i + 1; j < wasterCount; j++)
            if (wasters[j].primary > wasters[i].primary ||
                (wasters[j].primary == wasters[i].primary && wasters[j].secondary > wasters[i].secondary)) {
                Entry tmp = wasters[i]; wasters[i] = wasters[j]; wasters[j] = tmp;
            }
    
    int topAged = agedCount < totalRankingProcesos ? agedCount : totalRankingProcesos;
    scheduler->agingRanking.count = topAged;
    for (int i = 0; i < topAged; i++) {
        strcpy(scheduler->agingRanking.processIds[i], aged[i].id);
        scheduler->agingRanking.wasteValues[i] = aged[i].primary;
    }

    int topWasters = wasterCount < totalRankingProcesos ? wasterCount : totalRankingProcesos;
    scheduler->agingRanking.wasterCount = topWasters;
    for (int i = 0; i < topWasters; i++) {
        strcpy(scheduler->agingRanking.wasterProcessIds[i], wasters[i].id);
        scheduler->agingRanking.wasterWasteValues[i] = wasters[i].primary;
    }
}

void rrSchedulerPrioritizeProcess(RrScheduler* scheduler, const char* processId) {
    if (!scheduler || !processId) return;
    strncpy(scheduler->privilegedProcessId, processId, idProcesoLen - 1);
    scheduler->privilegedProcessId[idProcesoLen - 1] = '\0';
    scheduler->hasPrivilegedProcess = 1;
}

int rrSchedulerGetCurrentQuantum(RrScheduler* scheduler) { return scheduler ? scheduler->currentQuantum : 20; }
