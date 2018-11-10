#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "distributed.h"
#include "pa1.h"

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

int broadcast_send(ProcessState *state, int message_type, const char *payload) {
    Message message;

    message.s_header.s_magic = MESSAGE_MAGIC;
    message.s_header.s_type = (int16_t) message_type;
    message.s_header.s_payload_len = (uint16_t) strlen(payload);
    message.s_header.s_local_time = 0;

    strcpy(message.s_payload, payload);

    return send_multicast(state, &message);
}

int receive_from_all(ProcessState *state, int message_type) {
    Message *message;

    message = malloc(sizeof(Message));
    for (local_id id = 1; id <= state->processes_count; ++id) {
        if (id != state->id) {
            if (receive(state, id, message)) {
                fprintf(stderr, "(%d) Failed to receive message from: from=%d\n", state->id, id);
                free(message);
                return 1;
            }
            if (message->s_header.s_type != message_type) {
                fprintf(stderr, "(%d) Message has incorrect type: type=%d\n", state->id, message->s_header.s_type);
                free(message);
                return 2;
            }
        }
    }

    free(message);
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

    state = (ProcessState *) self;

    serialize_message(buffer, message);
    serialized_size = sizeof(MessageHeader) + message->s_header.s_payload_len;

    if (serialized_size != write(state->writing_pipes[to], buffer, serialized_size)) {
        fprintf(stderr, "(%d) Failed to send message to=%d (descriptor=%d) error=%s\n",
                state->id, to, state->writing_pipes[to], strerror(errno));
        return 1;
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
        if (bytes_read < 0) {
            fprintf(stderr, "(%d) Failed to read from pipe: descriptor=%d error=%s\n",
                    state->id, state->reading_pipes[from], strerror(errno));
            return 1;
        }
    }

    deserialize_header(buffer, &msg->s_header);

    while ((bytes_read = read(state->reading_pipes[from], msg->s_payload, msg->s_header.s_payload_len)) <= 0) {
        if (bytes_read < 0) {
            fprintf(stderr, "(%d) Failed to read from pipe: descriptor=%d error=%s\n",
                    state->id, state->reading_pipes[from], strerror(errno));
            return 2;
        }
    }

    return 0;
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
