#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "core.h"
#include "pa1.h"

int broadcast_started(ProcessState *state);
int broadcast_done(ProcessState *state);
int receive_started_from_all(ProcessState *state);
int receive_done_from_all(ProcessState *state);

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
    return 0;
}

int child_phase_3(ProcessState *state) {
    if (broadcast_done(state)) {
        return 1;
    }
    if (receive_done_from_all(state)) {
        return 2;
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

    sprintf(buffer, log_started_fmt, state->id, getpid(), getppid());
    construct_message(&message, STARTED, strlen(buffer), buffer);
    if (send_multicast(state, &message)) {
        return 1;
    }
    log_event(state, buffer);
    return 0;
}

int broadcast_done(ProcessState *state) {
    char buffer[MAX_PAYLOAD_LEN];
    Message message;

    sprintf(buffer, log_done_fmt, state->id);
    construct_message(&message, DONE, strlen(buffer), buffer);
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

            if (message.s_header.s_type != STARTED) {
                return 2;
            }
        }
    }

    sprintf(buffer, log_received_all_started_fmt, state->id);
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

            if (message.s_header.s_type != DONE) {
                return 2;
            }
        }
    }

    sprintf(buffer, log_received_all_done_fmt, state->id);
    log_event(state, buffer);
    return 0;
}

