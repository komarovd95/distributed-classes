#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ipc.h"
#include "banking.h"
#include "core.h"
#include "transfers.h"
#include "pa2345.h"

/**
 * Serializes balance record to byte-buffer.
 *
 * @param buffer a byte-buffer
 * @param state a balance state record
 * @return size of serialize record
 */
size_t serialize_balance_state(char *buffer, const BalanceState *state);

/**
 * Deserializes balance record from byte-buffer.
 *
 * @param buffer a byte-buffer
 * @param state a balance state record
 */
void deserialize_balance_state(const char *buffer, BalanceState *state);

void transfer(void *parent_data, local_id src, local_id dst, balance_t amount) {
    ProcessState *state;
    Message message;
    TransferOrder transfer_order;
    char buffer[MAX_PAYLOAD_LEN];
    size_t serialized_size;

    state = (ProcessState *) parent_data;

    if (src == PARENT_ID || dst == PARENT_ID) {
        fprintf(stderr, "Transfers from or to parent process is disallowed!\n");
        exit(1);
    }
    if (src == dst) {
        fprintf(stderr, "Transfers to self process is disallowed!\n");
        exit(2);
    }
    if (state->id != PARENT_ID) {
        fprintf(stderr, "Only parent cat initiate transfer!\n");
        exit(3);
    }

    if (amount <= 0) {
        fprintf(stderr, "Only positive amount to transfer is allowed!\n");
        exit(4);
    }

    transfer_order.s_src = src;
    transfer_order.s_dst = dst;
    transfer_order.s_amount = amount;

    serialized_size = serialize_order(buffer, &transfer_order);

    on_message_send(state);
    construct_message(state, &message, TRANSFER, serialized_size, buffer);
    if (send(state, src, &message)) {
        fprintf(stderr, "Failed to init transfer order: from=%d to=%d amount=%d\n", src, dst, amount);
        exit(5);
    }
    if (receive(state, dst, &message)) {
        fprintf(stderr, "Failed to receive message from destination process of transfer!\n");
        exit(6);
    }
    on_message_received(state, &message);

    if (message.s_header.s_type != ACK) {
        fprintf(stderr, "Failed to receive ACK message from destination process!\n");
        exit(7);
    }
}

int process_transfer_out(ProcessState *state, TransferOrder *transfer_order) {
    Message message;
    char buffer[MAX_PAYLOAD_LEN];
    size_t serialized_size;

    if (state->balance < transfer_order->s_amount) {
        return 1;
    }
    state->balance -= transfer_order->s_amount;

    on_message_send(state);
    sprintf(buffer, log_transfer_out_fmt,
            state->vector_time[state->id], transfer_order->s_src, transfer_order->s_amount, transfer_order->s_dst);
    log_event(state, buffer);

    serialized_size = serialize_order(buffer, transfer_order);

    construct_message(state, &message, TRANSFER, serialized_size, buffer);
    if (send(state, transfer_order->s_dst, &message)) {
        return 2;
    }
    return 0;
}

int process_transfer_in(ProcessState *state, TransferOrder *transfer_order) {
    Message message;
    char buffer[MAX_PAYLOAD_LEN];

    state->balance += transfer_order->s_amount;

    sprintf(buffer, log_transfer_in_fmt,
            state->vector_time[state->id], transfer_order->s_dst, transfer_order->s_amount, transfer_order->s_src);
    log_event(state, buffer);

    on_message_send(state);
    construct_message(state, &message, ACK, 0, NULL);
    if (send(state, PARENT_ID, &message)) {
        return 2;
    }
    return 0;
}

int sync_balance(ProcessState *state, timestamp_t sync_time) {
    Message message;
    BalanceState balance_state;
    char buffer[MAX_PAYLOAD_LEN];
    size_t serialized_size;

    while (state->vector_time[state->id] < sync_time) {
        if (receive(state, PARENT_ID, &message)) {
            fprintf(stderr, "(%d) Failed to receive message for time sync!\n", state->id);
            return 1;
        }
        on_message_received(state, &message);

        if (message.s_header.s_type != EMPTY) {
            fprintf(stderr, "(%d) Failed to receive EMPTY message: type=%d!\n", state->id, message.s_header.s_type);
            return 2;
        }
    }

    balance_state.s_balance = state->balance;
    memcpy(balance_state.s_timevector, state->vector_time, sizeof(timestamp_t) * (state->processes_count + 1));

    serialized_size = serialize_balance_state(buffer, &balance_state);

    on_message_send(state);
    construct_message(state, &message, BALANCE_STATE, serialized_size, buffer);
    if (send(state, PARENT_ID, &message)) {
        fprintf(stderr, "(%d) Failed to send BALANCE STATE message!\n", state->id);
        return 3;
    }

    return 0;
}

size_t serialize_order(char *buffer, const TransferOrder *transfer_order) {
    size_t serialized_size;

    memcpy(buffer, &transfer_order->s_src, sizeof(transfer_order->s_src));
    serialized_size = sizeof(transfer_order->s_src);

    memcpy(buffer + serialized_size, &transfer_order->s_dst, sizeof(transfer_order->s_dst));
    serialized_size += sizeof(transfer_order->s_dst);

    memcpy(buffer + serialized_size, &transfer_order->s_amount, sizeof(transfer_order->s_amount));
    serialized_size += sizeof(transfer_order->s_amount);

    return serialized_size;
}

