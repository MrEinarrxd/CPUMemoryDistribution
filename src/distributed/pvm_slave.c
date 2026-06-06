#include "distributed_tasks.h"
#include "protocol.h"

#include <stdio.h>
#include <string.h>

#include <pvm3.h>

static int receive_message(WireMessage* message) {
    int received = pvm_recv(-1, -1);
    if (received < 0) return -1;
    pvm_upkbyte((char*)message, (int)sizeof(*message), 1);
    return 0;
}

static int send_message(int master_tid, WireMessage* message) {
    pvm_initsend(PvmDataDefault);
    pvm_pkbyte((char*)message, (int)sizeof(*message), 1);
    return pvm_send(master_tid, message->type);
}

int main(void) {
    int master_tid = pvm_parent();
    if (master_tid < 0) {
        fprintf(stderr, "[simSlave] No se pudo obtener master PVM\n");
        pvm_exit();
        return 1;
    }

    while (1) {
        WireMessage incoming;
        if (receive_message(&incoming) != 0) continue;
        if (incoming.type == WIRE_FINISH) break;

        if (incoming.type == WIRE_STATS_REQUEST) {
            DistributedStats stats;
            int count = incoming.payload_size / (int)sizeof(BcpSummary);
            distributed_calculate_stats((const BcpSummary*)incoming.payload, count, &stats);
            WireMessage response;
            wire_message_init(&response, WIRE_STATS_RESPONSE);
            wire_message_set_payload(&response, &stats, (int)sizeof(stats));
            send_message(master_tid, &response);
        } else if (incoming.type == WIRE_AGING_REQUEST) {
            AgingResults aging;
            int count = incoming.payload_size / (int)sizeof(RrProcessData);
            distributed_calculate_aging((const RrProcessData*)incoming.payload, count, &aging);
            WireMessage response;
            wire_message_init(&response, WIRE_AGING_RESPONSE);
            wire_message_set_payload(&response, &aging, (int)sizeof(aging));
            send_message(master_tid, &response);
        }
    }

    pvm_exit();
    return 0;
}
