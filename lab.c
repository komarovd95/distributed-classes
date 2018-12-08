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
    timestamp_t snapshot_time;
    int stopped;

    stopped = 0;
    while (!stopped) {
        if (receive_any(state, &message)) {
            return 1;
        }

        switch (message.s_header.s_type) {
            case TRANSFER:
                on_message_received(state, &message);
                if (process_transfer_message(state, &message)) {
                    fprintf(stderr, "(%d) Failed to process transfer message!\n", state->id);
                    return 2;
                }
                break;
            case STOP:
                on_message_received(state, &message);
                stopped = 1;
                break;
            case SNAPSHOT_VTIME:
                snapshot_time = message.s_header.s_local_timevector[PARENT_ID];

                construct_message(state, &message, SNAPSHOT_ACK, 0, NULL);
                if (send(state, PARENT_ID, &message)) {
                    fprintf(stderr, "(%d) Failed to send VTIME ACK!\n", state->id);
                    return 3;
                }
                if (sync_balance(state, snapshot_time)) {
                    fprintf(stderr, "(%d) Failed to sync balance\n", state->id);
                    return 4;
                }
                break;
            case DONE:
                on_message_received(state, &message);
                state->done_received++;
                break;
            default:
                fprintf(stderr, "(%d) Unknown message type: %d\n", state->id, message.s_header.s_type);
                return 5;
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
        on_message_received(state, &message);

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
    return 0;
}

int broadcast_started(ProcessState *state) {
    char buffer[MAX_PAYLOAD_LEN];
    Message message;

    on_message_send(state);
    sprintf(buffer, log_started_fmt, state->vector_time[state->id], state->id, getpid(), getppid(), state->balance);
    construct_message(state, &message, STARTED, strlen(buffer), buffer);
    if (send_multicast(state, &message)) {
        return 1;
    }
    log_event(state, buffer);
    return 0;
}

int broadcast_stop(ProcessState *state) {
    Message message;

    on_message_send(state);
    construct_message(state, &message, STOP, 0, NULL);
    if (send_multicast(state, &message)) {
        return 1;
    }
    return 0;
}

int broadcast_done(ProcessState *state) {
    char buffer[MAX_PAYLOAD_LEN];
    Message message;

    on_message_send(state);
    sprintf(buffer, log_done_fmt, state->vector_time[state->id], state->id, state->balance);
    construct_message(state, &message, DONE, strlen(buffer), buffer);
    if (send_multicast(state, &message)) {
        return 1;
    }
    log_event(state, buffer);
    return 0;
}

int receive_started_from_all(ProcessState *state) {
    char buffer[MAX_PAYLOAD_LEN];
    Message message;

    for (local_id id = 1; id <= state->processes_count; ++id) {
        if (id != state->id) {
            if (receive(state, id, &message)) {
                return 1;
            }
            on_message_received(state, &message);

            if (message.s_header.s_type != STARTED) {
                return 2;
            }
        }
    }
    sprintf(buffer, log_received_all_started_fmt, state->vector_time[state->id], state->id);
    log_event(state, buffer);
    return 0;
}

int receive_done_from_all(ProcessState *state) {
    char buffer[MAX_PAYLOAD_LEN];
    Message message;

    for (local_id id = 1; id <= state->processes_count; ++id) {
        if (id != state->id) {
            if (receive(state, id, &message)) {
                return 1;
            }
            on_message_received(state, &message);

            if (message.s_header.s_type != DONE) {
                return 2;
            }
        }
    }
    sprintf(buffer, log_received_all_done_fmt, state->vector_time[state->id], state->id);
    log_event(state, buffer);
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
        if (process_transfer_in(state, &transfer_order)) {
            fprintf(stderr, "(%d) Failed to process transfer in!\n", state->id);
            return 1;
        }
        return 0;
    }

    fprintf(stderr, "(%d) Unknown transfer found!\n", state->id);
    return 1;
}
