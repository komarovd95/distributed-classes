#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include "pipes.h"

int init_pipes(ProcessState *state, int pipes_descriptors[TOTAL_PROCESSES][TOTAL_PROCESSES * 2]) {
    int i, j;

    for (i = 0; i <= state->processes_count; ++i) {
        for (j = 0; j <= state->processes_count; ++j) {
            if (i != j) {
                if (pipe(&pipes_descriptors[i][j * 2]) == -1) {
                    log_pipe(state, "Failed to initialize pipe: from=%d to=%d error=%s\n", i, j, strerror(errno));
                    return 1;
                }
                log_pipe(state, "Create pipe: from=%d to=%d descriptors=[%d, %d]\n",
                        i, j, pipes_descriptors[i][j * 2], pipes_descriptors[i][j * 2 + 1]);

                if (fcntl(pipes_descriptors[i][j * 2], F_SETFL, O_NONBLOCK) < 0) {
                    fprintf(stderr, "Failed to set non-blocking mode for read end of pipe: error=%s\n", strerror(errno));
                    return 2;
                }
                if (fcntl(pipes_descriptors[i][j * 2 + 1], F_SETFL, O_NONBLOCK) < 0) {
                    fprintf(stderr, "Failed to set non-blocking mode for write end of pipe: error=%s\n", strerror(errno));
                    return 3;
                }
            }
        }
    }

    return 0;
}

void close_pipes(ProcessState *state, int pipes_descriptors[TOTAL_PROCESSES][TOTAL_PROCESSES * 2]) {
    int i, j;

    for (i = 0; i <= state->processes_count; ++i) {
        for (j = 0; j <= state->processes_count; ++j) {
            if (i != j) {
                log_pipe(state, "Close pipe read and write descriptors: from=%d to=%d descriptors=[%d, %d]\n",
                        i, j, pipes_descriptors[i][j * 2], pipes_descriptors[i][j * 2 + 1]);
                close(pipes_descriptors[i][j * 2]);
                close(pipes_descriptors[i][j * 2 + 1]);
            }
        }
    }
}

int prepare_pipes(ProcessState *state, int pipes_descriptors[TOTAL_PROCESSES][TOTAL_PROCESSES * 2]) {
    int i, j;
    local_id id;
    long processes_count;

    id = state->id;
    processes_count = state->processes_count;

    for (i = 0; i <= processes_count; ++i) {
        if (i != id) {
            state->reading_pipes[i] = pipes_descriptors[i][id * 2];
            log_pipe(state, "(%d) Close unused pipe write endpoint: from=%d to=%d descriptor=%d\n",
                     id, i, id, pipes_descriptors[i][id * 2 + 1]);
            close(pipes_descriptors[i][id * 2 + 1]);

            state->writing_pipes[i] = pipes_descriptors[id][i * 2 + 1];
            log_pipe(state, "(%d) Close unused pipe read endpoint: from=%d to=%d descriptor=%d\n",
                     id, id, i, pipes_descriptors[id][i * 2]);
            close(pipes_descriptors[id][i * 2]);

            for (j = 0; j <= state->processes_count; ++j) {
                if (i != j && j != id) {
                    log_pipe(state, "(%d) Close unused pipe: from=%d to=%d descriptors=[%d, %d]\n",
                             id, i, j, pipes_descriptors[i][j * 2], pipes_descriptors[i][j * 2 + 1]);
                    close(pipes_descriptors[i][j * 2]);
                    close(pipes_descriptors[i][j * 2 + 1]);
                }
            }
        }
    }

    return 0;
}

void cleanup_pipes(ProcessState *state) {
    int i;
    local_id id;

    id = state->id;
    for (i = 0; i <= state->processes_count; ++i) {
        if (i != id) {
            log_pipe(state, "(%d) Close pipe write endpoint: from=%d to=%d descriptor=%d\n",
                    id, i, id, state->writing_pipes[i]);
            close(state->writing_pipes[i]);

            log_pipe(state, "(%d) Close pipe read endpoint: from=%d to=%d descriptor=%d\n",
                     id, id, i, state->reading_pipes[i]);
            close(state->reading_pipes[i]);
        }
    }
}
