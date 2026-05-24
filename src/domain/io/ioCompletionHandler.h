// === src/domain/io/ioCompletionHandler.h ===

#ifndef IO_COMPLETION_HANDLER_H
#define IO_COMPLETION_HANDLER_H

struct IoQueue;
struct ReadyQueue;

// I/O completion handler
// Checks all 4 I/O queues every cycle and moves finished processes back to ready
// Does NOT own IoQueue or ReadyQueue, only references them
typedef struct IoCompletionHandler {
    int completionsHandled;                             // Total processes returned to ready
    int lastCompletionCycle;                            // Cycle of last completion check
} IoCompletionHandler;

// Create I/O completion handler
IoCompletionHandler* ioCompletionHandlerCreate(void);

// Destroy I/O completion handler
void ioCompletionHandlerDestroy(IoCompletionHandler* handler);

// Process completions from I/O queue and move to ready queue, returns count
int ioCompletionHandlerProcess(IoCompletionHandler* handler, struct IoQueue* ioQueue, struct ReadyQueue* readyQueue);

// Get total completions handled
int ioCompletionHandlerGetCompletionsHandled(IoCompletionHandler* handler);

#endif
