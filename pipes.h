#include "distributed.h"

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
int init_pipes(long processes_count, int pipes_descriptors[TOTAL_PROCESSES][TOTAL_PROCESSES * 2]);

/**
 * Closes all opened pipes descriptors.
 *
 * @param processes_count a count of child processes
 * @param pipes_descriptors matrix of pipes descriptors
 */
void close_pipes(long processes_count, int pipes_descriptors[TOTAL_PROCESSES][TOTAL_PROCESSES * 2]);

/**
 * Initializes process info reading and writing pipes descriptors and closes unused.
 *
 * @param process_info a process info
 * @param pipes_descriptors pipes descriptors
 * @return 0 if success
 */
int prepare_pipes(ProcessInfo *process_info, int pipes_descriptors[TOTAL_PROCESSES][TOTAL_PROCESSES * 2]);

/**
 * Closes all opened pipes descriptors for child process.
 *
 * @param process_info a process info
 */
void cleanup_pipes(ProcessInfo *process_info);

#endif //PA1_PIPES_H
