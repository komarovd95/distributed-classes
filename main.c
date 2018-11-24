#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdarg.h>
#include <fcntl.h>
#include "ipc.h"
#include "pipes.h"
#include "common.h"
#include "phases.h"
#include "core.h"
#include "banking.h"

/**
 * Joins created processes.
 *
 * @param count a processes count to join
 */
void join_processes(local_id count);

/**
 * Executes phases for child process.
 *
 * @param state a state of current child process
 * @return 0 if success
 */
int execute_child(ProcessState *state);

/**
 * Executes phases for parent process.
 *
 * @param state a state of parent process
 * @return 0 if success
 */
int execute_parent(ProcessState *state);

timestamp_t local_time;

int main(int argc, const char *argv[]) {
    long processes_count;
    int pipes_descriptors[TOTAL_PROCESSES][TOTAL_PROCESSES * 2];
    int evt_log, pd_log;
    int use_mutex;
    ProcessState parent_state;

    use_mutex = 0;

    if (argc < 3) {
        fprintf(stderr, "Usage %s -p X [--mutexl], where X is number of child processes.\n", argv[0]);
        return 1;
    }
    if (strcmp(argv[1], "-p") == 0) {
        processes_count = strtol(argv[2], NULL, 10);
        if (argc == 4) {
            if (strcmp(argv[3], "--mutexl") == 0) {
                use_mutex = 1;
            } else {
                fprintf(stderr, "Usage %s -p X [--mutexl], where X is number of child processes.\n", argv[0]);
                return 1;
            }
        }
    } else if (strcmp(argv[1], "--mutexl") == 0) {
        use_mutex = 1;
        if (argc != 4) {
            fprintf(stderr, "Usage %s -p X [--mutexl], where X is number of child processes.\n", argv[0]);
            return 1;
        }
        processes_count = strtol(argv[3], NULL, 10);
    } else {
        fprintf(stderr, "Usage %s -p X [--mutexl], where X is number of child processes.\n", argv[0]);
        return 1;
    }

    if (processes_count > MAX_PROCESS_ID) {
        fprintf(stderr, "Too much processes to create: actual=%ld limit=%d\n", processes_count, MAX_PROCESS_ID);
        return 2;
    }

    pd_log = open(pipes_log, O_WRONLY | O_APPEND | O_CREAT, 0777);
    if (pd_log < 0) {
        fprintf(stderr, "Failed to open pipes log!\n");
        return 3;
    }

    evt_log = open(events_log, O_WRONLY | O_APPEND | O_CREAT, 0777);
    if (evt_log < 0) {
        fprintf(stderr, "Failed to open events log!\n");
        close(pd_log);
        return 4;
    }

    parent_state.id = PARENT_ID;
    parent_state.processes_count = processes_count;
    parent_state.evt_log = evt_log;
    parent_state.pd_log = pd_log;

    if (init_pipes(&parent_state, pipes_descriptors)) {
        fprintf(stderr, "Failed to initialize pipes descriptors!\n");
        close(pd_log);
        return 5;
    }

    local_time = 0;

    for (local_id id = 1; id <= processes_count; ++id) {
        pid_t pid;

        pid = fork();
        if (pid < 0) {
            fprintf(stderr, "Failed to fork process: id=%d\n", id);
            close_pipes(&parent_state, pipes_descriptors);
            close(pd_log);
            close(evt_log);
            join_processes(id);
            return -1;
        } else if (!pid) {
            int result;
            ProcessState process_state;

            process_state.id = id;
            process_state.processes_count = processes_count;
            process_state.evt_log = evt_log;
            process_state.pd_log = pd_log;

            for (int i = 0; i <= processes_count; ++i) {
                process_state.done_received[i] = 0;
            }

            process_state.use_mutex = use_mutex;

            for (int i = 1; i <= processes_count; ++i) {
                if (i > id) {
                    process_state.forks[i].is_controlled = 1;
                    process_state.forks[i].is_dirty = 1;
                    process_state.forks[i].has_request = 0;
                } else if (i < id) {
                    process_state.forks[i].is_controlled = 0;
                    process_state.forks[i].is_dirty = 0;
                    process_state.forks[i].has_request = 1;
                }
            }

            if (prepare_pipes(&process_state, pipes_descriptors)) {
                fprintf(stderr, "(%d) Failed to prepare pipes.\n", id);
                close_pipes(&process_state, pipes_descriptors);
                return 1;
            }

            if ((result = execute_child(&process_state))) {
                fprintf(stderr, "(%d) Failed to execute child!\n", id);
            }
            close(pd_log);
            close(evt_log);
            return result;
        }
    }

    {
        int result;

        if (prepare_pipes(&parent_state, pipes_descriptors)) {
            fprintf(stderr, "Failed to prepare parent pipes\n");
            close_pipes(&parent_state, pipes_descriptors);
            return 1;
        }

        if ((result = execute_parent(&parent_state))) {
            fprintf(stderr, "Failed to execute parent!\n");
        }
        close(pd_log);
        close(evt_log);
        join_processes((local_id) processes_count);
        return result;
    }
}

void join_processes(local_id count) {
    for (int i = 1; i <= count; ++i) {
        pid_t pid;
        int status;

        pid = wait(&status);
        if (WIFEXITED(status)) {
            int exit_status;

            exit_status = WEXITSTATUS(status);
            if (exit_status) {
                fprintf(stderr, "Child process exits abnormally: pid=%d exit_status=%d\n", pid, exit_status);
            }
        } else if (WIFSIGNALED(status)) {
            fprintf(stderr, "Child process was terminated by signal: pid=%d signal=%d\n", pid, WTERMSIG(status));
        } else if (WIFSTOPPED(status)) {
            fprintf(stderr, "Child process was stopped by signal: pid=%d signal=%d\n", pid, WSTOPSIG(status));
        }
    }
}

void log_pipe(ProcessState *state, const char *fmt, ...) {
    va_list args;
    char buffer[1024];

    va_start(args, fmt);
    vsprintf(buffer, fmt, args);
    write(state->pd_log, buffer, strlen(buffer));
    va_end(args);
}

void log_event(ProcessState *state, const char *message) {
    write(state->evt_log, message, strlen(message));
    printf("%s", message);
}

int execute_child(ProcessState *state) {
    if (child_phase_1(state)) {
        fprintf(stderr, "(%d) Failed to execute first phase!\n", state->id);
        cleanup_pipes(state);
        return 1;
    }
    if (child_phase_2(state)) {
        fprintf(stderr, "(%d) Failed to execute second phase!\n", state->id);
        cleanup_pipes(state);
        return 2;
    }
    if (child_phase_3(state)) {
        fprintf(stderr, "(%d) Failed to execute third phase!\n", state->id);
        cleanup_pipes(state);
        return 3;
    }

    cleanup_pipes(state);

    return 0;
}

int execute_parent(ProcessState *state) {
    if (parent_phase_1(state)) {
        fprintf(stderr, "Failed to execute first parent phase\n");
        cleanup_pipes(state);
        return 1;
    }
    if (parent_phase_2(state)) {
        fprintf(stderr, "Failed to execute second parent phase\n");
        cleanup_pipes(state);
        return 2;
    }
    if (parent_phase_3(state)) {
        fprintf(stderr, "Failed to execute third parent phase\n");
        cleanup_pipes(state);
        return 3;
    }

    cleanup_pipes(state);

    return 0;
}

timestamp_t get_lamport_time() {
    return local_time;
}

void on_message_send(void) {
    local_time++;
}

void on_message_received(timestamp_t message_time) {
    if (message_time > local_time) {
        local_time = message_time;
    }
    local_time++;
}
