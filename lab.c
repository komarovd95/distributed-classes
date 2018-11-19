#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "core.h"
#include "pa2345.h"

#define ITERATIONS_COUNT 5

int broadcast_started(ProcessState *state);
int broadcast_done(ProcessState *state);
int broadcast_cs_request(ProcessState *state);
int broadcast_cs_release(ProcessState *state);
int receive_started_from_all(ProcessState *state);
int receive_done_from_all(ProcessState *state);

int process_self_cs_request(ProcessState *state, const CsRequest *sent_request, int *reply_received);
int process_received_cs_request(ProcessState *state, const CsRequest *request);

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

//        printf("(%d) Queue: ", state->id);
//        for (int j = 1; j < state->queue.length; ++j) {
//            printf("(%d, %d), ", state->queue.requests[j].timestamp, state->queue.requests[j].process_id);
//        }
//        printf("(%d, %d)\n", state->queue.requests[state->queue.length].timestamp, state->queue.requests[state->queue.length].process_id);
//        fflush(stdout);
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
    int reply_received[MAX_PROCESS_ID];
    CsRequest sent_request, q_request;

    state = (ProcessState *) self;
    if (state->use_mutex == 0) {
        return 0;
    }

    memcpy(reply_received, state->done_received, MAX_PROCESS_ID * sizeof(int));

    on_message_send();

    sent_request.timestamp = get_lamport_time();
    sent_request.process_id = state->id;
    sent_request.replied = 0;

    if (enqueue(&state->queue, &sent_request)) {
        fprintf(stderr, "(%d) Failed to enqueue sent CS requests: time=%d\n", state->id, get_lamport_time());
        return 1;
    }
    if (broadcast_cs_request(state)) {
        fprintf(stderr, "(%d) Failed to broadcast CS requests!\n", state->id);
        return 2;
    }

    while (1) {
        if (peek(&state->queue, &q_request)) {
            fprintf(stderr, "(%d) Failed to peek top of requests queue!\n", state->id);
            return 3;
        }

        if (q_request.process_id == state->id) {
            int process_result;
            if ((process_result = process_self_cs_request(state, &sent_request, reply_received)) > 0) {
                fprintf(stderr, "(%d) Failed to process self CS request!\n", state->id);
                return 4;
            }
            if (!process_result) {
                return 0;
            }
        } else if (process_received_cs_request(state, &q_request)) {
            fprintf(stderr, "(%d) Failed to process received CS request: from=%d!\n", state->id, q_request.process_id);
            return 5;
        }
    }
}

int release_cs(const void * self) {
    ProcessState *state;

    state = (ProcessState *) self;
    if (state->use_mutex == 0) {
        return 0;
    }

    if (broadcast_cs_release(state)) {
        fprintf(stderr, "(%d) Failed to broadcast CS release!\n", state->id);
        return 1;
    }
    if (dequeue(&state->queue, NULL)) {
        fprintf(stderr, "(%d) Failed to dequeue self CS request!\n", state->id);
        return 2;
    }
    return 0;
}

int broadcast_cs_request(ProcessState *state) {
    Message message;

    construct_message(&message, CS_REQUEST, 0, NULL);
    for (local_id id = 1; id <= state->processes_count; ++id) {
        if (id != state->id && !state->done_received[id]) {
            if (send(state, id, &message)) {
                fprintf(stderr, "(%d) Failed to send CS request: to=%d\n", state->id, id);
                return 1;
            }
        }
    }
    return 0;
}

int broadcast_cs_release(ProcessState *state) {
    Message message;

    construct_message(&message, CS_RELEASE, 0, NULL);
    for (local_id id = 1; id <= state->processes_count; ++id) {
        if (id != state->id && !state->done_received[id]) {
            if (send(state, id, &message)) {
                fprintf(stderr, "(%d) Failed to send CS release: to=%d\n", state->id, id);
                return 1;
            }
        }
    }
    return 0;
}

