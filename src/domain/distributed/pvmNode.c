#include "pvmNode.h"
#include <stdlib.h>
#include <string.h>

PvmNode* pvmNodeCreate(int nodeId, const char* nodeName) {
    PvmNode* node = (PvmNode*)calloc(1, sizeof(PvmNode));
    if (!node) return NULL;
    node->nodeId = nodeId;
    if (nodeName)
        strncpy(node->nodeName, nodeName, sizeof(node->nodeName) - 1);
    node->isActive = 0;
    node->messagesReceived = 0;
    node->messagesSent = 0;
    node->localProcessCount = 0;
    return node;
}

void pvmNodeDestroy(PvmNode* node) { free(node); }

int pvmNodeSendMessage(PvmNode* node, struct PvmMessage* message) {
    if (!node || !message) return -1;
    node->messagesSent++;
    return 0;
}

struct PvmMessage* pvmNodeReceiveMessage(PvmNode* node) {
    if (!node) return NULL;
    node->messagesReceived++;
    return NULL;
}

int pvmNodeIsActive(PvmNode* node) { return node ? node->isActive : 0; }
void pvmNodeActivate(PvmNode* node)   { if (node) node->isActive = 1; }
void pvmNodeDeactivate(PvmNode* node) { if (node) node->isActive = 0; }

//Parte 10 – Orquestador principal y main