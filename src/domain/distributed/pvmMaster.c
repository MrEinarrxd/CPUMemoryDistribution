// === src/domain/distributed/pvmMaster.c ===
// Nodo maestro del sistema distribuido con PVM
// Responsable de distribuir tareas a esclavos, recolectar resultados e integrarlos

#include "pvmMaster.h"
#include "pvmSlave.h"
#include "messageProtocol.h"
#include "../process/bcp.h"
#include "../process/process.h"
#include "../process/processTable.h"
#include "../scheduler/rrScheduler.h"
#include "../../utils/constants.h"
#include "../../utils/errorHandler.h"
#include "../../presentation/consoleIo.h"

#include <pvm3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

// === Estructuras planas para serialización ===

// Para la tarea 1 (estadísticas por proceso)
typedef struct {
    int pid;
    char processId[idProcesoLen];
    int state;                 // ProcessState como entero
    int remainingCycles;
    int totalCpuCycles;
    int timesInIo;
    int wastedCpuCycles;
} BcpSummary;

// Para la tarea 2 (análisis RR por proceso)
typedef struct {
    int pid;
    char processId[idProcesoLen];
    int quantumAssigned;
    int quantumUsed;
    int timesReturnedToReady;
    float cpuWasteRatio;
} RrProcessData;

// === Función auxiliar para contar procesos activos ===
static int countActiveProcesses(ProcessTable* table) {
    int count = 0;
    if (!table) return 0;

    for (int i = 0; i < procesosEnEjecucion; i++) {
        if (table->runningProcesses[i] && table->runningProcesses[i]->bcp) {
            if (table->runningProcesses[i]->bcp->state != ProcessStateFinished) {
                count++;
            }
        }
    }

    for (int i = 0; i < procesosEnEspera; i++) {
        if (table->newRequests[i] && table->newRequests[i]->bcp) {
            if (table->newRequests[i]->bcp->state != ProcessStateFinished) {
                count++;
            }
        }
    }

    return count;
}

// === Función auxiliar para obtener procesos activos ===
static int getActiveProcesses(ProcessTable* table, Process** outArray, int maxSize) {
    int idx = 0;
    if (!table || !outArray) return 0;

    for (int i = 0; i < procesosEnEjecucion && idx < maxSize; i++) {
        if (table->runningProcesses[i] && table->runningProcesses[i]->bcp) {
            if (table->runningProcesses[i]->bcp->state != ProcessStateFinished) {
                outArray[idx++] = table->runningProcesses[i];
            }
        }
    }

    for (int i = 0; i < procesosEnEspera && idx < maxSize; i++) {
        if (table->newRequests[i] && table->newRequests[i]->bcp) {
            if (table->newRequests[i]->bcp->state != ProcessStateFinished) {
                outArray[idx++] = table->newRequests[i];
            }
        }
    }

    return idx;
}

// === Función auxiliar para convertir BCP a BcpSummary ===
static void bcpToBcpSummary(Bcp* bcp, BcpSummary* summary) {
    if (!bcp || !summary) return;

    summary->pid = bcp->pid;
    strncpy(summary->processId, bcp->processId, idProcesoLen - 1);
    summary->processId[idProcesoLen - 1] = '\0';
    summary->state = (int)bcp->state;
    summary->remainingCycles = bcp->remainingCycles;
    summary->totalCpuCycles = bcp->totalCpuCycles;
    summary->timesInIo = bcp->timesInIo;
    summary->wastedCpuCycles = bcp->wastedCpuCycles;
}

// === Función auxiliar para convertir BCP a RrProcessData ===
static void bcpToRrProcessData(Bcp* bcp, RrProcessData* data) {
    if (!bcp || !data) return;

    data->pid = bcp->pid;
    strncpy(data->processId, bcp->processId, idProcesoLen - 1);
    data->processId[idProcesoLen - 1] = '\0';
    data->quantumAssigned = bcp->quantumAssigned;
    data->quantumUsed = bcp->quantumUsed;
    data->timesReturnedToReady = bcp->timesReturnedToReady;
    data->cpuWasteRatio = bcp->cpuWasteRatio;
}

