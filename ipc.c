#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "ipc.h"
#include "common.h"
#include "dio.h"

/**
 * Serializes message into given buffer. Serialization format:
 *
 * message_magic  (big endian) 2 bytes
 * payload_length (big endian) 2 bytes
 * message_type   (big endian) 2 bytes
 * local_time     (big endian) 2 bytes
 *
 * then payload_len bytes of payload
 *
 * @param buffer  a buffer
 * @param message a message must be serialized
 */
void serialize_msg(char *buffer, const Message *message);
void deserialize_header(char*, MessageHeader*);

int create_pipes(int processes_count, int **pipes_descriptors, FILE *log) {
    int i, j;

#ifdef __PARENT_CAN_WRITE
    i = 0;
#else
    i = 1;
#endif

    for (; i <= processes_count; i++) {
        for (j = 0; j <= processes_count; j++) {
            if (i != j) {
                if (pipe(&pipes_descriptors[i][j * 2]) == -1) {
                    fprintf(stderr, "Failed to create pipe: from=%d, to=%d, error=%s\n",
                        i, j, strerror(errno));
                    return errno;
                }
                fprintf(log, "Create pipe: from=%d, to=%d, descriptors=[%d, %d]\n",
                    i, j, pipes_descriptors[i][j * 2], pipes_descriptors[i][j * 2 + 1]);
            }
        }
    }

    return 0;
}

int prepare_rw_pipes(ProcessPipe *pipe, int **pipes_descriptors, FILE *log) {
    int i, j;
    int p;
    local_id lid;

    p = pipe->processes_count;
    lid = pipe->local_id;

    // Set real read endpoint and close unused write endpoint of pipe.
#ifdef __PARENT_CAN_WRITE
    i = 0;
#else
    i = 1;
#endif

    for (; i <= p; i++) {
        if (i != lid) {
            pipe->read_pipes[i] = pipes_descriptors[i][lid * 2];
            if (close(pipes_descriptors[i][lid * 2 + 1]) < 0) {
                fprintf(stderr, "Failed to close write pipe: from=%d, to=%d, error=%s\n",
                    i, lid, strerror(errno));
                return errno;
            }
            fprintf(log, "Closed pipe write endpoint (%d): from=%d to=%d\n",
                lid, i, lid);
        }
    }
    // Set real write endpoint and close unused read endpont of pipe.
    for (i = 0; i <= p; i++) {
        if (i != lid) {
            pipe->write_pipes[i] = pipes_descriptors[lid][i * 2 + 1];
            if (close(pipes_descriptors[lid][i * 2]) < 0) {
                fprintf(stderr, "Failed to close read pipe: from=%d, to=%d, error=%s\n",
                    lid, i, strerror(errno));
                return errno;
            }
            fprintf(log, "Closed pipe read endpoint (%d): from=%d to=%d\n",
                lid, lid, i);
        }
    }
    // Close all other unused pipes descriptors.
    for (i = 0; i <= p; i++) {
        for (j = 0; j <= p; j++) {
            if (i != j && i != lid && j != lid) {
                if (close(pipes_descriptors[i][j * 2]) < 0) {
                    fprintf(stderr, "Failed to close unused read pipe: from=%d, to=%d, error=%s\n",
                        i, j, strerror(errno));
                    return errno;
                }
                if (close(pipes_descriptors[i][j * 2 + 1]) < 0) {
                    fprintf(stderr, "Failed to close write pipe: from=%d, to=%d, error=%s\n",
                        i, j, strerror(errno));
                    return errno;
                }
                fprintf(log, "Closed unused pipe (%d): from=%d to=%d\n", lid, i, j);
            }
        }
    }

    return 0;
}

