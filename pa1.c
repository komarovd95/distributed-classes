#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "pa1.h"
#include "common.h"
#include "ipc.h"

typedef struct {
    int total_p;
    local_id local_id;
    int *read_pipes;
    int *write_pipes;
} Pipe;

void create_pipes(int, FILE*, int**);

void prepare_child_pipe(Pipe*, FILE*, int**);
void execute_child(Pipe*);
void cleanup_child_pipe(Pipe*, FILE*);

void prepare_parrent_pipe(Pipe*, FILE*, int**);
void execute_parrent(Pipe*);
void cleanup_parrent_pipe(Pipe*, FILE*);

void log_event(FILE*, const char*, ...);

void send_started(Pipe*);
void receive_started(Pipe*);

void send_done(Pipe*);
void receive_done(Pipe*);

int serialize_msg(char*, const Message*);
void deserialize_header(char*, MessageHeader*);

int main(int argc, char *argv[]) {
    int p;
    int i;
    FILE *p_log;
    int **pipes;

    if (argc != 3 || strcmp(argv[1], "-p") != 0) {
        printf("Usage: '%s -p X' where X is number of subprocesses.\n", argv[0]);
        return 1;
    }

    p = atoi(argv[2]);

    p_log = fopen(pipes_log, "a");
    if (!p_log) {
        printf("Failed to open pipes log file!\n");
        return 1;
    }

    pipes = malloc(sizeof(int*) * (p + 1));
    for (i = 1; i <= p; i++) {
        pipes[i] = malloc(sizeof(int) * 2 * (p + 1));
    }
    create_pipes(p, p_log, pipes);

    for (local_id lid = 1; lid <= p; lid++) {
        pid_t pid;

        pid = fork();
        if (pid == -1) {
            printf("Failed to start child process: error=%s\n", strerror(errno));
            return 1;
        } else if (!pid) {
            Pipe *pipe;

            pipe = malloc(sizeof(Pipe));
            pipe->total_p = p;
            pipe->local_id = lid;
            pipe->read_pipes = malloc(sizeof(int) * (p + 1));
            pipe->write_pipes = malloc(sizeof(int) * (p + 1));

            prepare_child_pipe(pipe, p_log, pipes);
            execute_child(pipe);
            cleanup_child_pipe(pipe, p_log);

            free(pipe->read_pipes);
            free(pipe->write_pipes);
            free(pipe);

            return 0;
        }
    }

    {
        Pipe *parrent_pipe;

        parrent_pipe = malloc(sizeof(Pipe));
        parrent_pipe->total_p = p;
        parrent_pipe->local_id = PARENT_ID;
        parrent_pipe->read_pipes = malloc(sizeof(int) * (p + 1));

        prepare_parrent_pipe(parrent_pipe, p_log, pipes);
        execute_parrent(parrent_pipe);
        cleanup_parrent_pipe(parrent_pipe, p_log);

        free(parrent_pipe->read_pipes);
        free(parrent_pipe);
    }

    free(pipes);
    fclose(p_log);

    for (i = 0; i < p; i++) {
        int pid;
        int status;

        pid = wait(&status);
        if (WIFEXITED(status)) {
            int exit_status;

            exit_status = WEXITSTATUS(status);
            if (exit_status) {
                printf("Child process with pid=%d exits with status=%d\n", pid, exit_status);
            }
        } else if (WIFSIGNALED(status)) {
            printf("Child process with pid=%d was stopped by signal=%d\n", pid, WTERMSIG(status));
        } else if (WIFSTOPPED(status)) {
            printf("Child process with pid=%d was stopped with signal=%d\n", pid, WSTOPSIG(status));
        }
    }

    return 0;
}

void create_pipes(int p, FILE *log, int **pipes) {
    int i, j;

    for (i = 1; i <= p; i++) {
        for (j = 0; j <= p; j++) {
            if (i != j) {
                if (pipe(&pipes[i][j * 2]) == -1) {
                    printf("Failed to create pipe: error=%s\n", strerror(errno));
                    exit(1);
                }
                fprintf(log, "Create pipe: i=%d, j=%d, d=[%d, %d]\n", 
                    i, j, pipes[i][j * 2], pipes[i][j * 2 + 1]);
            }
        }
    }
}

void prepare_child_pipe(Pipe *pipe, FILE *log, int **pipes) {
    int i, j;

    for (i = 1; i <= pipe->total_p; i++) {
        if (i != pipe->local_id) {
            pipe->read_pipes[i] = pipes[i][pipe->local_id * 2];
            close(pipes[i][pipe->local_id * 2 + 1]);
            fprintf(log, "Closed pipe write endpoint (%d): from=%d to=%d\n", pipe->local_id, i, pipe->local_id);
        }
    }
    for (i = 0; i <= pipe->total_p; i++) {
        if (i != pipe->local_id) {
            pipe->write_pipes[i] = pipes[pipe->local_id][i * 2 + 1];
            close(pipes[pipe->local_id][i * 2]);
            fprintf(log, "Closed pipe read endpoint (%d): from=%d to=%d\n", pipe->local_id, pipe->local_id, i);
        }
    }
    for (i = 1; i <= pipe->total_p; i++) {
        for (j = 0; j <= pipe->total_p; j++) {
            if (i != j && i != pipe->local_id && j != pipe->local_id) {
                close(pipes[i][j * 2]);
                close(pipes[i][j * 2 + 1]);
                fprintf(log, "Closed unused pipe (%d): from=%d to=%d\n", pipe->local_id, i, j);
            }
        }
    }
}

