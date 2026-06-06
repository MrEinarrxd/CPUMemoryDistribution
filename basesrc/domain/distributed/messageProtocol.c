// === src/domain/distributed/messageProtocol.c ===
// Protocolo de mensajes para comunicación PVM
// Funciones de empaquetado y desempaquetado de estructuras de datos

#include "messageProtocol.h"
#include "../../utils/constants.h"
#include "../../utils/errorHandler.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

// === packBcpMessage ===
PvmMessage* packBcpMessage(int messageId, int sourceNodeId, int destinationNodeId,
                           const void* bcpData, int dataSize) {
    PvmMessage* msg = (PvmMessage*)malloc(sizeof(PvmMessage));
    if (!msg) {
        errorHandlerLog(ErrorCodeMemoryAllocationFailed,
                       "packBcpMessage: no se pudo asignar memoria");
        return NULL;
    }

    memset(msg, 0, sizeof(PvmMessage));
    msg->messageId = messageId;
    msg->sourceNodeId = sourceNodeId;
    msg->destinationNodeId = destinationNodeId;
    msg->messageType = MessageBcpSnapshot;
    msg->timestamp = (long)time(NULL);

    // Copiar datos al payload
    if (bcpData && dataSize > 0) {
        if (dataSize > tamanoBufferMensaje) {
            dataSize = tamanoBufferMensaje;
            errorHandlerLog(ErrorCodeInvalidArgument,
                           "packBcpMessage: tamaño de datos excede buffer");
        }
        memcpy(msg->payload, bcpData, dataSize);
        msg->payloadSize = dataSize;
    } else {
        msg->payloadSize = 0;
    }

    return msg;
}

// === unpackBcpMessage ===
void unpackBcpMessage(PvmMessage* message, void* outBcpData, int maxSize) {
    if (!message || !outBcpData) {
        errorHandlerLog(ErrorCodeInvalidArgument,
                       "unpackBcpMessage: argumentos inválidos");
        return;
    }

    int copySize = message->payloadSize;
    if (copySize > maxSize) {
        copySize = maxSize;
        errorHandlerLog(ErrorCodeInvalidArgument,
                       "unpackBcpMessage: buffer de salida insuficiente");
    }

    memcpy(outBcpData, message->payload, copySize);
}

// === packStatsMessage ===
PvmMessage* packStatsMessage(int messageId, int sourceNodeId, int destinationNodeId,
                             DistributedStats* stats) {
    PvmMessage* msg = (PvmMessage*)malloc(sizeof(PvmMessage));
    if (!msg) {
        errorHandlerLog(ErrorCodeMemoryAllocationFailed,
                       "packStatsMessage: no se pudo asignar memoria");
        return NULL;
    }

    memset(msg, 0, sizeof(PvmMessage));
    msg->messageId = messageId;
    msg->sourceNodeId = sourceNodeId;
    msg->destinationNodeId = destinationNodeId;
    msg->messageType = MessageStatistics;
    msg->timestamp = (long)time(NULL);

    if (stats) {
        memcpy(msg->payload, stats, sizeof(DistributedStats));
        msg->payloadSize = sizeof(DistributedStats);
    } else {
        msg->payloadSize = 0;
    }

    return msg;
}

// === unpackStatsMessage ===
void unpackStatsMessage(PvmMessage* message, DistributedStats* outStats) {
    if (!message || !outStats) {
        errorHandlerLog(ErrorCodeInvalidArgument,
                       "unpackStatsMessage: argumentos inválidos");
        return;
    }

    if (message->payloadSize >= sizeof(DistributedStats)) {
        memcpy(outStats, message->payload, sizeof(DistributedStats));
    } else {
        memset(outStats, 0, sizeof(DistributedStats));
        if (message->payloadSize > 0) {
            memcpy(outStats, message->payload, message->payloadSize);
        }
    }
}

// === packAgingMessage ===
PvmMessage* packAgingMessage(int messageId, int sourceNodeId, int destinationNodeId,
                             AgingResults* aging) {
    PvmMessage* msg = (PvmMessage*)malloc(sizeof(PvmMessage));
    if (!msg) {
        errorHandlerLog(ErrorCodeMemoryAllocationFailed,
                       "packAgingMessage: no se pudo asignar memoria");
        return NULL;
    }

    memset(msg, 0, sizeof(PvmMessage));
    msg->messageId = messageId;
    msg->sourceNodeId = sourceNodeId;
    msg->destinationNodeId = destinationNodeId;
    msg->messageType = MessageAging;
    msg->timestamp = (long)time(NULL);

    if (aging) {
        memcpy(msg->payload, aging, sizeof(AgingResults));
        msg->payloadSize = sizeof(AgingResults);
    } else {
        msg->payloadSize = 0;
    }

    return msg;
}

// === unpackAgingMessage ===
void unpackAgingMessage(PvmMessage* message, AgingResults* outAging) {
    if (!message || !outAging) {
        errorHandlerLog(ErrorCodeInvalidArgument,
                       "unpackAgingMessage: argumentos inválidos");
        return;
    }

    if (message->payloadSize >= sizeof(AgingResults)) {
        memcpy(outAging, message->payload, sizeof(AgingResults));
    } else {
        memset(outAging, 0, sizeof(AgingResults));
        if (message->payloadSize > 0) {
            memcpy(outAging, message->payload, message->payloadSize);
        }
    }
}

// === pvmMessageDestroy ===
void pvmMessageDestroy(PvmMessage* message) {
    if (message) {
        free(message);
    }
}
