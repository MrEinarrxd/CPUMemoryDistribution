#include "ioDispatcher.h"
#include "ioQueue.h"
#include "../process/process.h"
#include "../../utils/random.h"
#include "../../utils/phraseLoader.h"
#include <stdlib.h>

IoDispatcher* ioDispatcherCreate(IoQueue* ioQueue) {
    IoDispatcher* disp = (IoDispatcher*)calloc(1, sizeof(IoDispatcher));
    if (disp) disp->ioQueue = ioQueue;
    return disp;
}
void ioDispatcherDestroy(IoDispatcher* dispatcher) { free(dispatcher); }

int ioDispatcherDispatch(IoDispatcher* dispatcher, Process* process, int deviceId) {
    if (!dispatcher || !process || !process->bcp || deviceId < 0 || deviceId >= numColasEs) return -1;
    int mult = 0;
    switch(deviceId) {
        case 0: mult = multColaEs1; break;
        case 1: mult = multColaEs2; break;
        case 2: mult = multColaEs3; break;
        case 3: mult = multColaEs4; break;
        default: return -1;
    }
    int tiempo = randomInt(tiempoEsMin, tiempoEsMax);
    process->bcp->ioTimeRemaining = tiempo * mult;
    int ret = ioQueueSend(dispatcher->ioQueue, process, deviceId);
    if (ret == 0) dispatcher->totalIoOperations++;
    return ret;
}

void ioDispatcherTick(IoDispatcher* dispatcher) {
    if (dispatcher) ioQueueTick(dispatcher->ioQueue);
}
int ioDispatcherGetTotalIoOperations(IoDispatcher* dispatcher) { return dispatcher ? dispatcher->totalIoOperations : 0; }
float ioDispatcherGetAvgIoTime(IoDispatcher* dispatcher) { return dispatcher ? dispatcher->avgIoTime : 0.0f; }

void ioDispatcherLoadRandomPhrase(Process* process) {
    if (!process || !process->bcp) return;
    const char* frase = phraseLoaderGetRandom();
    bcpAddPhrase(process->bcp, frase);
}
