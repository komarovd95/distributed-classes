#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "core.h"
#include "pa2345.h"
#include "banking.h"

#define MESSAGE_HEADER_SIZE (sizeof(uint16_t) + sizeof(uint16_t) + sizeof(int16_t))

/**
 * Serializes message to byte buffer.
 *
 * @param buffer a buffer for serialized message
 * @param message a message to serialize
 * @return size of serialized message
 */
size_t serialize_message(char *buffer, const Message *message);

/**
 * Deserializes header of received message.
 *
 * @param buffer a buffer with serialized header
 * @param header a header that must be deserialized
 */
void deserialize_header(const char *buffer, MessageHeader *header);

int send_multicast(void *self, const Message *msg) {
    ProcessState *state;

    state = (ProcessState *) self;

    for (local_id id = 0; id <= state->processes_count; ++id) {
        if (id != state->id) {
            if (send(state, id, msg)) {
                fprintf(stderr, "(%d) Failed to send multicast message: to=%d\n", state->id, id);
                return 1;
            }
        }
    }
    return 0;
}

int send(void *self, local_id to, const Message *message) {
    size_t serialized_size;
    char buffer[MAX_PAYLOAD_LEN];
    ProcessState *state;
    ssize_t bytes_written;

    state = (ProcessState *) self;

    serialized_size = serialize_message(buffer, message);
    while ((bytes_written = write(state->writing_pipes[to], buffer, serialized_size)) < 0) {
        if (errno != EAGAIN) {
            fprintf(stderr, "(%d) Failed to send message to=%d (descriptor=%d) error=%s\n",
                    state->id, to, state->writing_pipes[to], strerror(errno));
            return 1;
        }
    }

    if (serialized_size != bytes_written) {
        fprintf(stderr, "(%d) Failed to send message to=%d (descriptor=%d) bytes_written=%ld\n",
                state->id, to, state->writing_pipes[to], bytes_written);
        return 2;
    }

    return 0;
}

size_t serialize_message(char *buffer, const Message *message) {
    size_t serialized_size;

    memcpy(buffer, &message->s_header.s_magic, sizeof(message->s_header.s_magic));
    serialized_size = sizeof(message->s_header.s_magic);

    memcpy(buffer + serialized_size, &message->s_header.s_type, sizeof(message->s_header.s_type));
    serialized_size += sizeof(message->s_header.s_type);

    memcpy(buffer + serialized_size, &message->s_header.s_payload_len, sizeof(message->s_header.s_payload_len));
    serialized_size += sizeof(message->s_header.s_payload_len);

    if (message->s_header.s_payload_len) {
        memcpy(&buffer[serialized_size], message->s_payload, message->s_header.s_payload_len);
    }

    return serialized_size + message->s_header.s_payload_len;
}

void deserialize_header(const char *buffer, MessageHeader *header) {
    size_t offset;

    memcpy(&header->s_magic, buffer, sizeof(header->s_magic));
    offset = sizeof(header->s_magic);

    memcpy(&header->s_type, buffer + offset, sizeof(header->s_type));
    offset += sizeof(header->s_type);

    memcpy(&header->s_payload_len, buffer + offset, sizeof(header->s_payload_len));
}

