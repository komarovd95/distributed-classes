#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdarg.h>
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
FILE *evt_log, *pd_log;

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

    pd_log = fopen(pipes_log, "a");
    if (!pd_log) {
        fprintf(stderr, "Failed to open pipes log!\n");
        return 3;
    }

    if (init_pipes(processes_count, pipes_descriptors)) {
        fprintf(stderr, "Failed to initialize pipes descriptors!\n");
        return 4;
    }

    evt_log = fopen(events_log, "a");
    if (!evt_log) {
        fprintf(stderr, "Failed to open events log!\n");
        cleanup_pipes(processes_count, pipes_descriptors);
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
                fclose(pd_log);
                fclose(evt_log);
                return 1;
            }
            fclose(pd_log);
            fclose(evt_log);
            return 0;
        }
    }

    if (lid > processes_count) {
        result = execute_parent();
    } else {
        fprintf(stderr, "Error occurred while forking processes!\n");
        result = -1;
    }

    fclose(pd_log);
    fclose(evt_log);

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

    va_start(args, fmt);
    vfprintf(pd_log, fmt, args);
    fflush(pd_log);
    va_end(args);
}

void log_event(const char *message) {
    fprintf(evt_log, "%s", message);
    fflush(evt_log);

    printf("%s", message);
    fflush(stdout);
}

int execute_child(local_id id) {
    ProcessInfo process_info;

    process_info.id = id;
    process_info.processes_count = processes_count;

    if (prepare_child_pipes(&process_info, pipes_descriptors)) {
        fprintf(stderr, "Failed to prepare child pipes: local_id=%d\n", id);
        cleanup_pipes(processes_count, pipes_descriptors);
        return 1;
    }

    if (child_phase_1(&process_info)) {
        fprintf(stderr, "Failed to execute first child phase: local_id=%d\n", id);
        cleanup_child_pipes(&process_info);
        return 2;
    }
    if (child_phase_2(&process_info)) {
        fprintf(stderr, "Failed to execute second child phase: local_id=%d\n", id);
        cleanup_child_pipes(&process_info);
        return 3;
    }
    if (child_phase_3(&process_info)) {
        fprintf(stderr, "Failed to execute third child phase: local_id=%d\n", id);
        cleanup_child_pipes(&process_info);
        return 4;
    }

    cleanup_child_pipes(&process_info);

    return 0;
}

int execute_parent() {
    ProcessInfo process_info;

    process_info.id = PARENT_ID;
    process_info.processes_count = processes_count;

    if (prepare_parent_pipes(&process_info, pipes_descriptors)) {
        fprintf(stderr, "Failed to prepare parent pipes\n");
        cleanup_pipes(processes_count, pipes_descriptors);
        return 1;
    }

    if (parent_phase_1(&process_info)) {
        fprintf(stderr, "Failed to execute first parent phase\n");
        cleanup_parent_pipes(&process_info);
        return 2;
    }
    if (parent_phase_2(&process_info)) {
        fprintf(stderr, "Failed to execute second parent phase\n");
        cleanup_parent_pipes(&process_info);
        return 3;
    }
    if (parent_phase_3(&process_info)) {
        fprintf(stderr, "Failed to execute third parent phase\n");
        cleanup_parent_pipes(&process_info);
        return 4;
    }

    cleanup_parent_pipes(&process_info);

    return 0;
}
