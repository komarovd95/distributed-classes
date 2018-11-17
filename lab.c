#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "core.h"
#include "pa2345.h"
#include "transfers.h"

int broadcast_started(ProcessState *state);
int broadcast_stop(ProcessState *state);
int broadcast_done(ProcessState *state);
int receive_started_from_all(ProcessState *state);
int receive_done_from_all(ProcessState *state);
int receive_history_from_all(ProcessState *state);
int send_history(ProcessState *state);

int process_transfer_message(ProcessState *state, const Message *message);

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
        on_message_received(message.s_header.s_local_time);

        switch (message.s_header.s_type) {
            case TRANSFER:
                if (process_transfer_message(state, &message)) {
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

    if (broadcast_done(state)) {
        return 1;
    }

    while (state->done_received < (state->processes_count - 1)) {
        if (receive_any(state, &message)) {
            return 2;
        }
        on_message_received(message.s_header.s_local_time);

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
    }

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
        fprintf(stderr, "Failed to read done from all by parent!\n");
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
            on_message_received(message.s_header.s_local_time);

            if (message.s_header.s_type != STARTED) {
                return 2;
            }
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
        on_message_received(message.s_header.s_local_time);
        if (message.s_header.s_type != BALANCE_HISTORY) {
            return 2;
        }

        deserialize_history(message.s_payload, &all_history.s_history[id - 1]);
    }
    adjust_history(&all_history);
    print_history(&all_history);
    return 0;
}

int process_transfer_message(ProcessState *state, const Message *message) {
    TransferOrder transfer_order;

    deserialize_order(message->s_payload, &transfer_order);

    if (transfer_order.s_src == state->id) {
        if (process_transfer_out(state, &transfer_order)) {
            fprintf(stderr, "(%d) Failed to process transfer out!\n", state->id);
            return 1;
        }
        return 0;
    }
    if (transfer_order.s_dst == state->id) {
        if (process_transfer_in(state, message->s_header.s_local_time, &transfer_order)) {
            fprintf(stderr, "(%d) Failed to process transfer in!\n", state->id);
            return 1;
        }
        return 0;
    }

    fprintf(stderr, "(%d) Unknown transfer found!\n", state->id);
    return 1;
}

int send_history(ProcessState *state) {
    Message message;
    char buffer[MAX_PAYLOAD_LEN];

    serialize_history(buffer, &state->history);

    on_message_send();
    construct_message(&message, BALANCE_HISTORY, (size_t) (2 + state->history.s_history_len * BALANCE_STATE_SIZE), buffer);
    if (send(state, PARENT_ID, &message)) {
        return 1;
    }

    return 0;
}