int prepare_ro_pipes(ProcessPipe *pipe, int **pipes_descriptors, FILE *log) {
    int i, j;
    int p;
    local_id lid;

    p = pipe->processes_count;
    lid = pipe->local_id;
    for (i = 0; i <= p; i++) {
        if (i != lid) {
            pipe->read_pipes[i] = pipes_descriptors[i][lid * 2];
            if (close(pipes_descriptors[i][lid * 2 + 1]) < 0) {
                fprintf(stderr, "Failed to close write pipe: from=%d, to=%d, error=%s\n",
                    i, lid, strerror(errno));
                return errno;
            }
            fprintf(log, "Closed pipe write endpoint (%d): from=%d to=%d\n",
                lid, i, lid);
        }
        for (j = 0; j <= p; j++) {
            if (i != j && i != lid && j != lid) {
                if (close(pipes_descriptors[i][j * 2]) < 0) {
                    fprintf(stderr, "Failed to close unused read pipe: from=%d, to=%d, error=%s\n",
                        i, j, strerror(errno));
                    return errno;
                }
                if (close(pipes_descriptors[i][j * 2 + 1]) < 0) {
                    fprintf(stderr, "Failed to close write pipe: from=%d, to=%d, error=%s\n",
                        i, j, strerror(errno));
                    return errno;
                }
                fprintf(log, "Closed unused pipe (%d): from=%d to=%d\n", lid, i, j);
            }
        }
    }

    return 0;
}

int cleanup_rw_pipe(ProcessPipe *pipe, FILE *log) {
    int i;
    int p;
    local_id lid;

#ifdef __PARENT_CAN_WRITE
    i = 0;
#else
    if (close(pipe->write_pipes[PARENT_ID]) < 0) {
        fprintf(stderr, "Failed to close write pipe: from=%d, to=%d, error=%s\n",
            i, lid, strerror(errno));
        return errno;
    }
    fprintf(log, "Closed pipe write endpoint (%d): from=%d to=%d\n", lid, i, lid);
    i = 1;
#endif

    p = pipe->processes_count;
    lid = pipe->local_id;
    for (; i <= p; i++) {
        if (i != lid) {
            if (close(pipe->read_pipes[i]) < 0) {
                fprintf(stderr, "Failed to close unused read pipe: from=%d, to=%d, error=%s\n",
                    lid, i, strerror(errno));
                return errno;
            }
            if (close(pipe->write_pipes[i]) < 0) {
                fprintf(stderr, "Failed to close write pipe: from=%d, to=%d, error=%s\n",
                    lid, i, strerror(errno));
                return errno;
            }
            fprintf(log, "Clean up pipe (%d): from=%d to=%d\n",lid, lid, i);
        }
    }
    return 0;
}

int cleanup_ro_pipe(ProcessPipe *pipe, FILE *log) {
    int i;
    int p;
    local_id lid;

    p = pipe->processes_count;
    lid = pipe->local_id;
    for (i = 0; i <= p; i++) {
        if (i != lid) {
            if (close(pipe->read_pipes[i]) < 0) {
                fprintf(stderr, "Failed to close unused read pipe: from=%d, to=%d, error=%s\n",
                    lid, i, strerror(errno));
                return errno;
            }
#ifdef __PARENT_CAN_WRITE
            if (close(pipe->write_pipes[i]) < 0) {
                fprintf(stderr, "Failed to close write pipe: from=%d, to=%d, error=%s\n",
                    lid, i, strerror(errno));
                return errno;
            }
#endif
            fprintf(log, "Clean up pipe (%d): from=%d to=%d\n",lid, lid, i);
        }
    }
    return 0;
}

