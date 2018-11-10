#include "distributed.h"
#include "core.h"

#ifndef PA1_PIPES_H
#define PA1_PIPES_H

/**
 * Initialize pipes descriptors. In position pipes_descriptors[i][j * 2] read
 * endpoint of pipe between i and j process and write endpoint in position
 * pipes_descriptors[i][j * 2 + 1].
 *
 * @param processes_count a count of child processes
 * @param pipes_descriptors matrix for pipes descriptors
 * @return 0 if success
 */
int init_pipes(ProcessState *state, int pipes_descriptors[TOTAL_PROCESSES][TOTAL_PROCESSES * 2]);

/**
 * Closes all opened pipes descriptors.
 *
 * @param processes_count a count of child processes
 * @param pipes_descriptors matrix of pipes descriptors
 */
void close_pipes(ProcessState *state, int pipes_descriptors[TOTAL_PROCESSES][TOTAL_PROCESSES * 2]);

/**
 * Initializes process info reading and writing pipes descriptors and closes unused.
 *
 * @param state a state of current process
 * @param pipes_descriptors pipes descriptors
 * @return 0 if success
 */
int prepare_pipes(ProcessState *state, int pipes_descriptors[TOTAL_PROCESSES][TOTAL_PROCESSES * 2]);

/**
 * Closes all opened pipes descriptors for child process.
 *
 * @param state a state of current process
 */
void cleanup_pipes(ProcessState *state);

#endif //PA1_PIPES_H
