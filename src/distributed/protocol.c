#include "protocol.h"

#include <string.h>

void wire_message_init(WireMessage* message, WireMessageType type) {
    if (!message) return;
    memset(message, 0, sizeof(*message));
    message->type = (int)type;
}

int wire_message_set_payload(WireMessage* message, const void* payload, int payload_size) {
    if (!message || !payload || payload_size < 0 || payload_size > SIM_WIRE_PAYLOAD_SIZE) return -1;
    memcpy(message->payload, payload, (size_t)payload_size);
    message->payload_size = payload_size;
    return 0;
}