int receive(void *self, local_id from, Message *msg) {
    ProcessState *state;
    char buffer[MAX_MESSAGE_LEN];
    ssize_t bytes_read;
    size_t vector_size;

    state = (ProcessState *) self;
    while ((bytes_read = read(state->reading_pipes[from], buffer, MESSAGE_HEADER_SIZE)) <= 0) {
        if (bytes_read < 0 && errno != EAGAIN) {
            fprintf(stderr, "(%d) Failed to read message header from pipe: descriptor=%d error=%s\n",
                    state->id, state->reading_pipes[from], strerror(errno));
            return 1;
        }
        if (bytes_read == 0) {
            return 2;
        }
    }

    if (bytes_read != MESSAGE_HEADER_SIZE) {
        fprintf(stderr, "(%d) Failed to read message header: from=%d bytes_read=%ld\n",
                state->id, from, bytes_read);
        return 2;
    }
    deserialize_header(buffer, &msg->s_header);

    if (msg->s_header.s_payload_len == 0) {
        return 0;
    }

    while ((bytes_read = read(state->reading_pipes[from], buffer, msg->s_header.s_payload_len)) <= 0) {
        if ((bytes_read < 0 && errno != EAGAIN) || bytes_read == 0) {
            fprintf(stderr, "(%d) Failed to read message payload from pipe: descriptor=%d error=%s\n",
                    state->id, state->reading_pipes[from], strerror(errno));
            return 3;
        }
    }

    if (bytes_read != msg->s_header.s_payload_len) {
        fprintf(stderr, "(%d) Failed to read message payload from pipe: from=%d bytes_read=%ld payload_len=%d\n",
                state->id, from, bytes_read, msg->s_header.s_payload_len);
        return 4;
    }

    vector_size = sizeof(timestamp_t) * (state->processes_count + 1);
    memcpy(msg->s_header.s_local_timevector, buffer, vector_size);
    memcpy(msg->s_payload, buffer + vector_size, bytes_read - vector_size);

    return 0;
}

int receive_any(void *self, Message *msg) {
    ProcessState *state;
    char buffer[MAX_MESSAGE_LEN];
    ssize_t bytes_read;

    state = (ProcessState *) self;
    while (1) {
        for (local_id id = 0; id <= state->processes_count; ++id) {
            if (id != state->id) {
                bytes_read = read(state->reading_pipes[id], buffer, MESSAGE_HEADER_SIZE);
                if (bytes_read < 0 && errno != EAGAIN) {
                    fprintf(stderr, "(%d) Failed to read message header from pipe (any): descriptor=%d error=%s\n",
                            state->id, state->reading_pipes[id], strerror(errno));
                    return 1;
                }
                if (bytes_read > 0) {
                    if (bytes_read != MESSAGE_HEADER_SIZE) {
                        fprintf(stderr, "(%d) Failed to read message header from pipe (any): descriptor=%d bytes_read=%ld\n",
                                state->id, state->reading_pipes[id], bytes_read);
                        return 2;
                    }
                    deserialize_header(buffer, &msg->s_header);

                    if (msg->s_header.s_payload_len > 0) {
                        size_t  vector_size;

                        while ((bytes_read = read(state->reading_pipes[id], buffer, msg->s_header.s_payload_len)) <= 0) {
                            if ((bytes_read < 0 && errno != EAGAIN) || bytes_read == 0) {
                                fprintf(stderr, "(%d) Failed to read message payload from pipe (any): descriptor=%d error=%s\n",
                                        state->id, state->reading_pipes[id], strerror(errno));
                                return 3;
                            }
                        }

                        if (bytes_read != msg->s_header.s_payload_len) {
                            fprintf(stderr, "(%d) Failed to read message payload from pipe (any): descriptor=%d bytes_read=%ld\n",
                                    state->id, state->reading_pipes[id], bytes_read);
                            return 4;
                        }

                        vector_size = sizeof(timestamp_t) * (state->processes_count + 1);
                        memcpy(msg->s_header.s_local_timevector, buffer, vector_size);
                        memcpy(msg->s_payload, buffer + vector_size, bytes_read - vector_size);
                    }

                    return 0;
                }
            }
        }
    }
}

void construct_message(const ProcessState *state, Message *message, int message_type, size_t payload_len, const char *payload) {
    size_t vector_size;

    vector_size = sizeof(timestamp_t) * (state->processes_count + 1);

    message->s_header.s_magic = MESSAGE_MAGIC;
    message->s_header.s_payload_len = (uint16_t) (payload_len + vector_size);
    message->s_header.s_type = (int16_t) message_type;
    message->s_header.s_local_time = 0;

    memcpy(message->s_header.s_local_timevector, state->vector_time, vector_size);
    memcpy(message->s_payload, state->vector_time, vector_size);

    if (payload_len) {
        memcpy(message->s_payload + vector_size, payload, payload_len);
    }
}
