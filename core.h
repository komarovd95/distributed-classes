#ifndef PA1_CORE_H
#define PA1_CORE_H

#include "ipc.h"

#define TOTAL_PROCESSES (MAX_PROCESS_ID + 1)

/**
 * A state of current process.
 */
typedef struct {
    local_id id;                            ///< Local process identifier
    long     processes_count;               ///< Total count of processes excluding parent
    int      reading_pipes[MAX_PROCESS_ID]; ///< Read endpoints of previously created pipes
    int      writing_pipes[MAX_PROCESS_ID]; ///< Write endpoints of previously created pipes
    int      evt_log;                       ///< Events log file descriptor
    int      pd_log;                        ///< Pipes events log file descriptor
} ProcessState;

/**
 * Logs occurred event to log file.
 *
 * @param state a state of current process
 * @param message message to log
 */
void log_event(ProcessState *state, const char *message);

/**
 * Logs pipe event, e.g. closing or creating new pipe.
 *
 * @param fmt format of log message
 * @param ... message parameters
 */
void log_pipe(ProcessState *state, const char *fmt, ...);

#endif //PA1_CORE_H