void deserialize_order(const char *buffer, TransferOrder *transfer_order) {
    size_t offset;

    memcpy(&transfer_order->s_src, buffer, sizeof(transfer_order->s_src));
    offset = sizeof(transfer_order->s_src);

    memcpy(&transfer_order->s_dst, buffer + offset, sizeof(transfer_order->s_dst));
    offset += sizeof(transfer_order->s_dst);

    memcpy(&transfer_order->s_amount, buffer + offset, sizeof(transfer_order->s_amount));
}

size_t serialize_balance_state(char *buffer, const BalanceState *state) {
    size_t serialized_size;

    memcpy(buffer, &state->s_time, sizeof(state->s_time));
    serialized_size = sizeof(state->s_time);

    memcpy(buffer + serialized_size, &state->s_balance, sizeof(state->s_balance));
    serialized_size += sizeof(state->s_balance);

    memcpy(buffer + serialized_size, &state->s_balance_pending_in, sizeof(state->s_balance_pending_in));
    serialized_size += sizeof(state->s_balance_pending_in);

    return serialized_size;
}

void deserialize_balance_state(const char *buffer, BalanceState *state) {
    size_t offset;

    memcpy(&state->s_time, buffer, sizeof(state->s_time));
    offset = sizeof(state->s_time);

    memcpy(&state->s_balance, buffer + offset, sizeof(state->s_balance));
    offset += sizeof(state->s_balance);

    memcpy(&state->s_balance_pending_in, buffer + offset, sizeof(state->s_balance_pending_in));
}

int calculate_total_balance(ProcessState *state) {
    Message message;
    BalanceState balance_state;
    balance_t total_balance;
    char buffer[MAX_PAYLOAD_LEN];
    timestamp_t temp_time[TOTAL_PROCESSES];
    timestamp_t received_time[TOTAL_PROCESSES];
    timestamp_t snapshot_time;

    memcpy(temp_time, state->vector_time, sizeof(timestamp_t) * (state->processes_count + 1));

    on_message_send(state);
    snapshot_time = state->vector_time[state->id];
    construct_message(state, &message, SNAPSHOT_VTIME, 0, NULL);
    if (send_multicast(state, &message)) {
        fprintf(stderr, "(%d) Failed to multicast SNAPSHOT VTIME!\n", state->id);
        return 1;
    }
    memcpy(state->vector_time, temp_time, sizeof(timestamp_t) * (state->processes_count + 1));

    for (local_id id = 1; id <= state->processes_count; ++id) {
        if (receive(state, id, &message)) {
            fprintf(stderr, "(%d) Failed to receive VTIME ACK message: from=%d\n", state->id, id);
            return 2;
        }
        if (message.s_header.s_type != SNAPSHOT_ACK) {
            fprintf(stderr, "(%d) Not a VTIME ACK received: from=%d type=%d\n", state->id, id, message.s_header.s_type);
            return 3;
        }
        received_time[id] = message.s_header.s_local_timevector[id];
    }

    for (local_id id = 1; id <= state->processes_count; ++id) {
        for (timestamp_t time = received_time[id]; time < snapshot_time; ++time) {
            on_message_send(state);
            construct_message(state, &message, EMPTY, 0, NULL);
            if (send(state, id, &message)) {
                fprintf(stderr, "(%d) Failed to send EMPTY message: to=%d\n", state->id, id);
                return 4;
            }
        }
    }

    total_balance = 0;
    for (local_id id = 1; id <= state->processes_count; ++id) {
        if (receive(state, id, &message)) {
            fprintf(stderr, "(%d) Failed to receive message on sync stage: from=%d\n", state->id, id);
            return 5;
        }
        on_message_received(state, &message);

        if (message.s_header.s_type != BALANCE_STATE) {
            fprintf(stderr, "(%d) Failed to receive BALANCE STATE message: from=%d type=%d\n",
                    state->id, id, message.s_header.s_type);
            return 6;
        }
        deserialize_balance_state(message.s_payload, &balance_state);

        total_balance += balance_state.s_balance;
    }

//    on_message_send(state);
//    construct_message(state, &message, EMPTY, 0, NULL);
//    if (send_multicast(state, &message)) {
//        fprintf(stderr, "(%d) Failed to multicast EMPTY message!\n", state->id);
//        return 4;
//    }
//
//    total_balance = 0;
//    for (local_id id = 1; id <= state->processes_count; ++id) {
//        if (receive(state, id, &message)) {
//            fprintf(stderr, "(%d) Failed to receive BALANCE STATE message: from=%d\n", state->id, id);
//            return 5;
//        }
//        on_message_received(state, &message);
//
//        if (message.s_header.s_type != BALANCE_STATE) {
//            fprintf(stderr, "(%d) Not a BALANCE STATE message was received: from=%d type=%d\n",
//                    state->id, id, message.s_header.s_type);
//            return 6;
//        }
//        deserialize_balance_state(buffer, &balance_state);
//        total_balance += balance_state.s_balance;
//    }
    sprintf(buffer, format_vector_snapshot,
            state->vector_time[0],
            state->vector_time[1],
            state->vector_time[2],
            state->vector_time[3],
            state->vector_time[4],
            state->vector_time[5],
            state->vector_time[6],
            state->vector_time[7],
            state->vector_time[8],
            state->vector_time[9],
            state->vector_time[10],
            total_balance
    );
    log_event(state, buffer);
    return 0;
}
