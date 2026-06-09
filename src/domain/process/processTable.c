#include "processTable.h"
#include "../../utils/idGenerator.h"
#include "../../utils/randomUtils.h"

#include <stdio.h>
#include <string.h>

static int processReadyTime(const Bcp* bcp) {
    if (!bcp) return 0;
    return bcp->arrivalTime + bcp->creationDelay;
}

static int moveToReady(ProcessTable* table, int processIndex) {
    Bcp* bcp;
    int readyTime;

    if (!table || processIndex < 0 || processIndex >= TotalProcesses) return -1;
    bcp = &table->processes[processIndex];
    if (bcp->state != processStateNew) return 0;
    readyTime = processReadyTime(bcp);
    if (readyTime > table->currentTime) return 0;
    if (readyQueuePush(&table->readyQueue, processIndex) != 0) return -1;
    bcp->state = processStateReady;
    bcp->lastReadyTime = readyTime;
    return 1;
}

static int findReadySlot(ProcessTable* table, int slots[], int slotCount) {
    int selectedSlot = -1;
    int selectedIndex = -1;
    int selectedReadyTime = 0;

    if (!table || !slots) return -1;
    for (int i = 0; i < slotCount; ++i) {
        int processIndex = slots[i];
        int readyTime;
        if (processIndex < 0 || processIndex >= TotalProcesses) continue;
        if (table->processes[processIndex].state != processStateNew) continue;
        readyTime = processReadyTime(&table->processes[processIndex]);
        if (readyTime > table->currentTime) continue;
        if (selectedSlot < 0 || readyTime < selectedReadyTime ||
            (readyTime == selectedReadyTime && processIndex < selectedIndex)) {
            selectedSlot = i;
            selectedIndex = processIndex;
            selectedReadyTime = readyTime;
        }
    }
    return selectedSlot;
}

static int findEmptyActiveSlot(ProcessTable* table) {
    if (!table) return -1;
    for (int i = 0; i < ActiveProcessCount; ++i) {
        if (table->activeSlots[i] < 0) return i;
    }
    return -1;
}

static void addToActive(ProcessTable* table, int slot, int processIndex) {
    table->activeSlots[slot] = processIndex;
    table->activeCount++;
    table->processes[processIndex].state = processStateNew;
}

void processTableInit(ProcessTable* table) {
    int arrivals[TotalProcesses];
    int order[TotalProcesses];

    if (!table) return;
    memset(table, 0, sizeof(*table));
    for (int i = 0; i < ActiveProcessCount; ++i) table->activeSlots[i] = -1;
    for (int i = 0; i < NewRequestCount; ++i) table->newSlots[i] = -1;
    readyQueueInit(&table->readyQueue);
    ioQueueInit(&table->ioQueue);

    randomUniqueArrivals(arrivals, TotalProcesses);
    for (int i = 0; i < TotalProcesses; ++i) {
        char processId[ProcessIdLen];
        generateProcessId(i + 1, processId, sizeof(processId));
        bcpInit(&table->processes[i], i + 1, processId, arrivals[i],
                 randomCpuCycles(), 0);
        table->processes[i].creationDelay = randomCreationSleep();
        table->processes[i].pageCount = randomEvenPageCount();
        order[i] = i;
    }

    for (int i = TotalProcesses - 1; i > 0; --i) {
        int j = randomInt(0, i);
        int tmp = order[i];
        order[i] = order[j];
        order[j] = tmp;
    }

    for (int i = 0; i < ActiveProcessCount; ++i) {
        table->processes[order[i]].generatedAsActive = 1;
        addToActive(table, i, order[i]);
    }

    table->newCount = NewRequestCount;
    for (int i = 0; i < NewRequestCount; ++i) {
        int processIndex = order[ActiveProcessCount + i];
        table->newSlots[i] = processIndex;
        table->processes[processIndex].state = processStateNew;
    }

    processTableUpdateQueueMetrics(table);
}