static int compareRankingEntries(const void* a, const void* b) {
    const struct {
        char processId[idProcesoLen];
        int waste;
    } *entryA = a, *entryB = b;

    int diff = entryB->waste - entryA->waste;
    if (diff != 0) return diff;
    return strcmp(entryA->processId, entryB->processId);
}

static void mergeTopRankings(const AgingResults* slaveResults, int numSlaves,
                             char outIds[totalRankingProcesos][idProcesoLen],
                             int outWaste[totalRankingProcesos],
                             int* outCount,
                             int useAgedList) {
    struct {
        char processId[idProcesoLen];
        int waste;
    } entries[pvmNumEsclavos * totalRankingProcesos];

    int totalEntries = 0;
    for (int i = 0; i < numSlaves; i++) {
        int count = useAgedList ? slaveResults[i].topAgedCount : slaveResults[i].topWastersCount;
        for (int j = 0; j < count && j < totalRankingProcesos; j++) {
            if (totalEntries >= pvmNumEsclavos * totalRankingProcesos) break;
            if (useAgedList) {
                memcpy(entries[totalEntries].processId, slaveResults[i].topAgedIds[j], idProcesoLen);
                entries[totalEntries].waste = slaveResults[i].topAgedCpuWaste[j];
            } else {
                memcpy(entries[totalEntries].processId, slaveResults[i].topWastersIds[j], idProcesoLen);
                entries[totalEntries].waste = slaveResults[i].topWastersCpuWaste[j];
            }
            totalEntries++;
        }
    }

    if (totalEntries == 0) {
        *outCount = 0;
        return;
    }

    qsort(entries, totalEntries, sizeof(entries[0]), compareRankingEntries);

    int countToCopy = totalEntries < totalRankingProcesos ? totalEntries : totalRankingProcesos;
    for (int i = 0; i < countToCopy; i++) {
        memcpy(outIds[i], entries[i].processId, idProcesoLen);
        outWaste[i] = entries[i].waste;
    }
    *outCount = countToCopy;
}

// === pvmMasterInit ===
PvmMaster* pvmMasterInit(void) {
    PvmMaster* master = (PvmMaster*)calloc(1, sizeof(PvmMaster));
    if (!master) {
        errorHandlerLog(ErrorCodeMemoryAllocationFailed, "pvmMasterInit: no se pudo asignar memoria");
        return NULL;
    }

    int tid = pvm_mytid();
    if (tid < 0) {
        errorHandlerLog(ErrorCodeNodeConnectionFailed,
                       "pvmMasterInit: pvm_mytid() falló - PVM puede no estar inicializado");
        free(master);
        return NULL;
    }

    master->masterTid = tid;
    memset(master->slaveTids, 0, sizeof(master->slaveTids));
    memset(master->task1Results, 0, sizeof(master->task1Results));
    memset(master->task2Results, 0, sizeof(master->task2Results));

    return master;
}

// === pvmMasterSpawnSlaves ===
int pvmMasterSpawnSlaves(PvmMaster* master) {
    if (!master) {
        errorHandlerLog(ErrorCodeInvalidArgument, "pvmMasterSpawnSlaves: master es NULL");
        return -1;
    }

    int num = pvm_spawn("pvmSlave", NULL, 0, "", pvmNumEsclavos, master->slaveTids);

    if (num < 0) {
        errorHandlerLog(ErrorCodeNodeConnectionFailed, "pvmMasterSpawnSlaves: pvm_spawn() falló");
        return -1;
    }

    if (num < pvmNumEsclavos) {
        errorHandlerLog(ErrorCodeNodeConnectionFailed,
                       "pvmMasterSpawnSlaves: solo se lanzaron algunos esclavos");
        return num;
    }

    return pvmNumEsclavos;
}

