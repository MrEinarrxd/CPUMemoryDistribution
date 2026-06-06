#include "protocol.h"

#include <string.h>

void protocolClearPacket(PvmPacket* packet, DistributedMessageType type, int workerIndex) {
    if (!packet) return;
    memset(packet, 0, sizeof(*packet));
    packet->messageType = (int)type;
    packet->workerIndex = workerIndex;
}