void processTablePromoteNew(ProcessTable* table) {
    if (!table) return;
    while (1) {
        int activeSlot = findReadySlot(table, table->activeSlots, ActiveProcessCount);
        if (activeSlot < 0) break;
        if (moveToReady(table, table->activeSlots[activeSlot]) < 0) break;
    }

    while (table->activeCount < ActiveProcessCount && table->newCount > 0) {
        int newPos = findReadySlot(table, table->newSlots, NewRequestCount);
        int activePos = findEmptyActiveSlot(table);
        int processIndex;
        if (newPos < 0) break;
        if (activePos < 0) break;
        processIndex = table->newSlots[newPos];
        addToActive(table, activePos, processIndex);
        if (moveToReady(table, processIndex) < 0) {
            table->activeSlots[activePos] = -1;
            table->activeCount--;
            break;
        }
        table->newSlots[newPos] = -1;
        table->newCount--;
    }
}

void processTableFinishProcess(ProcessTable* table, int processIndex) {
    Bcp* bcp;
    if (!table || processIndex < 0 || processIndex >= TotalProcesses) return;
    bcp = &table->processes[processIndex];
    if (bcp->state == processStateFinished) return;

    bcp->state = processStateFinished;
    bcp->finishTime = table->currentTime;
    bcp->turnaroundTime = bcp->finishTime - bcp->arrivalTime;
    table->finishedCount++;

    for (int i = 0; i < ActiveProcessCount; ++i) {
        if (table->activeSlots[i] == processIndex) {
            table->activeSlots[i] = -1;
            table->activeCount--;
            break;
        }
    }
}

void processTableUpdateQueueMetrics(ProcessTable* table) {
    int readyCount;
    int waitingCount;
    int totalQueue;

    if (!table) return;
    readyCount = table->readyQueue.count;
    waitingCount = ioQueueTotalCount(&table->ioQueue);
    totalQueue = readyCount + waitingCount;
    table->readyProportion = totalQueue > 0 ? (float)readyCount * 100.0f / (float)totalQueue : 0.0f;
    table->waitingProportion = totalQueue > 0 ? (float)waitingCount * 100.0f / (float)totalQueue : 0.0f;
}

void processTableUpdateAverages(ProcessTable* table) {
    long waitSum = 0;
    long execSum = 0;
    int finished = 0;

    if (!table) return;
    for (int i = 0; i < TotalProcesses; ++i) {
        waitSum += table->processes[i].waitingTime;
        execSum += table->processes[i].timeInExecution;
        if (table->processes[i].state == processStateFinished) finished++;
    }
    table->avgWaitingTime = (float)waitSum / (float)TotalProcesses;
    table->avgExecutionTime = (float)execSum / (float)TotalProcesses;
    table->avgFinishedPerTime = table->currentTime > 0
        ? (float)finished / (float)table->currentTime
        : 0.0f;
    table->cpuUtilization = table->currentTime > 0
        ? (float)table->totalCpuCyclesExecuted / (float)table->currentTime
        : 0.0f;
}

void processTableLogSnapshot(ProcessTable* table, Logger* logger) {
    char line[1024];
    if (!table || !logger) return;
    snprintf(line, sizeof(line),
             "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%.3f,%.3f,%.3f,%.3f,%d,%d",
             table->currentTime,
             table->cpuIterations,
             table->activeCount,
             table->newCount,
             table->finishedCount,
             table->readyQueue.count,
             ioQueueTotalCount(&table->ioQueue),
             table->totalCpuCyclesExecuted,
             table->totalCpuWasteCycles,
             table->totalContextSwitches,
             table->totalContextSwitchTime,
             table->totalIoOperations,
             table->totalPageFaults,
             table->memoryUsedFrames,
             table->memoryFreeFrames,
             table->memoryLargestFreeRun,
             table->fragmentation,
             table->avgWaitingTime,
             table->avgExecutionTime,
             table->cpuUtilization,
             table->currentQuantum,
             table->algorithmChanges);
    loggerTableLine(logger, line);
}

