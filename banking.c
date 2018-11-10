#include <stdio.h>
#include <stdlib.h>
#include "ipc.h"
#include "banking.h"
#include "distributed.h"
#include "bank_utils.h"

void transfer(void * parent_data, local_id src, local_id dst, balance_t amount) {
    ProcessInfo *process_info;
    TransferOrder transfer_order;

    process_info = (ProcessInfo *) parent_data;

    transfer_order.s_src = src;
    transfer_order.s_dst = dst;
    transfer_order.s_amount = amount;

    if (init_transfer(process_info, &transfer_order)) {
        fprintf(stderr, "Failed to init transfer order: from=%d to=%d amount=%d\n", src, dst, amount);
        exit(1);
    }
}

void serialize_order(unsigned char *buffer, TransferOrder *transfer_order) {
    buffer[0] = (unsigned char) transfer_order->s_src >> 8;
    buffer[1] = (unsigned char) transfer_order->s_src;

    buffer[2] = (unsigned char) transfer_order->s_dst >> 8;
    buffer[3] = (unsigned char) transfer_order->s_dst;

    buffer[4] = (unsigned char) transfer_order->s_amount >> 8;
    buffer[5] = (unsigned char) transfer_order->s_amount;
}
