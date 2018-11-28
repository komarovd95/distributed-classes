#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ipc.h"
#include "banking.h"
#include "core.h"
#include "transfers.h"
#include "pa2345.h"

/**
 * Decrements own balance and records it in balance history.
 *
 * @param state a state of current process
 * @param amount a transfer amount
 * @return 0 if success
 */
int do_decrement_balance(ProcessState *state, balance_t amount);

/**
 * Increments own balance and records it in balance history.
 *
 * @param state a state of current process
 * @param time a timestamp when transfer has been processed by transfer source
 * @param amount a transfer amount
 * @return 0 if success
 */
int do_increment_balance(ProcessState *state, timestamp_t time, balance_t amount);

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
    on_message_received(&message);

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
    BalanceState balance_state;

    if (state->balance < amount) {
        return 1;
    }
    state->balance -= amount;

    balance_state.s_balance = state->balance;
    balance_state.s_time = get_lamport_time();
    balance_state.s_balance_pending_in = state->history.s_history[state->history.s_history_len - 1].s_balance_pending_in;

    pad_history(&state->history, &balance_state);

    return 0;
}

int do_increment_balance(ProcessState *state, timestamp_t time, balance_t amount) {
    BalanceState balance_state;

    state->balance += amount;

    balance_state.s_time = get_lamport_time();
    balance_state.s_balance = state->balance;
    balance_state.s_balance_pending_in = 0;

    pad_history(&state->history, &balance_state);

    for (timestamp_t t = time; t < balance_state.s_time; ++t) {
        state->history.s_history[t].s_balance_pending_in += amount;
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

size_t serialize_history(char *buffer, const BalanceHistory *history) {
    size_t serialized_size;

    memcpy(buffer, &history->s_id, sizeof(history->s_id));
    serialized_size = sizeof(history->s_id);

    memcpy(buffer + serialized_size, &history->s_history_len, sizeof(history->s_history_len));
    serialized_size += sizeof(history->s_history_len);

    for (int i = 0; i < history->s_history_len; ++i) {
        serialized_size += serialize_balance_state(buffer + serialized_size, &history->s_history[i]);
    }
    return serialized_size;
}

void deserialize_history(const char *buffer, BalanceHistory *history) {
    size_t offset;

    memcpy(&history->s_id, buffer, sizeof(history->s_id));
    offset = sizeof(history->s_id);

    memcpy(&history->s_history_len, buffer + offset, sizeof(history->s_history_len));
    offset += sizeof(history->s_history_len);

    for (int i = 0; i < history->s_history_len; ++i) {
        deserialize_balance_state(buffer + offset, &history->s_history[i]);
        offset += BALANCE_STATE_SIZE;
    }
}

void pad_history(BalanceHistory *history, const BalanceState *state) {
    timestamp_t current_len;

    current_len = history->s_history_len;
    for (timestamp_t t = current_len; t < state->s_time; ++t) {
        history->s_history[t].s_balance = history->s_history[current_len - 1].s_balance;
        history->s_history[t].s_balance_pending_in = history->s_history[current_len - 1].s_balance_pending_in;
        history->s_history[t].s_time = t;
    }

    history->s_history[state->s_time].s_balance = state->s_balance;
    history->s_history[state->s_time].s_balance_pending_in = state->s_balance_pending_in;
    history->s_history[state->s_time].s_time = state->s_time;

    history->s_history_len = (uint8_t) (state->s_time + 1);
}
