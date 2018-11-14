#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "core.h"
#include "pa2345.h"
#include "distributed.h"
#include "transfers.h"

int broadcast_started(ProcessState *state);
int broadcast_stop(ProcessState *state);
int broadcast_done(ProcessState *state);
int receive_started_from_all(ProcessState *state);
int receive_done_from_all(ProcessState *state);
int receive_history_from_all(ProcessState *state);
int send_history(ProcessState *state);

int process_transfer_message(ProcessState *state, const char *payload);
//void serialize_history(char *buffer, const BalanceHistory *history);
//void deserialize_history(const char *buffer, BalanceHistory *history);

int child_phase_1(ProcessState *state) {
    if (broadcast_started(state)) {
        return 1;
    }
    if (receive_started_from_all(state)) {
        return 2;
    }
    return 0;
}

int child_phase_2(ProcessState *state) {
    Message message;
    int stopped;

    stopped = 0;
    while (!stopped) {
        if (receive_any(state, &message)) {
            return 1;
        }

        switch (message.s_header.s_type) {

//        }
//
//
//        size_t payload_len;
//        int message_type;
//        local_id sender;
//
//        if (receive_from_any(state, &sender, &message_type, &payload_len, buffer)) {
//            fprintf(stderr, "(%d) Failed to receive message from any!\n", state->id);
//            return 1;
//        }
//
//        switch (message_type) {
            case TRANSFER:
                if (process_transfer_message(state, message.s_payload)) {
                    fprintf(stderr, "(%d) Failed to process transfer message!\n", state->id);
                    return 2;
                }
                break;
            case STOP:
                stopped = 1;
                break;
            case DONE:
                state->done_received++;
                break;
            default:
                fprintf(stderr, "(%d) Unknown message type: %d\n", state->id, message.s_header.s_type);
                return 2;
        }
    }
    return 0;
}

int child_phase_3(ProcessState *state) {
    Message message;
//    int done_received;
//    char buffer[MAX_PAYLOAD_LEN];

    if (broadcast_done(state)) {
        return 1;
    }

    while (state->done_received < (state->processes_count - 1)) {
        if (receive_any(state, &message)) {
            return 2;
        }

        switch (message.s_header.s_type) {
            case TRANSFER:
                // no-op process is stopped
                break;
            case DONE:
                state->done_received++;
                break;
            default:
                fprintf(stderr, "(%d) Unknown message type: %d\n", state->id, message.s_header.s_type);
                return 3;
        }

//        done_received = receive_any(state, &message);
//        if (done_received) {
//            if (done_received == -1) {
//
//            }
//        }
    }

//    done_received = 0;
//    while (done_received < (state->processes_count - 1)) {
//        size_t payload_len;
//        int message_type;
//        local_id sender;
//
//        if (receive_from_any(state, &sender, &message_type, &payload_len, buffer)) {
//            fprintf(stderr, "(%d) Failed to receive message from any!\n", state->id);
//            return 2;
//        }
//
//        switch (message_type) {
//            case TRANSFER:
//                // no-op process is stopped
//                break;
//            case DONE:
//                done_received++;
//                break;
//            default:
//                fprintf(stderr, "(%d) Unknown message type: %d\n", state->id, message_type);
//                return 3;
//        }
//    }

    if (send_history(state)) {
        return 4;
    }
    return 0;
}

int parent_phase_1(ProcessState *state) {
    if (receive_started_from_all(state)) {
        return 1;
    }
    return 0;
}

int parent_phase_2(ProcessState *state) {
    bank_robbery(state, (local_id) state->processes_count);
    if (broadcast_stop(state)) {
        return 1;
    }
    return 0;
}

int parent_phase_3(ProcessState *state) {
    if (receive_done_from_all(state)) {
        return 1;
    }
    if (receive_history_from_all(state)) {
        return 2;
    }
    return 0;
}

int broadcast_started(ProcessState *state) {
    Message message;
    char buffer[MAX_PAYLOAD_LEN];

    on_message_send();
    sprintf(buffer, log_started_fmt, get_lamport_time(), state->id, getpid(), getppid(), state->balance);
    construct_message(&message, STARTED, strlen(buffer), buffer);
    if (send_multicast(state, &message)) {
        return 1;
    }
    log_event(state, buffer);
    return 0;
}

int broadcast_stop(ProcessState *state) {
    Message message;

    on_message_send();
    construct_message(&message, STOP, 0, NULL);
    if (send_multicast(state, &message)) {
        return 1;
    }
    return 0;
}

int broadcast_done(ProcessState *state) {
    Message message;
    char buffer[MAX_PAYLOAD_LEN];

    on_message_send();
    sprintf(buffer, log_done_fmt, get_lamport_time(), state->id, state->balance);
    construct_message(&message, DONE, strlen(buffer), buffer);
    if (send_multicast(state, &message)) {
        return 1;
    }
    log_event(state, buffer);
    return 0;
}

