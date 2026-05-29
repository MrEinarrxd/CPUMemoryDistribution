#ifndef IO_COMPLETION_HANDLER_H
#define IO_COMPLETION_HANDLER_H

struct IoQueue;
struct ReadyQueue;
struct PagingManager;
struct ProcessTable;

typedef struct IoCompletionHandler {
    int completionsHandled;
    int lastCompletionCycle;
} IoCompletionHandler;

IoCompletionHandler* ioCompletionHandlerCreate(void);
void ioCompletionHandlerDestroy(IoCompletionHandler* handler);
int ioCompletionHandlerProcess(IoCompletionHandler* handler, struct IoQueue* ioQueue,
                               struct ReadyQueue* readyQueue, struct PagingManager* pagingManager,
                               struct ProcessTable* processTable);
int ioCompletionHandlerGetCompletionsHandled(IoCompletionHandler* handler);

#endif