#include <stdio.h>
#include "phases.h"

int child_phase_1(ProcessInfo *process_info) {
    if (broadcast_started(process_info)) {
        fprintf(stderr, "(%d) Failed to broadcast STARTED event!\n", process_info->id);
        return 1;
    }
    if (receive_started(process_info)) {
        fprintf(stderr, "(%d) Failed to receive all STARTED events!\n", process_info->id);
        return 2;
    }
    return 0;
}

int child_phase_2(ProcessInfo *process_info) {
    return 0;
}

int child_phase_3(ProcessInfo *process_info) {
    if (broadcast_done(process_info)) {
        fprintf(stderr, "(%d) Failed to broadcast DONE event!\n", process_info->id);
        return 1;
    }
    if (receive_done(process_info)) {
        fprintf(stderr, "(%d) Failed to receive all DONE events!\n", process_info->id);
        return 2;
    }
    return 0;
}

int parent_phase_1(ProcessInfo *process_info) {
    if (receive_started(process_info)) {
        fprintf(stderr, "(%d) Failed to receive all STARTED events!\n", process_info->id);
        return 1;
    }
    return 0;
}

int parent_phase_2(ProcessInfo *process_info) {
    return 0;
}

int parent_phase_3(ProcessInfo *process_info) {
    if (receive_done(process_info)) {
        fprintf(stderr, "(%d) Failed to receive all DONE events!\n", process_info->id);
        return 1;
    }
    return 0;
}
