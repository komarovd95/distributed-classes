#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "core.h"
#include "pa2345.h"

#define ITERATIONS_COUNT 5

int broadcast_started(ProcessState *state);
int broadcast_done(ProcessState *state);
int receive_started_from_all(ProcessState *state);
int receive_done_from_all(ProcessState *state);

int process_controlled_fork(ProcessState *state, local_id id);
int process_uncontrolled_fork(ProcessState *state, local_id id);

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
    char buffer[1024];
    int iterations;

    iterations = ITERATIONS_COUNT * state->id;
    for (int i = 1; i <= iterations; ++i) {
        if (request_cs(state)) {
            fprintf(stderr, "(%d) Failed to requests CS!\n", state->id);
            return 1;
        }

        sprintf(buffer, log_loop_operation_fmt, state->id, i, iterations);
        print(buffer);

        if (release_cs(state)) {
            fprintf(stderr, "(%d) Failed to release CS!\n", state->id);
            return 2;
        }
    }
    return 0;
}

int child_phase_3(ProcessState *state) {
    int done_count;
    Message message;
    char buffer[MAX_PAYLOAD_LEN];

    if (broadcast_done(state)) {
        fprintf(stderr, "(%d) Failed to broadcast DONE event!\n", state->id);
        return 1;
    }

    done_count = 0;
    for (int i = 1; i <= state->processes_count; ++i) {
        if (i != state->id) {
            done_count += state->done_received[i];
        }
    }

    while (done_count < (state->processes_count - 1)) {
        if (receive_any(state, &message)) {
            fprintf(stderr, "(%d) Failed to receive any message in DONE stage!\n", state->id);
            return 1;
        }
        on_message_received(message.s_header.s_local_time);

        if (message.s_header.s_type != DONE) {
            fprintf(stderr, "(%d) Not a DONE event received: type=%d!\n", state->id, message.s_header.s_type);
            return 2;
        }

        done_count++;
    }
    sprintf(buffer, log_received_all_done_fmt, get_lamport_time(), state->id);
    log_event(state, buffer);
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
    Message message;
    char buffer[MAX_PAYLOAD_LEN];

    on_message_send();
    sprintf(buffer, log_started_fmt, get_lamport_time(), state->id, getpid(), getppid(), 0);
    construct_message(&message, STARTED, strlen(buffer), buffer);
    if (send_multicast(state, &message)) {
        return 1;
    }
    log_event(state, buffer);
    return 0;
}

