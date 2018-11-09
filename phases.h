#include "distributed.h"

#ifndef PA1_PHASES_H
#define PA1_PHASES_H

/**
 * Implements first phase of child process (starting synchronization).
 *
 * @param process_info a process info
 * @return 0 if success
 */
int child_phase_1(ProcessInfo *process_info);

/**
 * Implements second phase of child process (some work).
 *
 * @param process_info a process info
 * @return 0 if success
 */
int child_phase_2(ProcessInfo *process_info);

/**
 * Implements third phase of child process (done synchronization).
 *
 * @param process_info a process info
 * @return 0 if success
 */
int child_phase_3(ProcessInfo *process_info);

/**
 * Implements first phase of parent process (starting synchronization).
 *
 * @param process_info a process info
 * @return 0 if success
 */
int parent_phase_1(ProcessInfo *process_info);

/**
 * Implements second phase of parent process (some work).
 *
 * @param process_info a process info
 * @return 0 if success
 */
int parent_phase_2(ProcessInfo *process_info);

/**
 * Implements third phase of parent process (done synchronization).
 *
 * @param process_info a process info
 * @return 0 if success
 */
int parent_phase_3(ProcessInfo *process_info);

#endif //PA1_PHASES_H
