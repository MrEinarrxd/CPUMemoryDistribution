// === src/domain/process/bcp.h ===

#ifndef BCP_H
#define BCP_H

#include "../../utils/constants.h"

typedef enum {
    ProcessStateNew,
    ProcessStateReady,
    ProcessStateRunning,
    ProcessStateWaitingIo,
    ProcessStateSwap,
    ProcessStateSuspended,
    ProcessStateFinished
} ProcessState;

typedef struct Bcp {
    char processId[idProcesoLen];
    int pid;
    ProcessState state;
    int priority;
    int totalCpuCycles;
    int remainingCycles;
    int arrivalTime;
    int currentTimeSlice;
    int contextSwitchTime;
    int timeInExecution;
    int timesExecuted;
    int creationTime;
    int startTime;
    int finishTime;
    int timeInWaiting;
    int ageingTimeSlices;
    int timesInIo;
    int ioTimeRemaining;   // Tiempo restante en cola de E/S (1-100 * multiplicador)

    // ============ I/O PHRASE HANDLING (NEW) ============
    // Cuando proceso va a E/S: se lee una frase aleatoria de frases.txt
    // La frase se divide en palabras que se verifican en memoria
    char ioPhrase[tamanoFraseIo];        // Frase completa leída de frases.txt
    char requiredWords[palabrasPorFrase][maxCaracteresPalabra]; // Las 5 palabras clave de la frase
    int requiredWordsCount;    // Cuántas palabras requiere (típicamente 5)

    int pageCount;
    int memoryRequested;
    int totalMemoryAllocated;
    int assignedPages[maxPaginasPorProceso];
    int quantumAssigned;
    int quantumUsed;
    float cpuWasteRatio;
    int agingCounter;
    int timesReturnedToReady;
    int wastedCpuCycles;
    int pageTableBase;
    int swapAddress;
    int ioOperationsPending;
    int contextSwitchCount;
} Bcp;

// Propietario exclusivo de: phraseBuffer, requiredWords[][], assignedPages[]
// Solo referencia: pageTableBase, swapAddress
// Los campos numéricos (ioOperationsPending, etc.) son propiedad del BCP

Bcp* bcpCreate(const char* processId, int pid);

void bcpDestroy(Bcp* bcp);

void bcpInitialize(Bcp* bcp, int priority, int executionTime, int arrivalTime);

void bcpSetState(Bcp* bcp, ProcessState state);

ProcessState bcpGetState(Bcp* bcp);

void bcpIncrementContextSwitches(Bcp* bcp);

void bcpUpdateRemainingTime(Bcp* bcp, int cycles);

void bcpAddPage(Bcp* bcp, int pageIndex);

void bcpAddPhrase(Bcp* bcp, const char* phrase);

#endif
