#include "distributed.h"
#include "core.h"

#ifndef PA1_PHASES_H
#define PA1_PHASES_H

/**
 * Implements first phase of child process (starting synchronization).
 *
 * @param state a state of current process
 * @return 0 if success
 */
int child_phase_1(ProcessState *state);

/**
 * Implements second phase of child process (some work).
 *
 * @param state a state of current process
 * @return 0 if success
 */
int child_phase_2(ProcessState *state);

/**
 * Implements third phase of child process (done synchronization).
 *
 * @param state a state of current process
 * @return 0 if success
 */
int child_phase_3(ProcessState *state);

/**
 * Implements first phase of parent process (starting synchronization).
 *
 * @param state a state of current process
 * @return 0 if success
 */
int parent_phase_1(ProcessState *state);

/**
 * Implements second phase of parent process (some work).
 *
 * @param state a state of current process
 * @return 0 if success
 */
int parent_phase_2(ProcessState *state);

/**
 * Implements third phase of parent process (done synchronization).
 *
 * @param state a state of current process
 * @return 0 if success
 */
int parent_phase_3(ProcessState *state);

#endif //PA1_PHASES_H