// === pvmMasterTask1Stats ===
void pvmMasterTask1Stats(PvmMaster* master) {
    if (!master || !master->processTable) {
        errorHandlerLog(ErrorCodeInvalidArgument, "pvmMasterTask1Stats: argumentos inválidos");
        return;
    }

    Process* activeProcesses[totalProcesos];
    int totalActive = getActiveProcesses(master->processTable, activeProcesses, totalProcesos);

    if (totalActive == 0) {
        consoleIoPrintLine("No hay procesos activos para la tarea 1.");
        return;
    }

    // Dividir en dos subconjuntos
    int split = totalActive / 2;

    for (int slaveIdx = 0; slaveIdx < pvmNumEsclavos; slaveIdx++) {
        int start = (slaveIdx == 0) ? 0 : split;
        int end = (slaveIdx == 0) ? split : totalActive;
        int count = end - start;

        if (count <= 0) continue;

        // Crear array de BcpSummary
        BcpSummary* summaries = (BcpSummary*)malloc(count * sizeof(BcpSummary));
        if (!summaries) {
            errorHandlerLog(ErrorCodeMemoryAllocationFailed,
                           "pvmMasterTask1Stats: no se pudo asignar memoria para summaries");
            continue;
        }

        for (int i = 0; i < count; i++) {
            Process* p = activeProcesses[start + i];
            if (p && p->bcp) {
                bcpToBcpSummary(p->bcp, &summaries[i]);
            }
        }

        // Empaquetar y enviar mensaje
        PvmMessage* msg = packBcpMessage(slaveIdx, master->masterTid, master->slaveTids[slaveIdx],
                                        (void*)summaries, count * sizeof(BcpSummary));
        if (msg) {
            msg->messageType = MessageBcpSnapshot;
            if (pvmMasterSendData(master, msg) < 0) {
                errorHandlerLog(ErrorCodeMessageProtocolError,
                               "pvmMasterTask1Stats: fallo al enviar datos a esclavo");
            }
            pvmMessageDestroy(msg);
        }

        free(summaries);
    }

    // Recibir resultados
    for (int slaveIdx = 0; slaveIdx < pvmNumEsclavos; slaveIdx++) {
        PvmMessage* response = pvmMasterReceiveData(master);
        if (response) {
            if (response->messageType == MessageStatistics) {
                unpackStatsMessage(response, &master->task1Results[slaveIdx]);
            }
            pvmMessageDestroy(response);
        } else {
            errorHandlerLog(ErrorCodeMessageProtocolError,
                           "pvmMasterTask1Stats: timeout esperando respuesta de esclavo");
        }
    }
}

// === pvmMasterTask2Aging ===
void pvmMasterTask2Aging(PvmMaster* master) {
    if (!master || !master->processTable || !master->rrScheduler) {
        errorHandlerLog(ErrorCodeInvalidArgument, "pvmMasterTask2Aging: argumentos inválidos");
        return;
    }

    Process* activeProcesses[totalProcesos];
    int totalActive = getActiveProcesses(master->processTable, activeProcesses, totalProcesos);

    if (totalActive == 0) {
        consoleIoPrintLine("No hay procesos activos para la tarea 2.");
        return;
    }

    // Dividir en dos subconjuntos
    int split = totalActive / 2;

    for (int slaveIdx = 0; slaveIdx < pvmNumEsclavos; slaveIdx++) {
        int start = (slaveIdx == 0) ? 0 : split;
        int end = (slaveIdx == 0) ? split : totalActive;
        int count = end - start;

        if (count <= 0) continue;

        // Crear array de RrProcessData
        RrProcessData* rrData = (RrProcessData*)malloc(count * sizeof(RrProcessData));
        if (!rrData) {
            errorHandlerLog(ErrorCodeMemoryAllocationFailed,
                           "pvmMasterTask2Aging: no se pudo asignar memoria para rrData");
            continue;
        }

        for (int i = 0; i < count; i++) {
            Process* p = activeProcesses[start + i];
            if (p && p->bcp) {
                bcpToRrProcessData(p->bcp, &rrData[i]);
            }
        }

        // Empaquetar y enviar mensaje
        PvmMessage* msg = packBcpMessage(slaveIdx, master->masterTid, master->slaveTids[slaveIdx],
                                        (void*)rrData, count * sizeof(RrProcessData));
        if (msg) {
            msg->messageType = MessageBcpSnapshot;
            if (pvmMasterSendData(master, msg) < 0) {
                errorHandlerLog(ErrorCodeMessageProtocolError,
                               "pvmMasterTask2Aging: fallo al enviar datos a esclavo");
            }
            pvmMessageDestroy(msg);
        }

        free(rrData);
    }

    // Recibir resultados
    for (int slaveIdx = 0; slaveIdx < pvmNumEsclavos; slaveIdx++) {
        PvmMessage* response = pvmMasterReceiveData(master);
        if (response) {
            if (response->messageType == MessageAging) {
                unpackAgingMessage(response, &master->task2Results[slaveIdx]);
            }
            pvmMessageDestroy(response);
        } else {
            errorHandlerLog(ErrorCodeMessageProtocolError,
                           "pvmMasterTask2Aging: timeout esperando respuesta de esclavo");
        }
    }
}

