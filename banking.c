#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ipc.h"
#include "banking.h"
#include "core.h"
#include "transfers.h"
#include "pa2345.h"

int do_decrement_balance(ProcessState *state, balance_t amount);
int do_increment_balance(ProcessState *state, timestamp_t time, balance_t amount);

size_t serialize_balance_state(char *buffer, const BalanceState *state);
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

    on_message_send();
    construct_message(&message, TRANSFER, serialized_size, buffer);
    if (send(state, src, &message)) {
        fprintf(stderr, "Failed to init transfer order: from=%d to=%d amount=%d\n", src, dst, amount);
        exit(5);
    }
    if (receive(state, dst, &message)) {
        fprintf(stderr, "Failed to receive message from destination process of transfer!\n");
        exit(6);
    }
    if (message.s_header.s_type != ACK) {
        fprintf(stderr, "Failed to receive ACK message from destination process!\n");
        exit(7);
    }
}

int process_transfer_out(ProcessState *state, TransferOrder *transfer_order) {
    Message message;
    char buffer[MAX_PAYLOAD_LEN];
    size_t serialized_size;

    on_message_send();
    if (do_decrement_balance(state, transfer_order->s_amount)) {
        return 1;
    }
    sprintf(buffer, log_transfer_out_fmt,
            get_lamport_time(), transfer_order->s_src, transfer_order->s_amount, transfer_order->s_dst);
    log_event(state, buffer);

    serialized_size = serialize_order(buffer, transfer_order);

    construct_message(&message, TRANSFER, serialized_size, buffer);
    if (send(state, transfer_order->s_dst, &message)) {
        return 2;
    }
    return 0;
}

int process_transfer_in(ProcessState *state, timestamp_t time, TransferOrder *transfer_order) {
    Message message;
    char buffer[MAX_PAYLOAD_LEN];

    if (do_increment_balance(state, time, transfer_order->s_amount)) {
        return 1;
    }
    sprintf(buffer, log_transfer_in_fmt,
            get_lamport_time(), transfer_order->s_dst, transfer_order->s_amount, transfer_order->s_src);
    log_event(state, buffer);

    on_message_send();
    construct_message(&message, ACK, 0, NULL);
    if (send(state, PARENT_ID, &message)) {
        return 2;
    }
    return 0;
}

int do_decrement_balance(ProcessState *state, balance_t amount) {
    BalanceHistory *history;
    int current_len;
    timestamp_t current_time;

    if (state->balance < amount) {
        return 1;
    }
    state->balance -= amount;

    history = &state->history;
    current_time = get_lamport_time();
    current_len = history->s_history_len;
    for (int i = current_len; i < current_time; ++i) {
        history->s_history[i].s_balance = history->s_history[current_len - 1].s_balance;
        history->s_history[i].s_balance_pending_in = history->s_history[current_len - 1].s_balance_pending_in;
        history->s_history[i].s_time = (timestamp_t) i;
    }

    history->s_history[current_time].s_balance = state->balance;
    history->s_history[current_time].s_balance_pending_in = history->s_history[current_len - 1].s_balance_pending_in;;
    history->s_history[current_time].s_time = current_time;

    history->s_history_len = (uint8_t) (current_time + 1);
    return 0;
}

int do_increment_balance(ProcessState *state, timestamp_t time, balance_t amount) {
    BalanceHistory *history;
    int current_len;
    timestamp_t current_time;

    state->balance += amount;

    history = &state->history;
    current_time = get_lamport_time();
    current_len = history->s_history_len;

    for (int i = current_len; i < current_time; ++i) {
        history->s_history[i].s_balance = history->s_history[current_len - 1].s_balance;
        history->s_history[i].s_balance_pending_in = history->s_history[current_len - 1].s_balance_pending_in;
        if (time <= i) {
            history->s_history[i].s_balance_pending_in += amount;
        }
        history->s_history[i].s_time = (timestamp_t) i;
    }

    history->s_history[current_time].s_balance = state->balance;
    history->s_history[current_time].s_balance_pending_in = 0;
    history->s_history[current_time].s_time = current_time;

    history->s_history_len = (uint8_t) (current_time + 1);
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


//    local_id src;
//    local_id dst;
//    balance_t amount;
//
//    src = buffer[0];
//    dst = buffer[1];
//
//    amount = buffer[2] << 8;
//    amount |= buffer[3];
//
//    transfer_order->s_src = src;
//    transfer_order->s_dst = dst;
//    transfer_order->s_amount = amount;
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

//    serialized_size = 0;
//    buffer[serialized_size++] = (char) (state->s_time >> 8);
//    buffer[serialized_size++] = (char) state->s_time;
//
//    buffer[serialized_size++] = (char) (state->s_balance >> 8);
//    buffer[serialized_size++] = (char) state->s_balance;
//
//    buffer[serialized_size++] = (char) (state->s_balance_pending_in >> 8);
//    buffer[serialized_size++] = (char) state->s_balance_pending_in;
//
//    return serialized_size;
}

void deserialize_balance_state(const char *buffer, BalanceState *state) {
    size_t offset;

    memcpy(&state->s_time, buffer, sizeof(state->s_time));
    offset = sizeof(state->s_time);

    memcpy(&state->s_balance, buffer + offset, sizeof(state->s_balance));
    offset += sizeof(state->s_balance);

    memcpy(&state->s_balance_pending_in, buffer + offset, sizeof(state->s_balance_pending_in));

//    timestamp_t time;
//    balance_t balance;
//    balance_t balance_pending;
//
//    time = buffer[0] << 8;
//    time |= buffer[1];
//
//    balance = buffer[2] << 8;
//    balance |= buffer[3];
//
//    balance_pending = buffer[4] << 8;
//    balance_pending |= buffer[5];
//
//    state->s_time = time;
//    state->s_balance = balance;
//    state->s_balance_pending_in = balance_pending;
}

void serialize_history(char *buffer, const BalanceHistory *history) {
    buffer[0] = history->s_id;
    buffer[1] = history->s_history_len;

    for (int i = 0; i < history->s_history_len; ++i) {
        serialize_balance_state(&buffer[2 + i * BALANCE_STATE_SIZE], &history->s_history[i]);
    }
}

void deserialize_history(const char *buffer, BalanceHistory *history) {
    history->s_id = buffer[0];
    history->s_history_len = (uint8_t) buffer[1];
    for (int i = 0; i < history->s_history_len; ++i) {
        deserialize_balance_state(&buffer[2 + i * BALANCE_STATE_SIZE], &history->s_history[i]);
    }
}

void adjust_history(AllHistory *history) {
    int i, j;
    uint8_t max_len;

    max_len = 0;
    for (i = 0; i < history->s_history_len; ++i) {
        if (history->s_history[i].s_history_len > max_len) {
            max_len = history->s_history[i].s_history_len;
        }
    }
    for (i = 0; i < history->s_history_len; ++i) {
        uint8_t current_len;
        BalanceHistory *balance_history;

        balance_history = &history->s_history[i];
        current_len = balance_history->s_history_len;
        for (j = current_len; j < max_len; ++j) {
            balance_history->s_history[j].s_balance = balance_history->s_history[current_len - 1].s_balance;
            balance_history->s_history[j].s_balance_pending_in = balance_history->s_history[current_len - 1].s_balance_pending_in;
            balance_history->s_history[j].s_time = (timestamp_t) j;
        }
        balance_history->s_history_len = max_len;
    }
}