void log_event(FILE *log, const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    vfprintf(log, fmt, args);
    va_end(args);

    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

int serialize_msg(char *buffer, const Message *message) {
    buffer[0] = message->header.s_magic >> 8;
    buffer[1] = message->header.s_magic;

    buffer[2] = message->header.s_payload_len >> 8;
    buffer[3] = message->header.s_payload_len;

    buffer[4] = message->header.s_type >> 8;
    buffer[5] = message->header.s_type;

    buffer[6] = message->header.s_local_time >> 8;
    buffer[7] = message->header.s_local_time;

    memcpy(&buffer[sizeof(MessageHeader)], message->s_payload, message->header.s_payload_len);
}

// typedef struct {
//     int total_p;
//     local_id local_id;
//     int *read_pipes;
//     int *write_pipes;
// } Pipe;
//
// void create_pipes(int, FILE*, int**);
//
// void prepare_child_pipe(Pipe*, FILE*, int**);
// void cleanup_child_pipe(Pipe*, FILE*);
//
// void prepare_parrent_pipe(Pipe*, FILE*, int**);
// void cleanup_parrent_pipe(Pipe*, FILE*);
//
// void log_event(FILE*, const char*, ...);

void send_started(Pipe*);
void receive_started(Pipe*);

void send_done(Pipe*);
void receive_done(Pipe*);


// void prepare_parrent_pipe(Pipe *pipe, FILE *log, int **pipes) {
//     int i, j;
//
//     for (i = 1; i <= pipe->total_p; i++) {
//         pipe->read_pipes[i] = pipes[i][0];
//
//         for (j = 1; j <= pipe->total_p; j++) {
//             if (i != j) {
//                 close(pipes[i][j * 2]);
//                 close(pipes[i][j * 2 + 1]);
//                 fprintf(log, "Closed unused pipe (%d): from=%d to=%d\n", PARENT_ID, i, j);
//             }
//         }
//     }
// }

// void cleanup_parrent_pipe(Pipe *pipe, FILE *log) {
//     int i;
//
//     for (i = 1; i <= pipe->total_p; i++) {
//         close(pipe->read_pipes[i]);
//         fprintf(log, "Clean up pipe (%d): from=%d to=%d\n", PARENT_ID, i, PARENT_ID);
//     }
// }



int serialize_msg(char *buffer, const Message *message) {
    int serialized_size;
    MessageHeader header;

    serialized_size = 0;

    header = message->s_header;
    buffer[serialized_size++] = header.s_magic >> 8;
    buffer[serialized_size++] = header.s_magic;

    buffer[serialized_size++] = header.s_payload_len >> 8;
    buffer[serialized_size++] = header.s_payload_len;

    buffer[serialized_size++] = header.s_type >> 8;
    buffer[serialized_size++] = header.s_type;

    buffer[serialized_size++] = header.s_local_time >> 8;
    buffer[serialized_size++] = header.s_local_time;

    strcpy(&buffer[serialized_size], message->s_payload);
    serialized_size += header.s_payload_len;

    return serialized_size;
}

void deserialize_header(char *buffer, MessageHeader *header) {
    uint16_t magic;
    uint16_t payload_len;
    int16_t type;
    timestamp_t local_time;

    magic = buffer[0] << 8;
    magic |= buffer[1];

    payload_len = buffer[2] << 8;
    payload_len |= buffer[3];

    type = buffer[4] << 8;
    type |= buffer[5];

    local_time = buffer[6] << 8;
    local_time = buffer[7];

    header->s_magic = magic;
    header->s_payload_len = payload_len;
    header->s_type = type;
    header->s_local_time = local_time;
}

int send_multicast(void * self, const Message * msg) {
    int p;
    int i;
    int local_id;
    int serialized_size;
    Pipe *pipe;
    char buffer[MAX_MESSAGE_LEN];

    pipe = (Pipe *) self;

    p = pipe->total_p;
    local_id = pipe->local_id;

    serialized_size = serialize_msg(buffer, msg);
    for (i = 0; i <= p; i++) {
        if (local_id != i) {
            if (write(pipe->write_pipes[i], buffer, serialized_size) != serialized_size) {
                printf("Failed to write message to pipe: local_id=%d to=%d\n", local_id, i);
                return 1;
            }
        }
    }

    return 0;
}

int receive(void * self, local_id from, Message * msg) {
    Pipe *pipe;
    char buffer[MAX_MESSAGE_LEN];

    pipe = (Pipe *) self;

    while (!read(pipe->read_pipes[from], buffer, sizeof(MessageHeader)));
    deserialize_header(buffer, &msg->s_header);

    while (!read(pipe->read_pipes[from], &buffer[sizeof(MessageHeader)], msg->s_header.s_payload_len));
    strcpy(msg->s_payload, &buffer[sizeof(MessageHeader)]);

    return 0;
}