int receive_started_from_all(ProcessState *state) {
    Message message;
    char buffer[MAX_PAYLOAD_LEN];

    for (local_id id = 1; id <= state->processes_count; ++id) {
        if (id != state->id) {
            if (receive(state, id, &message)) {
                return 1;
            }
            if (message.s_header.s_type != STARTED) {
                return 2;
            }
            on_message_received(message.s_header.s_local_time);
        }
    }
    sprintf(buffer, log_received_all_started_fmt, get_lamport_time(), state->id);
    log_event(state, buffer);
    return 0;
}

int receive_done_from_all(ProcessState *state) {
    Message message;
    char buffer[MAX_PAYLOAD_LEN];

    for (local_id id = 1; id <= state->processes_count; ++id) {
        if (id != state->id) {
            if (receive(state, id, &message)) {
                return 1;
            }
            if (message.s_header.s_type != DONE) {
                return 2;
            }
            on_message_received(message.s_header.s_local_time);
        }
    }
    sprintf(buffer, log_received_all_done_fmt, get_lamport_time(), state->id);
    log_event(state, buffer);
    return 0;
}

int receive_history_from_all(ProcessState *state) {
    Message message;
    AllHistory all_history;

    all_history.s_history_len = (uint8_t) state->processes_count;
    for (local_id id = 1; id <= state->processes_count; ++id) {
        if (receive(state, id, &message)) {
            return 1;
        }
        if (message.s_header.s_type != BALANCE_HISTORY) {
            return 2;
        }
        on_message_received(message.s_header.s_local_time);

        deserialize_history(message.s_payload, &all_history.s_history[id]);
    }
    print_history(&all_history);
    return 0;
}

int process_transfer_message(ProcessState *state, const char *payload) {
    TransferOrder transfer_order;
    char buffer[MAX_PAYLOAD_LEN];

    deserialize_order(payload, &transfer_order);

    if (transfer_order.s_src == state->id) {
        on_message_send();
        if (process_transfer_out(state, &transfer_order)) {
            fprintf(stderr, "(%d) Failed to process transfer out!\n", state->id);
            return 1;
        }
        sprintf(buffer, log_transfer_out_fmt,
                get_lamport_time(), transfer_order.s_src, transfer_order.s_amount, transfer_order.s_dst);
        log_event(state, buffer);
        return 0;
    }
    if (transfer_order.s_dst == state->id) {
        on_message_send();
        if (process_transfer_in(state, &transfer_order)) {
            fprintf(stderr, "(%d) Failed to process transfer in!\n", state->id);
            return 1;
        }
        sprintf(buffer, log_transfer_in_fmt,
                get_lamport_time(), transfer_order.s_dst, transfer_order.s_amount, transfer_order.s_src);
        log_event(state, buffer);
        return 0;
    }

    fprintf(stderr, "(%d) Unknown transfer found!\n", state->id);
    return 1;
}

int send_history(ProcessState *state) {
    char buffer[MAX_PAYLOAD_LEN];

    serialize_history(buffer, &state->history);

    on_message_send();
    if (send_message(state, PARENT_ID, BALANCE_HISTORY,
            sizeof(local_id) + sizeof(uint8_t) + state->history.s_history_len * sizeof(BalanceState), buffer)) {
        return 1;
    }

    return 0;
}


//void serialize_history(char *buffer, const BalanceHistory *history) {
//    buffer[0] = history->s_id;
//    buffer[1] = history->s_history_len;
//
//    for (int i = 0; i < history->s_history_len; ++i) {
//        buffer[2 + i * sizeof(BalanceState)] = (char) (history->s_history[i].s_time >> 8);
//        buffer[2 + i * sizeof(BalanceState) + 1] = (char) history->s_history[i].s_time;
//
//        buffer[2 + i * sizeof(BalanceState) + 2] = (char) (history->s_history[i].s_balance >> 8);
//        buffer[2 + i * sizeof(BalanceState) + 3] = (char) history->s_history[i].s_balance;
//
//        buffer[2 + i * sizeof(BalanceState) + 4] = (char) (history->s_history[i].s_balance_pending_in >> 8);
//        buffer[2 + i * sizeof(BalanceState) + 5] = (char) history->s_history[i].s_balance_pending_in;
//    }
//}

//void deserialize_history(const char *buffer, BalanceHistory *history) {
//    uint8_t history_len;
//
//    history_len = (uint8_t) buffer[1];
//
//    history->s_history_len = history_len;
//    for (int i = 0; i < history_len; ++i) {
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
//    }
//}
