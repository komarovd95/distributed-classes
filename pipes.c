#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "pipes.h"
#include "logging.h"

int init_pipes(long processes_count, int pipes_descriptors[TOTAL_PROCESSES][TOTAL_PROCESSES * 2]) {
    int i, j;

    for (i = 0; i <= processes_count; ++i) {
        for (j = 0; j <= processes_count; ++j) {
            if (i != j) {
                if (pipe(&pipes_descriptors[i][j * 2]) == -1) {
                    log_pipe("Failed to initialize pipe: from=%d to=%d error=%s\n", i, j, strerror(errno));
                    return 1;
                }
                log_pipe("Create pipe: from=%d to=%d descriptors=[%d, %d]\n",
                        i, j, pipes_descriptors[i][j * 2], pipes_descriptors[i][j * 2 + 1]);
            }
        }
    }

    return 0;
}

void close_pipes(long processes_count, int pipes_descriptors[TOTAL_PROCESSES][TOTAL_PROCESSES * 2]) {
    int i, j;

    for (i = 0; i <= processes_count; ++i) {
        for (j = 0; j <= processes_count; ++j) {
            if (i != j) {
                log_pipe("Close pipe read and write descriptors: from=%d to=%d descriptors=[%d, %d]\n",
                        i, j, pipes_descriptors[i][j * 2], pipes_descriptors[i][j * 2 + 1]);
                close(pipes_descriptors[i][j * 2]);
                close(pipes_descriptors[i][j * 2 + 1]);
            }
        }
    }
}

int prepare_pipes(ProcessInfo *process_info, int pipes_descriptors[TOTAL_PROCESSES][TOTAL_PROCESSES * 2]) {
    int i, j;
    local_id id;
    long processes_count;

    id = process_info->id;
    processes_count = process_info->processes_count;

    for (i = 0; i <= processes_count; ++i) {
        if (i != id) {
            process_info->reading_pipes[i] = pipes_descriptors[i][id * 2];
            log_pipe("(%d) Close unused pipe write endpoint: from=%d to=%d descriptor=%d\n",
                     id, i, id, pipes_descriptors[i][id * 2 + 1]);
            close(pipes_descriptors[i][id * 2 + 1]);

            process_info->writing_pipes[i] = pipes_descriptors[id][i * 2 + 1];
            log_pipe("(%d) Close unused pipe read endpoint: from=%d to=%d descriptor=%d\n",
                     id, id, i, pipes_descriptors[id][i * 2]);
            close(pipes_descriptors[id][i * 2]);

            for (j = 0; j <= process_info->processes_count; ++j) {
                if (i != j && j != id) {
                    log_pipe("(%d) Close unused pipe: from=%d to=%d descriptors=[%d, %d]\n",
                             id, i, j, pipes_descriptors[i][j * 2], pipes_descriptors[i][j * 2 + 1]);
                    close(pipes_descriptors[i][j * 2]);
                    close(pipes_descriptors[i][j * 2 + 1]);
                }
            }
        }
    }

    return 0;
}

void cleanup_pipes(ProcessInfo *process_info) {
    int i;
    local_id id;

    id = process_info->id;
    for (i = 0; i <= process_info->processes_count; ++i) {
        if (i != id) {
            log_pipe("(%d) Close pipe write endpoint: from=%d to=%d descriptor=%d\n",
                    id, i, id, process_info->writing_pipes[i]);
            close(process_info->writing_pipes[i]);

            log_pipe("(%d) Close pipe read endpoint: from=%d to=%d descriptor=%d\n",
                     id, id, i, process_info->reading_pipes[i]);
            close(process_info->reading_pipes[i]);
        }
    }
}
