#ifndef BCP_LOG_H
#define BCP_LOG_H

struct Logger;
struct Bcp;

typedef struct BcpLog {
    struct Logger* logger;
} BcpLog;

BcpLog* bcpLogCreate(struct Logger* logger);
void bcpLogDestroy(BcpLog* log);
void bcpLogRecordContextSwitch(BcpLog* log, struct Bcp* bcp);
void bcpLogRecordQuantumExpired(BcpLog* log, struct Bcp* bcp);
void bcpLogRecordPageSwap(BcpLog* log, struct Bcp* bcp, int swapAddress);
void bcpLogFlush(BcpLog* log);
void bcpLogRecordFull(BcpLog* log, struct Bcp* bcp);

#endif
