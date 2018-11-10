#ifndef PA1_DISTRIBUTED_H
#define PA1_DISTRIBUTED_H

#include "ipc.h"

#define TOTAL_PROCESSES (MAX_PROCESS_ID + 1)

/**
 * Struct that describes an executing process.
 */
typedef struct {
    local_id id;                            ///< Local identifier of child process.
    long     processes_count;               ///< Total count of child processes.
    int      reading_pipes[MAX_PROCESS_ID]; ///< Reading pipes descriptors.
    int      writing_pipes[MAX_PROCESS_ID]; ///< Writing pipes descriptors.
} ProcessInfo;

/**
 * Broadcasts STARTED event to all other processes.
 *
 * @param process_info a process info
 * @return 0 if success
 */
int broadcast_started(ProcessInfo *process_info);

/**
 * Receives STARTED event from all other processes.
 *
 * @param process_info a process info
 * @return 0 if success
 */
int receive_started(ProcessInfo *process_info);

/**
 * Broadcasts DONE event to all other processes.
 *
 * @param process_info a process info
 * @return 0 if success
 */
int broadcast_done(ProcessInfo *process_info);

/**
 * Receives DONE event from all other processes.
 *
 * @param process_info a process info
 * @return 0 if success
 */
int receive_done(ProcessInfo *process_info);

#endif //PA1_DISTRIBUTED_H
