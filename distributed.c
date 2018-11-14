#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "distributed.h"
#include "pa2345.h"
#include "banking.h"

typedef struct {
    ProcessState state;
    local_id     sender;
} ReceiveAny;

/**
 * Serializes message to bytes.
 *
 * @param buffer a buffer for serialized message
 * @param message a message to serialize
 */
void serialize_message(unsigned char *buffer, const Message *message);

/**
 * Deserializes header of received message.
 *
 * @param buffer a buffer with serialized header
 * @param header a header that must be deserialized
 */
void deserialize_header(const unsigned char *buffer, MessageHeader *header);

int send_message(ProcessState *state, local_id to, int message_type, size_t payload_len, const char *payload) {
    Message message;

    message.s_header.s_magic = MESSAGE_MAGIC;
    message.s_header.s_type = (int16_t) message_type;
    message.s_header.s_payload_len = (uint16_t) payload_len;
    message.s_header.s_local_time = get_lamport_time();

    memcpy(message.s_payload, payload, payload_len);

    return send(state, to, &message);
}

int receive_from_any(ProcessState *state, local_id *sender, int *message_type, size_t *payload_len, char *payload) {
    Message message;
    ReceiveAny s_receive;

    s_receive.state = *state;
    if (receive_any(&s_receive, &message)) {
        fprintf(stderr, "(%d) Failed to receive any message!\n", state->id);
        return 1;
    }
    on_message_received(message.s_header.s_local_time);

    *message_type = message.s_header.s_type;
    *payload_len = message.s_header.s_payload_len;
    *sender = s_receive.sender;
    if (payload != NULL) {
        memcpy(payload, message.s_payload, *payload_len);
    }

    return 0;
}

int receive_from(ProcessState *state, local_id sender, int *message_type, char *payload) {
    Message message;

    if (receive(state, sender, &message)) {
        fprintf(stderr, "(%d) Failed to receive message from: from=%d\n", state->id, sender);
        return 1;
    }
    on_message_received(message.s_header.s_local_time);

    *message_type = message.s_header.s_type;
    if (payload != NULL) {
        memcpy(payload, message.s_payload, message.s_header.s_payload_len);
    }
    return 0;
}

int send_multicast(void *self, const Message *msg) {
    ProcessState *state;
    local_id id;

    state = (ProcessState *) self;

    for (id = 0; id <= state->processes_count; ++id) {
        if (id != state->id) {
            if (send(state, id, msg)) {
                fprintf(stderr, "(%d) Failed to write multicast message: to=%d\n", state->id, id);
                return 1;
            }
        }
    }

    return 0;
}

int send(void *self, local_id to, const Message *message) {
    size_t serialized_size;
    unsigned char buffer[MAX_PAYLOAD_LEN];
    ProcessState *state;
    ssize_t bytes_written;

    state = (ProcessState *) self;

    serialize_message(buffer, message);
    serialized_size = sizeof(MessageHeader) + message->s_header.s_payload_len;

    while ((bytes_written = write(state->writing_pipes[to], buffer, serialized_size)) < 0) {
        if (errno != EAGAIN) {
            fprintf(stderr, "(%d) Failed to send message to=%d (descriptor=%d) error=%s\n",
                    state->id, to, state->writing_pipes[to], strerror(errno));
            return 1;
        }
    }

    if (serialized_size != bytes_written) {
        fprintf(stderr, "(%d) Failed to send message to=%d (descriptor=%d) error=%s\n",
                state->id, to, state->writing_pipes[to], strerror(errno));
        return 2;
    }

    return 0;
}

void serialize_message(unsigned char *buffer, const Message *message) {
    buffer[0] = (unsigned char) message->s_header.s_magic >> 8;
    buffer[1] = (unsigned char) message->s_header.s_magic;

    buffer[2] = (unsigned char) message->s_header.s_type >> 8;
    buffer[3] = (unsigned char) message->s_header.s_type;

    buffer[4] = (unsigned char) message->s_header.s_payload_len >> 8;
    buffer[5] = (unsigned char) message->s_header.s_payload_len;

    buffer[6] = (unsigned char) message->s_header.s_local_time >> 8;
    buffer[7] = (unsigned char) message->s_header.s_local_time;

    memcpy(&buffer[8], message->s_payload, message->s_header.s_payload_len);
}