int process_self_cs_request(ProcessState *state, const CsRequest *sent_request, int *reply_received) {
    Message message;
    CsRequest received_request;
    int should_continue;
    int replies_received;

    replies_received = 0;
    for (int i = 1; i <= state->processes_count; ++i) {
        if (i != state->id) {
            replies_received += reply_received[i];
        }
    }

    should_continue = 1;
    while (replies_received < (state->processes_count - 1) && should_continue) {
        for (local_id id = 1; id <= state->processes_count; ++id) { //&& should_continue; ++id) {
            if (id != state->id && !reply_received[id]) {
                if (receive(state, id, &message)) {
                    fprintf(stderr, "(%d) Failed to receive CS message: from=%d\n", state->id, id);
                    return 1;
                }
                on_message_received(message.s_header.s_local_time);
    
                switch (message.s_header.s_type) {
                    case CS_REPLY:
//                        printf("(%d) Receive reply: %d\n", state->id, id);
                        reply_received[id] = 1;
                        replies_received++;
                        break;
    
                    case CS_REQUEST:
                        received_request.timestamp = message.s_header.s_local_time;
                        received_request.process_id = id;
    
                        if (timestamp_compare(sent_request, &received_request) >= 0) {
                            should_continue = 0;
                        }
//                        printf("(%d) Enqueue: %d\n", state->id, id);
                        if (enqueue(&state->queue, &received_request)) {
                            fprintf(stderr, "(%d) Failed to enqueue received CS requests: from=%d\n", state->id, id);
                            return 2;
                        }
//                        printf("(%d) Queue: ", state->id);
//                        for (int j = 1; j < state->queue.length; ++j) {
//                            printf("(%d, %d), ", state->queue.requests[j].timestamp, state->queue.requests[j].process_id);
//                        }
//                        printf("(%d, %d)\n", state->queue.requests[state->queue.length].timestamp, state->queue.requests[state->queue.length].process_id);
//                        fflush(stdout);
                        break;
    
                    case DONE:
                        reply_received[id] = 1;
                        replies_received++;
                        state->done_received[id] = 1;
                        break;
    
                    default:
                        fprintf(stderr, "(%d) Unknown message type: from=%d type=%d\n", state->id, id, message.s_header.s_type);
                        return 3;
                }
            }
        }
    }

    if (should_continue) {
        return 0;
    }
    return -1;
}

int process_received_cs_request(ProcessState *state, const CsRequest *request) {
    Message message;

//    printf("(%d) Process: %d\n", state->id, request->process_id);

    on_message_send();
    construct_message(&message, CS_REPLY, 0, NULL);
    if (send(state, request->process_id, &message)) {
        fprintf(stderr, "(%d) Failed to send CS reply: to=%d\n", state->id, request->process_id);
        return 1;
    }

    if (receive(state, request->process_id, &message)) {
        fprintf(stderr, "(%d) Failed to receive CS message: from=%d\n", state->id, request->process_id);
        return 2;
    }

    if (message.s_header.s_type != CS_RELEASE) {
        fprintf(stderr, "(%d) Failed to receive RELEASE event: from=%d type=%d!\n",
                state->id, request->process_id, message.s_header.s_type);
        return 3;
    }
    if (dequeue(&state->queue, NULL)) {
        fprintf(stderr, "(%d) Failed to dequeue REQUEST event: from=%d\n", state->id, request->process_id);
        return 4;
    }
//    printf("(%d) De-Queue: ", state->id);
//    for (int j = 1; j < state->queue.length; ++j) {
//        printf("(%d, %d), ", state->queue.requests[j].timestamp, state->queue.requests[j].process_id);
//    }
//    printf("(%d, %d)\n", state->queue.requests[state->queue.length].timestamp, state->queue.requests[state->queue.length].process_id);
//    fflush(stdout);
    return 0;
}
