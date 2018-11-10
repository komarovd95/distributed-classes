#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "banking.h"
#include "pa2345.h"

//typedef struct {
//    int total_p;
//    local_id local_id;
//    int *read_pipes;
//    int *write_pipes;
//} Pipe;
//
//typedef struct {
//    Pipe pipe;
//    local_id sender;
//} SenderPipe;
//
//void create_pipes(int, FILE*, int**);
//
//void prepare_child_pipe(Pipe*, FILE*, int**);
//void cleanup_child_pipe(Pipe*, FILE*);
//
//void prepare_parrent_pipe(Pipe*, FILE*, int**);
//void cleanup_parrent_pipe(Pipe*, FILE*);
//
//void log_event(FILE*, const char*, ...);
//
//int serialize_msg(char*, const Message*);
//void deserialize_header(char*, MessageHeader*);
//
//void execute_child(Pipe*, balance_t);
//void execute_parrent(Pipe*);
//
//void send_started(Pipe*, FILE*, balance_t);
//void receive_started(Pipe*, FILE*);
//
//void send_done(Pipe*, FILE*, balance_t);
//void receive_done(Pipe*, FILE*);
//
//void send_ack(Pipe*);
//
//void send_stop(Pipe*);
//
//void serialize_transfer_order(char*, TransferOrder*);
//void deserialize_transfer_order(const char*, TransferOrder*);
//
//timestamp_t lamport_clock = 0;
//
//void send_msg(void);
//void receive_msg(timestamp_t);
//
//int main(int argc, char *argv[]) {
//    int p;
//    int i;
//    FILE *p_log;
//    int **pipes;
//    balance_t *balances;
//
//    if (argc < 3 || strcmp(argv[1], "-p") != 0) {
//        printf("Usage: '%s -p X' where X is number of subprocesses. Then provide X balances.\n",
//            argv[0]);
//        return 1;
//    }
//    p = atoi(argv[2]);
//
//    if ((p + 3) != argc) {
//        printf("Usage: '%s -p X' where X is number of subprocesses. Then provide X balances.\n",
//            argv[0]);
//        return 1;
//    }
//
//    balances = malloc(sizeof(balance_t) * p);
//    for (i = 0; i < p; i++) {
//        balances[i] = atoi(argv[i + 3]);
//    }
//
//    p_log = fopen(pipes_log, "a");
//    if (!p_log) {
//        printf("Failed to open pipes log file!\n");
//        return 1;
//    }
//
//    pipes = malloc(sizeof(int*) * (p + 1));
//    for (i = 0; i <= p; i++) {
//        pipes[i] = malloc(sizeof(int) * 2 * (p + 1));
//    }
//    create_pipes(p, p_log, pipes);
//
//    for (local_id lid = 1; lid <= p; lid++) {
//        pid_t pid;
//
//        pid = fork();
//        if (pid == -1) {
//            printf("Failed to start child process: error=%s\n", strerror(errno));
//            return 1;
//        } else if (!pid) {
//            Pipe *pipe;
//
//            pipe = malloc(sizeof(Pipe));
//            pipe->total_p = p;
//            pipe->local_id = lid;
//            pipe->read_pipes = malloc(sizeof(int) * (p + 1));
//            pipe->write_pipes = malloc(sizeof(int) * (p + 1));
//
//            prepare_child_pipe(pipe, p_log, pipes);
//            execute_child(pipe, balances[lid - 1]);
//            cleanup_child_pipe(pipe, p_log);
//
//            free(pipe->read_pipes);
//            free(pipe->write_pipes);
//            free(pipe);
//
//            return 0;
//        }
//    }
//
//    {
//        Pipe *parrent_pipe;
//
//        parrent_pipe = malloc(sizeof(Pipe));
//        parrent_pipe->total_p = p;
//        parrent_pipe->local_id = PARENT_ID;
//        parrent_pipe->read_pipes = malloc(sizeof(int) * (p + 1));
//        parrent_pipe->write_pipes = malloc(sizeof(int) * (p + 1));
//
//        prepare_parrent_pipe(parrent_pipe, p_log, pipes);
//        execute_parrent(parrent_pipe);
//        cleanup_parrent_pipe(parrent_pipe, p_log);
//
//        free(parrent_pipe->read_pipes);
//        free(parrent_pipe->write_pipes);
//        free(parrent_pipe);
//    }
//
//    free(pipes);
//    fclose(p_log);
//
//    for (i = 0; i < p; i++) {
//        int pid;
//        int status;
//
//        pid = wait(&status);
//        if (WIFEXITED(status)) {
//            int exit_status;
//
//            exit_status = WEXITSTATUS(status);
//            if (exit_status) {
//                printf("Child process with pid=%d exits with status=%d\n", pid, exit_status);
//            }
//        } else if (WIFSIGNALED(status)) {
//            printf("Child process with pid=%d was stopped by signal=%d\n", pid, WTERMSIG(status));
//        } else if (WIFSTOPPED(status)) {
//            printf("Child process with pid=%d was stopped with signal=%d\n", pid, WSTOPSIG(status));
//        }
//    }
//
//    return 0;
//}
//
//void execute_child(Pipe *pipe, balance_t balance) {
//    FILE *log;
//    int stopped;
//    int done_received;
//
//    log = fopen(events_log, "a");
//    if (!log) {
//        printf("Failed to open events log file!\n");
//        exit(1);
//    }
//
//    send_started(pipe, log, balance);
//    receive_started(pipe, log);
//
//    stopped = 0;
//    done_received = 0;
//    while (done_received < pipe->total_p) {
//        Message *msg;
//        TransferOrder *to;
//        SenderPipe *s_pipe;
//
//        s_pipe = malloc(sizeof(SenderPipe));
//        s_pipe->pipe = *pipe;
//
//        msg = malloc(sizeof(Message));
//
//        receive_any(s_pipe, msg);
//        receive_msg(msg->s_header.s_local_time);
//        if (msg->s_header.s_type == TRANSFER) {
//            to = malloc(sizeof(TransferOrder));
//            deserialize_transfer_order(msg->s_payload, to);
//            if (to->s_src == pipe->local_id) {
//                // Transfer init
//                if (stopped) {
//                    continue;
//                }
//
//                if (balance < to->s_amount) {
//                    printf("Not enough funds: local_id=%d balance=%d transfer_amount=%d\n", pipe->local_id, balance, to->s_amount);
//                    exit(1);
//                }
//                balance -= to->s_amount;
//                transfer(pipe, to->s_src, to->s_dst, to->s_amount);
//                log_event(log, log_transfer_out_fmt, get_lamport_time(), to->s_src, to->s_amount, to->s_dst);
//            } else if (to->s_dst == pipe->local_id) {
//                // Transfer in
//                balance += to->s_amount;
//                send_ack(pipe);
//                log_event(log, log_transfer_in_fmt, get_lamport_time(), to->s_dst, to->s_amount, to->s_src);
//            } else {
//                printf("Unknown transfer message: local_id=%d src=%d dst=%d\n", pipe->local_id, to->s_src, to->s_dst);
//                exit(1);
//            }
//            free(to);
//        } else if (msg->s_header.s_type == STOP) {
//            if (!stopped) {
//                send_done(pipe, log, balance);
//                stopped = 1;
//            }
//        } else if (msg->s_header.s_type == DONE) {
//            done_received++;
//        }
//
//        free(s_pipe);
//        free(msg);
//    }
//
//    // Do some work
//
//    // send_done(pipe, log, balance);
//    // receive_done(pipe, log);
//
//    fclose(log);
//}
//
//void execute_parrent(Pipe *pipe) {
//    FILE *log;
//    int ack_received;
//    int done_received;
//
//    log = fopen(events_log, "a");
//    if (!log) {
//        printf("Failed to open events log file!\n");
//        exit(1);
//    }
//
//    receive_started(pipe, log);
//    bank_robbery(pipe, pipe->total_p);
//
//    send_stop(pipe);
//
//    ack_received = 0;
//    done_received = 0;
//    while (done_received < pipe->total_p) {
//        Message *msg;
//        SenderPipe *s_pipe;
//
//        s_pipe = malloc(sizeof(SenderPipe));
//        s_pipe->pipe = *pipe;
//
//        msg = malloc(sizeof(Message));
//
//        receive_any(s_pipe, msg);
//        receive_msg(msg->s_header.s_local_time);
//
//        if (msg->s_header.s_type == ACK) {
//            ack_received++;
//        } else if (msg->s_header.s_type == DONE) {
//            done_received++;
//        } else {
//            printf("Unknown message type: %d\n", msg->s_header.s_type);
//            exit(1);
//        }
//
//        free(s_pipe);
//        free(msg);
//    }
//
//        //print_history(all);
//    // receive_done(pipe, log);
//
//    fclose(log);
//}
//
//void send_started(Pipe *pipe, FILE *log, balance_t balance) {
//    Message *msg;
//    MessageHeader msg_hdr;
//    int payload_len;
//    char buffer[MAX_PAYLOAD_LEN];
//
//    send_msg();
//    payload_len = sprintf(buffer, log_started_fmt,
//        get_lamport_time(), pipe->local_id, getpid(), getppid(), balance);
//
//    msg_hdr.s_magic = MESSAGE_MAGIC;
//    msg_hdr.s_payload_len = payload_len;
//    msg_hdr.s_type = STARTED;
//    msg_hdr.s_local_time = get_lamport_time();
//
//    msg = malloc(sizeof(Message));
//    msg->s_header = msg_hdr;
//    strcpy(msg->s_payload, buffer);
//
//    send_multicast(pipe, msg);
//    log_event(log, buffer);
//
//    free(msg);
//}
//
//void receive_started(Pipe *pipe, FILE *log) {
//    int i;
//
//    for (i = 1; i <= pipe->total_p; i++) {
//        Message *msg;
//
//        if (i != pipe->local_id) {
//            msg = malloc(sizeof(Message));
//            receive(pipe, i, msg);
//            if (msg->s_header.s_type != STARTED) {
//                printf("(%d) Not a STARTED event was received!\n", pipe->local_id);
//                exit(1);
//            }
//            receive_msg(msg->s_header.s_local_time);
//
//            free(msg);
//        }
//    }
//    log_event(log, log_received_all_started_fmt, get_lamport_time(), pipe->local_id);
//}
//
//void send_done(Pipe *pipe, FILE *log, balance_t balance) {
//    Message *msg;
//    MessageHeader msg_hdr;
//    int payload_len;
//    char buffer[MAX_PAYLOAD_LEN];
//
//    send_msg();
//    payload_len = sprintf(buffer, log_done_fmt, get_lamport_time(), pipe->local_id, balance);
//
//    msg_hdr.s_magic = MESSAGE_MAGIC;
//    msg_hdr.s_payload_len = payload_len;
//    msg_hdr.s_type = DONE;
//    msg_hdr.s_local_time = get_lamport_time();
//
//    msg = malloc(sizeof(Message));
//    msg->s_header = msg_hdr;
//    strcpy(msg->s_payload, buffer);
//
//    send_multicast(pipe, msg);
//    log_event(log, buffer);
//
//    free(msg);
//}
//
//void receive_done(Pipe *pipe, FILE *log) {
//    int i;
//
//    for (i = 1; i <= pipe->total_p; i++) {
//        Message *msg;
//
//        if (i != pipe->local_id) {
//            msg = malloc(sizeof(Message));
//            receive(pipe, i, msg);
//            if (msg->s_header.s_type != DONE) {
//                printf("(%d) Not a DONE event was received!\n", pipe->local_id);
//                exit(1);
//            }
//            receive_msg(msg->s_header.s_local_time);
//
//            free(msg);
//        }
//    }
//    log_event(log, log_received_all_done_fmt, get_lamport_time(), pipe->local_id);
//}
//
//void send_ack(Pipe *pipe) {
//    Message *msg;
//    MessageHeader msg_hdr;
//
//    send_msg();
//    msg_hdr.s_magic = MESSAGE_MAGIC;
//    msg_hdr.s_payload_len = 0;
//    msg_hdr.s_type = ACK;
//    msg_hdr.s_local_time = get_lamport_time();
//
//    msg = malloc(sizeof(Message));
//    msg->s_header = msg_hdr;
//
//    send(pipe, PARENT_ID, msg);
//
//    free(msg);
//}
//
//void send_stop(Pipe *pipe) {
//    Message *msg;
//    MessageHeader msg_hdr;
//
//    send_msg();
//    msg_hdr.s_magic = MESSAGE_MAGIC;
//    msg_hdr.s_payload_len = 0;
//    msg_hdr.s_type = STOP;
//    msg_hdr.s_local_time = get_lamport_time();
//
//    msg = malloc(sizeof(Message));
//    msg->s_header = msg_hdr;
//
//    send_multicast(pipe, msg);
//
//    free(msg);
//}
//
//void transfer(void *parent_data, local_id src, local_id dst, balance_t amount) {
//    TransferOrder *to;
//    Pipe *pipe;
//    Message *msg;
//    char buffer[sizeof(TransferOrder)];
//
//    pipe = (Pipe *) parent_data;
//
//    to = malloc(sizeof(TransferOrder));
//    to->s_src = src;
//    to->s_dst = dst;
//    to->s_amount = amount;
//
//    serialize_transfer_order(buffer, to);
//
//    send_msg();
//    msg = malloc(sizeof(Message));
//    msg->s_header.s_magic = MESSAGE_MAGIC;
//    msg->s_header.s_type = TRANSFER;
//    msg->s_header.s_local_time = get_lamport_time();
//    msg->s_header.s_payload_len = sizeof(TransferOrder);
//    memcpy(msg->s_payload, buffer, sizeof(TransferOrder));
//
//    if (pipe->local_id == PARENT_ID) {
//        // Transfer init
//        send(pipe, src, msg);
//    } else {
//        // Transfer continuation
//        send(pipe, dst, msg);
//    }
//
//    free(to);
//    free(msg);
//}
//
//void serialize_transfer_order(char *buffer, TransferOrder *to) {
//    buffer[0] = to->s_src;
//    buffer[1] = to->s_dst;
//    buffer[2] = to->s_amount >> 8;
//    buffer[3] = to->s_amount;
//}
//
//void deserialize_transfer_order(const char *buffer, TransferOrder *to) {
//    to->s_src = buffer[0];
//    to->s_dst = buffer[1];
//    to->s_amount = buffer[2] << 8;
//    to->s_amount |= buffer[3];
//}
//
//timestamp_t get_lamport_time() {
//    return lamport_clock;
//}
//
//void send_msg() {
//    lamport_clock++;
//}
//
//void receive_msg(timestamp_t msg_time) {
//    if (msg_time > lamport_clock) {
//        lamport_clock = msg_time;
//    }
//    lamport_clock++;
//}
