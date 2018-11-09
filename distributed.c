#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "distributed.h"
#include "pa1.h"
#include "logging.h"

typedef struct {
    ProcessInfo process_info;
    local_id    to;
    int16_t     type;
    uint16_t    payload_len;
    char        payload[MAX_PAYLOAD_LEN];
} SendMessage;

typedef struct {
    ProcessInfo process_info;
    local_id    sender;
    int         available_to_send[MAX_PROCESS_ID];
    Message     message;
} ReceiveMessage;

int broadcast_message(SendMessage *send_message);
int receive_from_all(ReceiveMessage *receive_message, int16_t message_type);

void serialize_message(unsigned char *buffer, const Message *message);
void deserialize_header(const unsigned char *buffer, MessageHeader *header);

int broadcast_started(ProcessInfo *process_info) {
    SendMessage send_message;
    int payload_len;
    char buffer[MAX_PAYLOAD_LEN];

    send_message.process_info = *process_info;

    payload_len = sprintf(buffer, log_started_fmt, process_info->id, getpid(), getppid());
    send_message.type = STARTED;
    send_message.payload_len = (uint16_t) payload_len;
    memcpy(send_message.payload, buffer, (size_t) payload_len);

    if (broadcast_message(&send_message)) {
        fprintf(stderr, "(%d) Failed to broadcast STARTED event!\n", process_info->id);
        return 1;
    }
    log_event(buffer);
    return 0;
}

int broadcast_done(ProcessInfo *process_info) {
    SendMessage send_message;
    int payload_len;
    char buffer[MAX_PAYLOAD_LEN];

    send_message.process_info = *process_info;

    payload_len = sprintf(buffer, log_done_fmt, process_info->id);
    send_message.type = DONE;
    send_message.payload_len = (uint16_t) payload_len;
    memcpy(send_message.payload, buffer, (size_t) payload_len);

    if (broadcast_message(&send_message)) {
        fprintf(stderr, "(%d) Failed to broadcast DONE event!\n", process_info->id);
        return 1;
    }
    log_event(buffer);
    return 0;
}

int broadcast_message(SendMessage *send_message) {
    Message message;

    message.s_header.s_magic = MESSAGE_MAGIC;
    message.s_header.s_type = send_message->type;
    message.s_header.s_payload_len = send_message->payload_len;
    message.s_header.s_local_time = 0;
    memcpy(message.s_payload, send_message->payload, send_message->payload_len);

    return send_multicast(&send_message->process_info, &message);
}

int send_multicast(void *self, const Message *msg) {
    ProcessInfo *process_info;
    local_id id;

    process_info = (ProcessInfo *) self;

    for (id = 0; id <= process_info->processes_count; ++id) {
        if (id != process_info->id) {
            if (send(process_info, id, msg)) {
                fprintf(stderr, "(%d) ailed to write multicast message: to=%d\n", process_info->id, id);
                return 1;
            }
        }
    }

    return 0;
}

int send(void *self, local_id to, const Message *message) {
    size_t serialized_size;
    unsigned char buffer[MAX_PAYLOAD_LEN];
    ProcessInfo *process_info;

    process_info = (ProcessInfo *) self;

    serialize_message(buffer, message);
    serialized_size = sizeof(MessageHeader) + message->s_header.s_payload_len;

    if (serialized_size != write(process_info->writing_pipes[to], buffer, serialized_size)) {
        fprintf(stderr, "(%d) Failed to send message to=%d\n", process_info->id, to);
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

int receive_started(ProcessInfo *process_info) {
    ReceiveMessage receive_message;
    int i;
    char buffer[MAX_PAYLOAD_LEN];

    receive_message.process_info = *process_info;

    for (i = 0; i <= process_info->processes_count; ++i) {
        receive_message.available_to_send[i] = 1;
    }

    if (receive_from_all(&receive_message, STARTED)) {
        fprintf(stderr, "(%d) Failed to receive all STARTED events!\n", process_info->id);
        return 1;
    }
    sprintf(buffer, log_received_all_started_fmt, process_info->id);
    log_event(buffer);
    return 0;
}

int receive_done(ProcessInfo *process_info) {
    ReceiveMessage receive_message;
    int i;
    char buffer[MAX_PAYLOAD_LEN];

    receive_message.process_info = *process_info;

    for (i = 0; i <= process_info->processes_count; ++i) {
        receive_message.available_to_send[i] = 1;
    }

    if (receive_from_all(&receive_message, DONE)) {
        fprintf(stderr, "(%d) Failed to receive all DONE events!\n", process_info->id);
        return 1;
    }
    sprintf(buffer, log_received_all_done_fmt, process_info->id);
    log_event(buffer);
    return 0;
}

int receive_from_all(ReceiveMessage *receive_message, int16_t message_type) {
    local_id id;
    ProcessInfo process_info;

    process_info = receive_message->process_info;
    for (id = 1; id <= process_info.processes_count; ++id) {
        if (id != process_info.id && receive_message->available_to_send[id]) {
            Message *message;

            message = &receive_message->message;
            if (receive(&process_info, id, message)) {
                fprintf(stderr, "(%d) Failed to receive message from: from=%d\n", process_info.id, id);
                return 1;
            }
            if (message->s_header.s_type != message_type) {
                fprintf(stderr, "(%d) Message has incorrect type: type=%d\n",
                        process_info.id, message->s_header.s_type);
                return 2;
            }
        }
    }

    return 0;
}

int receive(void *self, local_id from, Message *msg) {
    ProcessInfo *process_info;
    unsigned char buffer[MAX_MESSAGE_LEN];

    process_info = (ProcessInfo *) self;
    while (!read(process_info->reading_pipes[from], buffer, sizeof(MessageHeader)));

    deserialize_header(buffer, &msg->s_header);

    while (!read(process_info->reading_pipes[from], msg->s_payload, msg->s_header.s_payload_len));

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
