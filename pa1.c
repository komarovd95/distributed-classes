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
void cleanup_child_pipe(Pipe*, FILE*);

void prepare_parrent_pipe(Pipe*, FILE*, int**);
void cleanup_parrent_pipe(Pipe*, FILE*);

void log_event(FILE*, const char*, ...);

int serialize_msg(char*, const Message*);
void deserialize_header(char*, MessageHeader*);

void execute_child(Pipe*);
void execute_parrent(Pipe*);

void send_started(Pipe*, FILE*);
void receive_started(Pipe*, FILE*);

void send_done(Pipe*, FILE*);
void receive_done(Pipe*, FILE*);

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

void execute_child(Pipe *pipe) {
    FILE *log;

    log = fopen(events_log, "a");
    if (!log) {
        printf("Failed to open events log file!\n");
        exit(1);
    }

    send_started(pipe, log);
    receive_started(pipe, log);

    // Do some work

    send_done(pipe, log);
    receive_done(pipe, log);

    fclose(log);
}

void execute_parrent(Pipe *pipe) {
    FILE *log;

    log = fopen(events_log, "a");
    if (!log) {
        printf("Failed to open events log file!\n");
        exit(1);
    }

    receive_started(pipe, log);
    receive_done(pipe, log);

    fclose(log);
}

void send_started(Pipe *pipe, FILE *log) {
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
    log_event(log, buffer);

    free(msg);
}

void receive_started(Pipe *pipe, FILE *log) {
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
    log_event(log, log_received_all_started_fmt, pipe->local_id);
}

void send_done(Pipe *pipe, FILE *log) {
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
    log_event(log, buffer);

    free(msg);
}

void receive_done(Pipe *pipe, FILE *log) {
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
    log_event(log, log_received_all_done_fmt, pipe->local_id);
}