int receive(void *self, local_id from, Message *msg) {
    ProcessState *state;
    unsigned char buffer[MAX_MESSAGE_LEN];
    ssize_t bytes_read;

    state = (ProcessState *) self;
    while ((bytes_read = read(state->reading_pipes[from], buffer, sizeof(MessageHeader))) <= 0) {
        if (bytes_read < 0 && errno != EAGAIN) {
            fprintf(stderr, "(%d) Failed to read from pipe: descriptor=%d error=%s\n",
                    state->id, state->reading_pipes[from], strerror(errno));
            return 1;
        }
    }

    if (bytes_read != sizeof(MessageHeader)) {
        fprintf(stderr, "(%d) Failed to read from pipe: descriptor=%d bytes_read=%ld\n",
                state->id, state->reading_pipes[from], bytes_read);
        return 2;
    }

    deserialize_header(buffer, &msg->s_header);

    if (msg->s_header.s_payload_len) {
        while ((bytes_read = read(state->reading_pipes[from], msg->s_payload, msg->s_header.s_payload_len)) <= 0) {
            if (bytes_read < 0 && errno != EAGAIN) {
                fprintf(stderr, "(%d) Failed to read from pipe: descriptor=%d error=%s\n",
                        state->id, state->reading_pipes[from], strerror(errno));
                return 2;
            }
        }

        if (bytes_read != msg->s_header.s_payload_len) {
            fprintf(stderr, "(%d) Failed to read from pipe: descriptor=%d error=%s\n",
                    state->id, state->reading_pipes[from], strerror(errno));
            return 2;
        }
    }

    return 0;
}

int receive_any(void *self, Message *msg) {
    ReceiveAny *s_receive;
    ProcessState *state;
    unsigned char buffer[MAX_MESSAGE_LEN];
    ssize_t bytes_read;

    s_receive = (ReceiveAny *) self;
    state = &s_receive->state;
    while (1) {
        for (local_id id = 0; id <= state->processes_count; ++id) {
            if (id != state->id) {
                bytes_read = read(state->reading_pipes[id], buffer, sizeof(MessageHeader));
                if (bytes_read < 0 && errno != EAGAIN) {
                    fprintf(stderr, "(%d) Failed to read from pipe: descriptor=%d error=%s\n",
                            state->id, state->reading_pipes[id], strerror(errno));
                    return 1;
                }
                if (bytes_read > 0) {
                    if (bytes_read != sizeof(MessageHeader)) {
                        fprintf(stderr, "(%d) Failed to read from pipe: descriptor=%d bytes_read=%ld\n",
                                state->id, state->reading_pipes[id], bytes_read);
                        return 2;
                    }
                    deserialize_header(buffer, &msg->s_header);

                    if (msg->s_header.s_payload_len) {
                        while ((bytes_read = read(state->reading_pipes[id], msg->s_payload, msg->s_header.s_payload_len)) <= 0) {
                            if (bytes_read < 0 && errno != EAGAIN) {
                                fprintf(stderr, "(%d) Failed to read from pipe: descriptor=%d error=%s\n",
                                        state->id, state->reading_pipes[id], strerror(errno));
                                return 2;
                            }
                        }

                        if (bytes_read != msg->s_header.s_payload_len) {
                            fprintf(stderr, "(%d) Failed to read from pipe: descriptor=%d error=%s\n",
                                    state->id, state->reading_pipes[id], strerror(errno));
                            return 2;
                        }
                    }

                    s_receive->sender = id;

                    return 0;
                }
            }
        }
    }
}

void deserialize_header(const unsigned char *buffer, MessageHeader *header) {
    uint16_t magic;
    uint16_t payload_len;
    int16_t type;
    timestamp_t local_time;

    magic = buffer[0] << 8;
    magic |= buffer[1];

    type = buffer[2] << 8;
    type |= buffer[3];

    payload_len = buffer[4] << 8;
    payload_len |= buffer[5];

    local_time = buffer[6] << 8;
    local_time |= buffer[7];

    header->s_magic = magic;
    header->s_payload_len = payload_len;
    header->s_type = type;
    header->s_local_time = local_time;
}

void construct_message(Message *message, int message_type, size_t payload_len, const char *payload) {
    message->s_header.s_magic = MESSAGE_MAGIC;
    message->s_header.s_payload_len = (uint16_t) payload_len;
    message->s_header.s_type = (int16_t) message_type;
    message->s_header.s_local_time = get_lamport_time();

    if (payload_len) {
        memcpy(message->s_payload, payload, payload_len);
    }
}