int broadcast_done(ProcessState *state) {
    Message message;
    char buffer[MAX_PAYLOAD_LEN];

    on_message_send();
    sprintf(buffer, log_done_fmt, get_lamport_time(), state->id, 0);
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

void transfer(void * parent_data, local_id src, local_id dst, balance_t amount) {
    fprintf(stderr, "Transfers not allowed in PA3!\n");
    exit(1);
}

int request_cs(const void * self) {
    ProcessState *state;
    int fork_received[MAX_PROCESS_ID];
    int forks_received;

    state = (ProcessState *) self;
    if (state->use_mutex == 0) {
        return 0;
    }

    memcpy(fork_received, state->done_received, sizeof(int) * MAX_PROCESS_ID);

    forks_received = 0;
    for (int i = 1; i <= state->processes_count; ++i) {
        if (i != state->id) {
            forks_received += state->done_received[i];
        }
    }

    while (forks_received < (state->processes_count - 1)) {
        for (local_id id = 1; id <= state->processes_count; ++id) {
            if (id != state->id && !fork_received[id]) {
//                printf("(%d) Forks: ", state->id);
//                for (int i = 1; i <= state->processes_count; ++i) {
//                    if (i != state->id) {
//                        printf("(%d, %d, %d) ", state->forks[i].is_controlled, state->forks[i].is_dirty, state->forks[i].has_request);
//                    } else {
//                        printf("(-1, -1, -1) ");
//                    }
//                }
//                printf("\n");


                if (state->forks[id].is_controlled) {
                    if (process_controlled_fork(state, id)) {
                        fprintf(stderr, "(%d) Failed to process controlled fork: id=%d\n", state->id, id);
                        return 1;
                    }
                } else {
                    if (process_uncontrolled_fork(state, id)) {
                        fprintf(stderr, "(%d) Failed to process uncontrolled fork: id=%d\n", state->id, id);
                        return 2;
                    }
                }

                fork_received[id] = (state->forks[id].is_controlled && !state->forks[id].is_dirty);
                forks_received += fork_received[id];
            }
        }
    }

    for (int i = 1; i <= state->processes_count; ++i) {
        if (i != state->id) {
            state->forks[i].is_dirty = 1;
        }
    }
    return 0;
}

int release_cs(const void * self) {
    ProcessState *state;
    Message message;

    state = (ProcessState *) self;
    if (state->use_mutex == 0) {
        return 0;
    }

    for (local_id id = 1; id <= state->processes_count; ++id) {
        if (id != state->id && !state->done_received[id]) {
            if (receive(state, id, &message)) {
                fprintf(stderr, "(%d) Failed to receive message (RELEASE) from: %d\n", state->id, id);
                return 1;
            }
            on_message_received(message.s_header.s_local_time);

            switch (message.s_header.s_type) {
                case CS_REQUEST:
                    state->forks[id].has_request = 1;

                    on_message_send();
                    construct_message(&message, CS_REPLY, 0, NULL);
                    if (send(state, id, &message)) {
                        fprintf(stderr, "(%d) Failed to send CS reply to: %d\n", state->id, id);
                        return 1;
                    }
                    state->forks[id].is_controlled = 0;
                    state->forks[id].is_dirty = 0;
                    break;
                case DONE:
                    state->done_received[id] = 1;
                    state->forks[id].is_controlled = 1;
                    state->forks[id].is_dirty = 0;
                    state->forks[id].has_request = 0;
                    break;

                default:
                    fprintf(stderr, "(%d) Unknown type (DIRTY stage): %d\n", state->id, message.s_header.s_type);
                    return 3;
            }
        }
    }
    return 0;
}

int process_controlled_fork(ProcessState *state, local_id id) {
    Message message;

    if (state->forks[id].has_request) {
        on_message_send();
        construct_message(&message, CS_REPLY, 0, NULL);
        if (send(state, id, &message)) {
            fprintf(stderr, "(%d) Failed to send CS reply to: %d\n", state->id, id);
            return 1;
        }
        state->forks[id].is_controlled = 0;
        state->forks[id].is_dirty = 0;
    } else {
        if (receive(state, id, &message)) {
            fprintf(stderr, "(%d) Failed to receive CS request from: %d\n", state->id, id);
            return 2;
        }
        on_message_received(message.s_header.s_local_time);

        switch (message.s_header.s_type) {
            case CS_REQUEST:
                state->forks[id].has_request = 1;
                break;
            case DONE:
                state->done_received[id] = 1;
                state->forks[id].is_controlled = 1;
                state->forks[id].is_dirty = 0;
                state->forks[id].has_request = 0;
                break;

            default:
                fprintf(stderr, "(%d) Unknown type (DIRTY stage): %d\n", state->id, message.s_header.s_type);
                return 3;
        }
    }
    return 0;
}

int process_uncontrolled_fork(ProcessState *state, local_id id) {
    Message message;

    if (state->forks[id].has_request) {
        on_message_send();
        construct_message(&message, CS_REQUEST, 0, NULL);
        if (send(state, id, &message)) {
            fprintf(stderr, "(%d) Failed to send CS request to: %d\n", state->id, id);
            return 1;
        }
        state->forks[id].has_request = 0;
    } else {
        if (receive(state, id, &message)) {
            fprintf(stderr, "(%d) Failed to receive CS reply from: %d\n", state->id, id);
            return 2;
        }
        on_message_received(message.s_header.s_local_time);

        switch (message.s_header.s_type) {
            case CS_REPLY:
                state->forks[id].is_controlled = 1;
                state->forks[id].is_dirty = 0;
                break;
            case DONE:
                state->done_received[id] = 1;
                state->forks[id].is_controlled = 1;
                state->forks[id].is_dirty = 0;
                state->forks[id].has_request = 0;
                break;

            default:
                fprintf(stderr, "(%d) Unknown type (REPLY stage): %d\n", state->id, message.s_header.s_type);
                return 3;
        }
    }
    return 0;
}