// === pvmMasterSendData ===
int pvmMasterSendData(PvmMaster* master, PvmMessage* message) {
    if (!master || !message) {
        errorHandlerLog(ErrorCodeInvalidArgument, "pvmMasterSendData: argumentos inválidos");
        return -1;
    }

    if (message->destinationNodeId < 0 || message->destinationNodeId >= pvmNumEsclavos) {
        errorHandlerLog(ErrorCodeInvalidArgument,
                       "pvmMasterSendData: destinationNodeId fuera de rango");
        return -1;
    }

    int destTid = master->slaveTids[message->destinationNodeId];
    if (destTid <= 0) {
        errorHandlerLog(ErrorCodeNodeConnectionFailed,
                       "pvmMasterSendData: TID destino inválido");
        return -1;
    }

    pvm_initsend(PvmDataDefault);
    pvm_pkbyte((char*)message, sizeof(PvmMessage), 1);

    int ret = pvm_send(destTid, message->messageType);
    if (ret < 0) {
        errorHandlerLog(ErrorCodeNodeConnectionFailed,
                       "pvmMasterSendData: pvm_send() falló");
        return -1;
    }

    return 0;
}

// === pvmMasterReceiveData ===
PvmMessage* pvmMasterReceiveData(PvmMaster* master) {
    if (!master) {
        errorHandlerLog(ErrorCodeInvalidArgument, "pvmMasterReceiveData: master es NULL");
        return NULL;
    }

    /* Opcionalmente solicitar ruta directa para mejorar el rendimiento de envío/recepción */
    pvm_setopt(PvmRoute, PvmRouteDirect);

    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;

    /* Esperar mensaje de cualquier origen (-1) de cualquier tipo (-1) con timeout */
    int ret = pvm_trecv(-1, -1, &timeout);
    if (ret == 0) {
        errorHandlerLog(ErrorCodeMessageProtocolError,
                       "timeout esperando respuesta de esclavo");
        return NULL;
    }

    if (ret < 0) {
        errorHandlerLog(ErrorCodeMessageProtocolError,
                       "pvmMasterReceiveData: pvm_trecv() falló");
        return NULL;
    }

    /* Asignar memoria para el mensaje */
    PvmMessage* msg = (PvmMessage*)malloc(sizeof(PvmMessage));
    if (!msg) {
        errorHandlerLog(ErrorCodeMemoryAllocationFailed,
                       "pvmMasterReceiveData: no se pudo asignar memoria");
        return NULL;
    }

    /* Desempaquetar */
    memset(msg, 0, sizeof(PvmMessage));
    pvm_upkint(&msg->messageId, 1, 1);
    pvm_upkint(&msg->sourceNodeId, 1, 1);
    pvm_upkint(&msg->destinationNodeId, 1, 1);
    pvm_upkint((int*)&msg->messageType, 1, 1);
    pvm_upkbyte(msg->payload, tamanoBufferMensaje, 1);
    pvm_upkint(&msg->payloadSize, 1, 1);
    pvm_upklong(&msg->timestamp, 1, 1);

    return msg;
}

