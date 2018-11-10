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
#include "distributed.h"
#include "common.h"
#include "phases.h"

/**
 * Executes phases for child process.
 *
 * @param id local ID of current child process
 * @return 0 if success
 */
int execute_child(local_id id);

/**
 * Executes phases for parent process.
 *
 * @return 0 if success
 */
int execute_parent();


long processes_count;
int pipes_descriptors[TOTAL_PROCESSES][TOTAL_PROCESSES * 2];
int evt_log, pd_log;

int main(int argc, const char *argv[]) {
    local_id lid;
    int result;

    if (argc != 3 || strcmp(argv[1], "-p") != 0) {
        fprintf(stderr, "Usage %s -p X, where X is number of child processes.\n", argv[0]);
        return 1;
    }
    processes_count = strtol(argv[2], NULL, 10);

    if (processes_count > MAX_PROCESS_ID) {
        fprintf(stderr, "Too much processes to create: actual=%ld limit=%d\n", processes_count, MAX_PROCESS_ID);
        return 2;
    }

    pd_log = open(pipes_log, O_WRONLY | O_APPEND | O_CREAT, 0777);
    if (pd_log < 0) {
        fprintf(stderr, "Failed to open pipes log!\n");
        return 3;
    }

    if (init_pipes(processes_count, pipes_descriptors)) {
        fprintf(stderr, "Failed to initialize pipes descriptors!\n");
        close(pd_log);
        return 4;
    }

    evt_log = open(events_log, O_WRONLY | O_APPEND | O_CREAT, 0777);
    if (evt_log < 0) {
        fprintf(stderr, "Failed to open events log!\n");
        close_pipes(processes_count, pipes_descriptors);
        close(pd_log);
        return 5;
    }

    for (lid = 1; lid <= processes_count; ++lid) {
        pid_t pid;

        pid = fork();
        if (pid < 0) {
            fprintf(stderr, "Failed to fork process: local_id=%d\n", lid);
            break;
        } else if (!pid) {
            if (execute_child(lid)) {
                fprintf(stderr, "Failed to execute child: local_id=%d\n", lid);
                close(pd_log);
                close(evt_log);
                return 1;
            }
            close(pd_log);
            close(evt_log);
            return 0;
        }
    }

    if (lid > processes_count) {
        result = execute_parent();
    } else {
        fprintf(stderr, "Error occurred while forking processes!\n");
        result = -1;
    }

    close(pd_log);
    close(evt_log);

    for (int i = 1; i < lid; ++i) {
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

    return result;
}

void log_pipe(const char *fmt, ...) {
    va_list args;
    char buffer[1024];

    va_start(args, fmt);
    vsprintf(buffer, fmt, args);
    write(pd_log, buffer, strlen(buffer));
    va_end(args);
}

void log_event(const char *message) {
    write(evt_log, message, strlen(message));
    printf("%s", message);
}

int execute_child(local_id id) {
    ProcessInfo process_info;

    process_info.id = id;
    process_info.processes_count = processes_count;

    if (prepare_pipes(&process_info, pipes_descriptors)) {
        fprintf(stderr, "Failed to prepare child pipes: local_id=%d\n", id);
        close_pipes(processes_count, pipes_descriptors);
        return 1;
    }

    if (child_phase_1(&process_info)) {
        fprintf(stderr, "Failed to execute first child phase: local_id=%d\n", id);
        cleanup_pipes(&process_info);
        return 2;
    }
    if (child_phase_2(&process_info)) {
        fprintf(stderr, "Failed to execute second child phase: local_id=%d\n", id);
        cleanup_pipes(&process_info);
        return 3;
    }
    if (child_phase_3(&process_info)) {
        fprintf(stderr, "Failed to execute third child phase: local_id=%d\n", id);
        cleanup_pipes(&process_info);
        return 4;
    }

    cleanup_pipes(&process_info);

    return 0;
}

int execute_parent() {
    ProcessInfo process_info;

    process_info.id = PARENT_ID;
    process_info.processes_count = processes_count;

    if (prepare_pipes(&process_info, pipes_descriptors)) {
        fprintf(stderr, "Failed to prepare parent pipes\n");
        close_pipes(processes_count, pipes_descriptors);
        return 1;
    }

    if (parent_phase_1(&process_info)) {
        fprintf(stderr, "Failed to execute first parent phase\n");
        cleanup_pipes(&process_info);
        return 2;
    }
    if (parent_phase_2(&process_info)) {
        fprintf(stderr, "Failed to execute second parent phase\n");
        cleanup_pipes(&process_info);
        return 3;
    }
    if (parent_phase_3(&process_info)) {
        fprintf(stderr, "Failed to execute third parent phase\n");
        cleanup_pipes(&process_info);
        return 4;
    }

    cleanup_pipes(&process_info);

    return 0;
}
