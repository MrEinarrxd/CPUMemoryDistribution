// === src/utils/errorHandler.h ===

#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

typedef enum {
    ErrorCodeSuccess,
    ErrorCodeMemoryAllocationFailed,
    ErrorCodeInvalidProcess,
    ErrorCodeInvalidScheduler,
    ErrorCodeSwapFull,
    ErrorCodeIoOperationFailed,
    ErrorCodeInvalidPageTable,
    ErrorCodePageFaultUnrecoverable,
    ErrorCodeQueueFull,
    ErrorCodeQueueEmpty,
    ErrorCodeInvalidArgument,
    ErrorCodeNodeConnectionFailed,
    ErrorCodeMessageProtocolError,
    ErrorCodeFileOperationFailed,
    ErrorCodeSystemError
} ErrorCode;

void errorHandlerInit(void);

void errorHandlerLog(ErrorCode code, const char* context);

const char* errorHandlerGetMessage(ErrorCode code);

void errorHandlerCleanup(void);

#endif