// === pvmMasterIntegrateResults ===
void pvmMasterIntegrateResults(PvmMaster* master) {
    if (!master) {
        errorHandlerLog(ErrorCodeInvalidArgument, "pvmMasterIntegrateResults: master es NULL");
        return;
    }

    // Integrar estadísticas de tarea 1
    DistributedStats integrated1;
    memset(&integrated1, 0, sizeof(DistributedStats));
    for (int i = 0; i < pvmNumEsclavos; i++) {
        integrated1.totalProcessesFinished += master->task1Results[i].totalProcessesFinished;
        integrated1.totalProcessesWaiting += master->task1Results[i].totalProcessesWaiting;
        integrated1.avgRemainingCycles += master->task1Results[i].avgRemainingCycles;
        integrated1.totalIoOperations += master->task1Results[i].totalIoOperations;
        integrated1.avgCpuUtilization += master->task1Results[i].avgCpuUtilization;
    }
    integrated1.avgRemainingCycles /= pvmNumEsclavos;
    integrated1.avgCpuUtilization /= pvmNumEsclavos;

    // Integrar resultados de tarea 2
    AgingResults integrated2;
    memset(&integrated2, 0, sizeof(AgingResults));

    for (int i = 0; i < pvmNumEsclavos; i++) {
        integrated2.totalReturnsToReady += master->task2Results[i].totalReturnsToReady;
        integrated2.avgCpuUtilizationPerSlave += master->task2Results[i].avgCpuUtilizationPerSlave;
    }
    integrated2.avgCpuUtilizationPerSlave /= pvmNumEsclavos;

    // Fusionar rankings usando mergeTopRankings
    mergeTopRankings(master->task2Results, pvmNumEsclavos,
                     integrated2.topAgedIds,
                     integrated2.topAgedCpuWaste,
                     &integrated2.topAgedCount,
                     1);

    mergeTopRankings(master->task2Results, pvmNumEsclavos,
                     integrated2.topWastersIds,
                     integrated2.topWastersCpuWaste,
                     &integrated2.topWastersCount,
                     0);

    // Guardar resultados integrados en el slot 0
    master->task1Results[0] = integrated1;
    master->task2Results[0] = integrated2;
}

// === pvmMasterPrintResults ===
void pvmMasterPrintResults(PvmMaster* master) {
    if (!master) {
        errorHandlerLog(ErrorCodeInvalidArgument, "pvmMasterPrintResults: master es NULL");
        return;
    }

    consoleIoClear();
    consoleIoPrintSeparator();
    consoleIoPrintLine("=== RESULTADOS TAREA DISTRIBUIDA 1: ESTADÍSTICAS ===");
    consoleIoPrintSeparator();

    DistributedStats* stats1 = &master->task1Results[0];
    consoleIoPrintInt("Procesos finalizados: ", stats1->totalProcessesFinished);
    consoleIoPrintInt("Procesos en espera: ", stats1->totalProcessesWaiting);
    consoleIoPrintInt("Promedio ciclos pendientes: ", stats1->avgRemainingCycles);
    consoleIoPrintInt("Operaciones E/S totales: ", stats1->totalIoOperations);
    consoleIoPrintFloat("Utilización CPU promedio: ", stats1->avgCpuUtilization);

    consoleIoPrintSeparator();
    consoleIoPrintLine("=== RESULTADOS TAREA DISTRIBUIDA 2: ANÁLISIS RR ===");
    consoleIoPrintSeparator();

    AgingResults* aging = &master->task2Results[0];

    consoleIoPrintLine("Top 5 procesos más envejecidos:");
    for (int i = 0; i < aging->topAgedCount && i < totalRankingProcesos; i++) {
        char buf[256];
        snprintf(buf, sizeof(buf), "  %d. %s (desperdicio: %d)",
                 i + 1, aging->topAgedIds[i], aging->topAgedCpuWaste[i]);
        consoleIoPrintLine(buf);
    }

    consoleIoPrintLine("");
    consoleIoPrintLine("Top 5 procesos con mayor desperdicio CPU:");
    for (int i = 0; i < aging->topWastersCount && i < totalRankingProcesos; i++) {
        char buf[256];
        snprintf(buf, sizeof(buf), "  %d. %s (desperdicio: %d)",
                 i + 1, aging->topWastersIds[i], aging->topWastersCpuWaste[i]);
        consoleIoPrintLine(buf);
    }

    consoleIoPrintSeparator();
    consoleIoPrintFloat("Promedio aprovechamiento CPU: ", aging->avgCpuUtilizationPerSlave);
    consoleIoPrintInt("Veces que procesos retornaron a cola Listos: ", aging->totalReturnsToReady);
    consoleIoPrintSeparator();
}

// === pvmMasterCleanup ===
void pvmMasterCleanup(PvmMaster* master) {
    if (!master) return;

    pvm_exit();
    free(master);
}