void processTableLogBcps(ProcessTable* table, Logger* logger) {
    char line[1600];
    if (!table || !logger) return;
    for (int i = 0; i < TotalProcesses; ++i) {
        Bcp* b = &table->processes[i];
        snprintf(line, sizeof(line),
                 "%s,%d,%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%.3f,%d,%d,%d",
                 b->processId,
                 b->pid,
                 processStateName(b->state),
                 b->priority,
                 b->generatedAsActive,
                 b->arrivalTime,
                 b->creationDelay,
                 b->startTime,
                 b->finishTime,
                 b->totalCpuCycles,
                 b->remainingCpuCycles,
                 b->currentInstanceCycles,
                 b->contextSwitchTime,
                 b->contextSwitchCount,
                 b->totalContextSwitchTime,
                 b->timeInExecution,
                 b->timesExecuted,
                 b->waitingTime,
                 b->turnaroundTime,
                 b->ioDevice,
                 b->ioTimeRemaining,
                 b->timesInIo,
                 b->memoryRequested,
                 b->totalMemoryAllocated,
                 b->pageCount,
                 b->pageFaults,
                 b->swapIns,
                 b->swapOuts,
                 b->cpuWasteRatio,
                 b->agingCounter,
                 b->timesReturnedToReady,
                 b->cpuWasteCycles);
        loggerBcpLine(logger, line);
    }
}

int processTableExportStatsRows(const ProcessTable* table, ProcessStatsRow rows[], int maxRows) {
    int count = 0;
    if (!table || !rows || maxRows <= 0) return 0;
    for (int i = 0; i < TotalProcesses && count < maxRows; ++i) {
        const Bcp* b = &table->processes[i];
        ProcessStatsRow* row = &rows[count++];
        memset(row, 0, sizeof(*row));
        row->pid = b->pid;
        strncpy(row->processId, b->processId, ProcessIdLen - 1);
        row->state = (int)b->state;
        row->remainingCycles = b->remainingCpuCycles;
        row->totalCpuCycles = b->totalCpuCycles;
        row->executedCycles = b->timeInExecution;
        row->timesInIo = b->timesInIo;
        row->wastedCpuCycles = b->cpuWasteCycles;
        row->timesReturnedToReady = b->timesReturnedToReady;
    }
    return count;
}

int processTableExportRrRows(const ProcessTable* table, RrAnalysisRow rows[], int maxRows) {
    int count = 0;
    if (!table || !rows || maxRows <= 0) return 0;
    for (int i = 0; i < TotalProcesses && count < maxRows; ++i) {
        const Bcp* b = &table->processes[i];
        RrAnalysisRow* row;

        /* Solo se exportan procesos que realmente pasaron por Round Robin.
           Esto evita que PVM genere rankings RR falsos cuando FCFS esta activo. */
        if (b->rrExecutionCount <= 0) continue;

        row = &rows[count++];
        memset(row, 0, sizeof(*row));
        row->pid = b->pid;
        strncpy(row->processId, b->processId, ProcessIdLen - 1);
        row->processId[ProcessIdLen - 1] = '\0';
        row->remainingCycles = b->remainingCpuCycles;
        row->totalCpuCycles = b->totalCpuCycles;
        row->executedCycles = b->timeInExecution;
        row->quantumAssigned = b->quantumAssigned;
        row->quantumUsed = b->quantumUsed;
        row->rrExecutionCount = b->rrExecutionCount;
        row->rrQuantumAssignedTotal = b->rrQuantumAssignedTotal;
        row->rrQuantumUsedTotal = b->rrQuantumUsedTotal;
        row->timesReturnedToReady = b->timesReturnedToReady;
        row->wastedCpuCycles = b->cpuWasteCycles;
        row->cpuWasteRatio = b->cpuWasteRatio;
    }
    return count;
}

int processTableFindById(ProcessTable* table, const char* processId) {
    if (!table || !processId) return -1;
    for (int i = 0; i < TotalProcesses; ++i) {
        if (strcmp(table->processes[i].processId, processId) == 0) return i;
    }
    return -1;
}
