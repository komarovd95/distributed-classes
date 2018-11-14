#include <stdio.h>
#include <stdlib.h>
#include "ipc.h"
#include "banking.h"
#include "distributed.h"
#include "transfers.h"

int do_decrement_balance(ProcessState *state, balance_t amount);
int do_increment_balance(ProcessState *state, balance_t amount);

void serialize_balance_state(char *buffer, const BalanceState *state);
void deserialize_balance_state(const char *buffer, BalanceState *state);

void adjust_history(ProcessState *state);

void transfer(void *parent_data, local_id src, local_id dst, balance_t amount) {
    ProcessState *state;
    Message message;
    TransferOrder transfer_order;
    char buffer[sizeof(TransferOrder)];

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

    serialize_order(buffer, &transfer_order);

    on_message_send();
    construct_message(&message, TRANSFER, sizeof(TransferOrder), buffer);
    if (send(state, src, &message)) {
//
//
//    if (send_message(state, src, TRANSFER, sizeof(TransferOrder), buffer)) {
        fprintf(stderr, "Failed to init transfer order: from=%d to=%d amount=%d\n", src, dst, amount);
        exit(5);
    }
    if (receive(state, dst, &message)) {
//
//    }
//
//    if (receive_from(state, dst, &message_type, NULL)) {
        fprintf(stderr, "Failed to receive message from destination process of transfer!\n");
        exit(6);
    }
    if (message.s_header.s_type != ACK) {
//
//    if (message_type != ACK) {
        fprintf(stderr, "Failed to receive ACK message from destination process!\n");
        exit(7);
    }
}

int process_transfer_out(ProcessState *state, TransferOrder *transfer_order) {
    Message message;
    char buffer[sizeof(TransferOrder)];

    if (do_decrement_balance(state, transfer_order->s_amount)) {
        return 1;
    }

    serialize_order(buffer, transfer_order);

    construct_message(&message, TRANSFER, sizeof(TransferOrder), buffer);
    if (send(state, transfer_order->s_dst, &message)) {
//    if (send_message(state, transfer_order->s_dst, TRANSFER, sizeof(TransferOrder), buffer)) {
        return 2;
    }
    return 0;
}

int process_transfer_in(ProcessState *state, TransferOrder *transfer_order) {
    Message message;

    if (do_increment_balance(state, transfer_order->s_amount)) {
        return 1;
    }

    construct_message(&message, ACK, 0, NULL);
    if (send(state, PARENT_ID, &message)) {
        return 2;
    }
    return 0;
}

int do_decrement_balance(ProcessState *state, balance_t amount) {
//    BalanceHistory *history;
//    timestamp_t start_time;

    if (state->balance < amount) {
        return 1;
    }
    state->balance -= amount;

//    history = &state->history; GHGttt654ew
//
//    history->s_history[history->s_history_len].s_time = history->s_history_len;
//    history->s_history[history->s_history_len].s_balance = state->balance;
//    history->s_history[history->s_history_len].s_balance_pending_in = 0; //get_lamport_time();
//
//    history->s_history_len++;

//    history->s_history[history->s_history_len].s_time = get_lamport_time();

//    start_time = (timestamp_t) (history->s_history[history->s_history_len - 1].s_time + 1);
//    for (timestamp_t t = start_time; t < get_lamport_time(); ++t) {
//        history->s_history[t].s_time = t;
//        history->s_history[t].s_balance = history->s_history[start_time].s_balance;
//        history->s_history[t].s_balance_pending_in = history->s_history[start_time].s_balance_pending_in;
//    }
//
//    history->s_history[get_lamport_time()].s_balance = state->balance;
//    history->s_history[get_lamport_time()].s_balance_pending_in = amount;
//    history->s_history[get_lamport_time()].s_time = get_lamport_time();
//
//    history->s_history_len = (uint8_t) get_lamport_time();

    return 0;
}

