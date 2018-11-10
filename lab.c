#include <stdio.h>
#include <unistd.h>
#include "core.h"
#include "pa1.h"
#include "distributed.h"

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

    sprintf(buffer, log_started_fmt, state->id, getpid(), getppid());
    if (broadcast_send(state, STARTED, buffer)) {
        return 1;
    }
    log_event(state, buffer);
    return 0;
}

int broadcast_done(ProcessState *state) {
    char buffer[MAX_PAYLOAD_LEN];

    sprintf(buffer, log_done_fmt, state->id);
    if (broadcast_send(state, DONE, buffer)) {
        return 1;
    }
    log_event(state, buffer);
    return 0;
}

int receive_started_from_all(ProcessState *state) {
    char buffer[MAX_PAYLOAD_LEN];

    if (receive_from_all(state, STARTED)) {
        return 1;
    }
    sprintf(buffer, log_received_all_started_fmt, state->id);
    log_event(state, buffer);
    return 0;
}

int receive_done_from_all(ProcessState *state) {
    char buffer[MAX_PAYLOAD_LEN];

    if (receive_from_all(state, DONE)) {
        return 1;
    }
    sprintf(buffer, log_received_all_done_fmt, state->id);
    log_event(state, buffer);
    return 0;
}

