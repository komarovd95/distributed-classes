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
    balance_t      balance;                       ///< Current balance
    BalanceHistory history;                       ///< History of balance
} ProcessState;

/**
 * Constructs message to send.
 *
 * @param message a pointer to message that will be constructed
 * @param message_type a type of message
 * @param payload_len a length of message payload
 * @param payload a pointer to message payload. May be null when payload_len parameter equals 0
 */
void construct_message(Message *message, int message_type, size_t payload_len, const char *payload);

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
 * Process message send event: increment scalar clock by 1.
 */
void on_message_send(void);

/**
 * Process message receive event: update scalar clock.
 */
void on_message_received(const Message *message);

#endif //PA1_CORE_H