int do_increment_balance(ProcessState *state, balance_t amount) {
    BalanceHistory *history;
//    timestamp_t start_time;

    state->balance += amount;

//    history = &state->history;
//
//    history->s_history[history->s_history_len].s_time = history->s_history_len;
//    history->s_history[history->s_history_len].s_balance = state->balance;
//    history->s_history[history->s_history_len].s_balance_pending_in = 0;
//
//    history->s_history_len++;

//    start_time = (timestamp_t) (history->s_history[history->s_history_len - 1].s_time + 1);
//    for (timestamp_t t = start_time; t < get_lamport_time(); ++t) {
//        history->s_history[t].s_time = t;
//        history->s_history[t].s_balance = history->s_history[start_time].s_balance;
//        history->s_history[t].s_balance_pending_in = history->s_history[start_time].s_balance_pending_in;
//    }
//
//    history->s_history[get_lamport_time()].s_balance = state->balance;
//    history->s_history[get_lamport_time()].s_balance_pending_in = 0;
//    history->s_history[get_lamport_time()].s_time = get_lamport_time();
//
//    history->s_history_len = (uint8_t) get_lamport_time();

    return 0;
}

void adjust_history(ProcessState *state) {
//    BalanceHistory *history;
//
//    history = &state->history;

}

void serialize_order(char *buffer, const TransferOrder *transfer_order) {
    buffer[0] = transfer_order->s_src;
    buffer[1] = transfer_order->s_dst;

    buffer[2] = (char) (transfer_order->s_amount >> 8);
    buffer[3] = (char) transfer_order->s_amount;
}

void deserialize_order(const char *buffer, TransferOrder *transfer_order) {
    local_id src;
    local_id dst;
    balance_t amount;

    src = buffer[0];
    dst = buffer[1];

    amount = buffer[2] << 8;
    amount |= buffer[3];

    transfer_order->s_src = src;
    transfer_order->s_dst = dst;
    transfer_order->s_amount = amount;
}

void serialize_balance_state(char *buffer, const BalanceState *state) {
    buffer[0] = (char) (state->s_time >> 8);
    buffer[1] = (char) state->s_time;

    buffer[2] = (char) (state->s_balance >> 8);
    buffer[3] = (char) state->s_balance;

    buffer[4] = (char) (state->s_balance_pending_in >> 8);
    buffer[5] = (char) state->s_balance_pending_in;
}

void deserialize_balance_state(const char *buffer, BalanceState *state) {
    timestamp_t time;
    balance_t balance;
    balance_t balance_pending;

    time = buffer[0] << 8;
    time |= buffer[1];

    balance = buffer[2] << 8;
    balance |= buffer[3];

    balance_pending = buffer[4] << 8;
    balance_pending |= buffer[5];

    state->s_time = time;
    state->s_balance = balance;
    state->s_balance_pending_in = balance_pending;
}

void serialize_history(char *buffer, const BalanceHistory *history) {
    buffer[0] = history->s_id;
    buffer[1] = history->s_history_len;

    for (int i = 0; i < history->s_history_len; ++i) {
        serialize_balance_state(&buffer[2 + i * sizeof(BalanceState)], &history->s_history[i]);

//        buffer[2 + i * sizeof(BalanceState)] = (char) (history->s_history[i].s_time >> 8);
//        buffer[2 + i * sizeof(BalanceState) + 1] = (char) history->s_history[i].s_time;
//
//        buffer[2 + i * sizeof(BalanceState) + 2] = (char) (history->s_history[i].s_balance >> 8);
//        buffer[2 + i * sizeof(BalanceState) + 3] = (char) history->s_history[i].s_balance;
//
//        buffer[2 + i * sizeof(BalanceState) + 4] = (char) (history->s_history[i].s_balance_pending_in >> 8);
//        buffer[2 + i * sizeof(BalanceState) + 5] = (char) history->s_history[i].s_balance_pending_in;
    }
}

void deserialize_history(const char *buffer, BalanceHistory *history) {
    history->s_id = buffer[0];
    history->s_history_len = (uint8_t) buffer[1];
    for (int i = 0; i < history->s_history_len; ++i) {
        deserialize_balance_state(&buffer[2 + i * sizeof(BalanceState)], &history->s_history[i]);
//
//        timestamp_t time;
//        balance_t balance;
//        balance_t balance_pending;
//
//        time = buffer[2 + i * sizeof(BalanceState)] << 8;
//        time |= buffer[2 + i * sizeof(BalanceState) + 1];
//
//        balance = buffer[2 + i * sizeof(BalanceState) + 2] << 8;
//        balance |= buffer[2 + i * sizeof(BalanceState) + 3];
//
//        balance_pending = buffer[2 + i * sizeof(BalanceState) + 4] << 8;
//        balance_pending |= buffer[2 + i * sizeof(BalanceState) + 5];
//
//        history->s_history[i].s_time = time;
//        history->s_history[i].s_balance = balance;
//        history->s_history[i].s_balance_pending_in = balance_pending;
    }
}
