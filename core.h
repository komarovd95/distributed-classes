#ifndef PA1_CORE_H
#define PA1_CORE_H

#include "ipc.h"
#include "banking.h"
#include "queue.h"

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
    int      done_received[MAX_PROCESS_ID]; ///< Count of DONE events received
    int      use_mutex;                     ///< 1 if critical section is used
    Queue    queue;                         ///< Priority queue for CS requests
} ProcessState;

void construct_message(Message *message, int message_type, size_t payload_len, const char *payload);

local_id try_receive(ProcessState *state, Message *message);

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

void on_message_send(void);

void on_message_received(timestamp_t message_time);

int timestamp_compare(const CsRequest *a, const CsRequest *b);

#endif //PA1_CORE_H
