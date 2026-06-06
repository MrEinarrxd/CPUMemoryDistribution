// === src/domain/distributed/pvmNode.h ===

#ifndef PVM_NODE_H
#define PVM_NODE_H

#include "../../utils/constants.h"

struct PvmMessage;

typedef struct PvmNode {
    int nodeId;
    char nodeName[longitudMaximaCadena];
    int isActive;
    int messagesReceived;
    int messagesSent;
    int localProcessCount;
} PvmNode;

PvmNode* pvmNodeCreate(int nodeId, const char* nodeName);

void pvmNodeDestroy(PvmNode* node);

int pvmNodeSendMessage(PvmNode* node, struct PvmMessage* message);

struct PvmMessage* pvmNodeReceiveMessage(PvmNode* node);

int pvmNodeIsActive(PvmNode* node);

void pvmNodeActivate(PvmNode* node);

void pvmNodeDeactivate(PvmNode* node);

#endif