void cleanup_child_pipe(Pipe *pipe, FILE *log) {
    int i;

    close(pipe->write_pipes[PARENT_ID]);
    fprintf(log, "Clean up pipe (%d): from=%d to=%d\n", pipe->local_id, pipe->local_id, PARENT_ID);

    for (i = 1; i <= pipe->total_p; i++) {
        if (i != pipe->local_id) {
            close(pipe->read_pipes[i]);
            close(pipe->write_pipes[i]);
            fprintf(log, "Clean up pipe (%d): from=%d to=%d\n", pipe->local_id, pipe->local_id, i);
        }
    }
}

void prepare_parrent_pipe(Pipe *pipe, FILE *log, int **pipes) {
    int i, j;

    for (i = 1; i <= pipe->total_p; i++) {
        pipe->read_pipes[i] = pipes[i][0];

        for (j = 1; j <= pipe->total_p; j++) {
            if (i != j) {
                close(pipes[i][j * 2]);
                close(pipes[i][j * 2 + 1]);
                fprintf(log, "Closed unused pipe (%d): from=%d to=%d\n", PARENT_ID, i, j);
            }
        }
    }
}

void cleanup_parrent_pipe(Pipe *pipe, FILE *log) {
    int i;

    for (i = 1; i <= pipe->total_p; i++) {
        close(pipe->read_pipes[i]);
        fprintf(log, "Clean up pipe (%d): from=%d to=%d\n", PARENT_ID, i, PARENT_ID);
    }
}

void execute_child(Pipe *pipe) {
    FILE *log;

    log = fopen(events_log, "a");
    if (!log) {
        printf("Failed to open events log file!\n");
        exit(1);
    }

    send_started(pipe);
    log_event(log, log_started_fmt, pipe->local_id, getpid(), getppid());

    receive_started(pipe);
    log_event(log, log_received_all_started_fmt, pipe->local_id);

    // Do some work

    send_done(pipe);
    log_event(log, log_done_fmt, pipe->local_id);

    receive_done(pipe);
    log_event(log, log_received_all_done_fmt, pipe->local_id);

    fclose(log);
}

void execute_parrent(Pipe *pipe) {
    FILE *log;

    log = fopen(events_log, "a");
    if (!log) {
        printf("Failed to open events log file!\n");
        exit(1);
    }

    receive_started(pipe);
    log_event(log, log_received_all_started_fmt, PARENT_ID);

    receive_done(pipe);
    log_event(log, log_received_all_done_fmt, PARENT_ID);

    fclose(log);
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

void send_started(Pipe *pipe) {
    Message *msg;
    MessageHeader msg_hdr;
    int payload_len;
    char buffer[MAX_PAYLOAD_LEN];

    payload_len = sprintf(buffer, log_started_fmt, pipe->local_id, getpid(), getppid());

    msg_hdr.s_magic = MESSAGE_MAGIC;
    msg_hdr.s_payload_len = payload_len;
    msg_hdr.s_type = STARTED;
    msg_hdr.s_local_time = 0;

    msg = malloc(sizeof(Message));
    msg->s_header = msg_hdr;    
    strcpy(msg->s_payload, buffer);

    send_multicast(pipe, msg);

    free(msg);
}

void receive_started(Pipe *pipe) {
    int i;

    for (i = 1; i <= pipe->total_p; i++) {
        Message *msg;

        if (i != pipe->local_id) {
            msg = malloc(sizeof(Message));
            receive(pipe, i, msg);
            if (msg->s_header.s_type != STARTED) {
                printf("Not a STARTED event was received!\n");
                exit(1);
            }
            free(msg);
        }
    }
}

void send_done(Pipe *pipe) {
    Message *msg;
    MessageHeader msg_hdr;
    int payload_len;
    char buffer[MAX_PAYLOAD_LEN];

    payload_len = sprintf(buffer, log_done_fmt, pipe->local_id);

    msg_hdr.s_magic = MESSAGE_MAGIC;
    msg_hdr.s_payload_len = payload_len;
    msg_hdr.s_type = DONE;
    msg_hdr.s_local_time = 0;

    msg = malloc(sizeof(Message));
    msg->s_header = msg_hdr;    
    strcpy(msg->s_payload, buffer);

    send_multicast(pipe, msg);

    free(msg);
}

void receive_done(Pipe *pipe) {
    int i;

    for (i = 1; i <= pipe->total_p; i++) {
        Message *msg;

        if (i != pipe->local_id) {
            msg = malloc(sizeof(Message));
            receive(pipe, i, msg);
            if (msg->s_header.s_type != DONE) {
                printf("Not a DONE event was received!\n");
                exit(1);
            }
            free(msg);
        }
    }
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
