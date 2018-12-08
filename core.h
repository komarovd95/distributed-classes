#ifndef PA1_CORE_H
#define PA1_CORE_H

#include "ipc.h"
#include "banking.h"

#define TOTAL_PROCESSES (MAX_PROCESS_ID + 1)

/**
 * A state of current process.
 */
typedef struct {
    local_id       id;                            ///< Local process identifier
    long           processes_count;               ///< Total count of processes excluding parent
    int            reading_pipes[MAX_PROCESS_ID]; ///< Read endpoints of previously created pipes
    int            writing_pipes[MAX_PROCESS_ID]; ///< Write endpoints of previously created pipes
    int            evt_log;                       ///< Events log file descriptor
    int            pd_log;                        ///< Pipes events log file descriptor
    int            done_received;                 ///< Count of DONE events received
    timestamp_t    vector_time[TOTAL_PROCESSES];  ///< Vector time
    balance_t      balance;                       ///< Current balance
} ProcessState;

/**
 * Constructs message to send.
 *
 * @param state a state of current process
 * @param message a pointer to message that will be constructed
 * @param message_type a type of message
 * @param payload_len a length of message payload
 * @param payload a pointer to message payload. May be null when payload_len parameter equals 0
 */
void construct_message(const ProcessState *state, Message *message, int message_type, size_t payload_len, const char *payload);

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

/**
 * Process message send event: increment clock of current process by 1.
 *
 * @param state a state of current process
 */
void on_message_send(ProcessState *state);

/**
 * Process message receive event: update vector clock.
 *
 * @param state a state of current process
 * @param message a message has been received
 */
void on_message_received(ProcessState *state, const Message *message);

/**
 * Calculates total balance.
 *
 * @param state a state of current process
 * @return 0 if success
 */
int calculate_total_balance(ProcessState *state);

#endif //PA1_CORE_H
