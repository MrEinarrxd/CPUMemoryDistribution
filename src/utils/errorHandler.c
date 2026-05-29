#include "errorHandler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* errorMessages[] = {
    [ErrorCodeSuccess]                   = "Éxito",
    [ErrorCodeMemoryAllocationFailed]    = "Fallo de asignación de memoria",
    [ErrorCodeInvalidProcess]            = "Proceso inválido",
    [ErrorCodeInvalidScheduler]          = "Planificador inválido",
    [ErrorCodeSwapFull]                  = "Área de swap llena",
    [ErrorCodeIoOperationFailed]         = "Operación de E/S fallida",
    [ErrorCodeInvalidPageTable]          = "Tabla de páginas inválida",
    [ErrorCodePageFaultUnrecoverable]    = "Fallo de página irrecuperable",
    [ErrorCodeQueueFull]                 = "Cola llena",
    [ErrorCodeQueueEmpty]                = "Cola vacía",
    [ErrorCodeInvalidArgument]           = "Argumento inválido",
    [ErrorCodeNodeConnectionFailed]      = "Conexión de nodo fallida",
    [ErrorCodeMessageProtocolError]      = "Error de protocolo de mensajes",
    [ErrorCodeFileOperationFailed]       = "Operación de archivo fallida",
    [ErrorCodeSystemError]               = "Error del sistema"
};

void errorHandlerInit(void) {}

void errorHandlerLog(ErrorCode code, const char* context) {
    if (code < 0 || code >= sizeof(errorMessages)/sizeof(errorMessages[0])) {
        fprintf(stderr, "[ERROR] Código desconocido: %d, Contexto: %s\n", code, context ? context : "(null)");
        return;
    }
    const char* msg = errorMessages[code];
    if (context && context[0] != '\0')
        fprintf(stderr, "[ERROR] %s: %s\n", msg, context);
    else
        fprintf(stderr, "[ERROR] %s\n", msg);
}

const char* errorHandlerGetMessage(ErrorCode code) {
    if (code < 0 || code >= sizeof(errorMessages)/sizeof(errorMessages[0]))
        return "Código de error desconocido";
    return errorMessages[code];
}

void errorHandlerCleanup(void) {}
