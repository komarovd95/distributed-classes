#include "ipc.h"

/**
 * A struct that describes configured pipes for current process.
 */
typedef struct {
    int         processes_count;             ///< Total count of processes.
    local_id    local_id;                    ///< A local ID of current process.
    int         read_pipes[MAX_PROCESS_ID];  ///< Pipe descriptors to read from.
    int         write_pipes[MAX_PROCESS_ID]; ///< Pipe descriptors to write to.
} ProcessPipe;

/**
 * A struct that must be used while receiving messages.
 */
typedef struct {
    ProcessPipe pipe;                         ///< A pipe of current process.
    local_id    sender;                       ///< A local ID of sender process.
    int         receive_from[MAX_PROCESS_ID]; ///< An array of flags where 1 in
                                              ///< position i means that
                                              ///< receiving message from sender
                                              ///< i is available.
    Message     message;                      ///< Received message.
} ReceiveMsg;

/**
 * A struct that must be used while sending messages.
 */
typedef struct {
    ProcessPipe pipe;                     ///< A pipe of current process.
    local_id    receiver;                 ///< A receiver process local ID.
    size_t      payload_len;              ///< A length of payload in bytes.
    char        payload[MAX_PAYLOAD_LEN]; ///< A payload.
    int         message_type;             ///< A type of sending message.
} SendMsg;

/**
 * Receives message from concrete sender. To qualify sender 'sender' field is
 * used. The 'receive_from' field is ignored.
 *
 * @param receive_msg helper struct for receiving message
 *
 * @return 0 on success, any non-zero value on error
 */
int receive_from(ReceiveMsg *receive_msg);

/**
 * Receives message from any sender. The 'receive_from' field is used to
 * determine message receiving availability.
 *
 * @param receive_msg helper struct for receiving message
 *
 * @return 0 on success, any non-zero value on error
 */
int receive_from_any(ReceiveMsg *receive_msg);

/**
 * Sends given message to concrete process. To qualify receiver 'receiver' field
 * is used.
 *
 * @param send_msg helper struct for receiving message.
 *
 * @return 0 on success, any non-zero value on error
 */
int send_to(SendMsg *send_msg);

/**
 * Sends given message to all available receivers. The 'receiver' field is
 * ignored.
 *
 * @param send_msg helper struct for receiving message.
 *
 * @return 0 on success, any non-zero value on error
 */
int send_to_any(SendMsg *send_msg);

/**
 * Creates pipes descriptors. Read endpoint for pipe from i-th process to j-th
 * process is identified by pipes_descriptors[i][j * 2]. Write endpoint by
 * pipes_descriptors[i][j * 2 + 1].
 *
 * @param processes_count   count of processes
 * @param pipes_descriptors pipes descriptors that will be created
 * @param log               a log file
 *
 * @return 0 on success, any non-zero value on error
 */
int create_pipes(int processes_count, int **pipes_descriptors, FILE *log);

/**
 * Filling the 'read_pipes' and 'write_pipes' arrays with real descriptors and
 * closing unused descriptors. RW means Read-Write mode.
 *
 * @param pipe              a pipe of current process
 * @param pipes_descriptors pipes descriptors
 * @param log               a log file
 *
 * @return 0 on success, any non-zero value on error
 */
int prepare_rw_pipes(ProcessPipe *pipe, int **pipes_descriptors, FILE *log);

/**
 * Filling the 'read_pipes' array with real descriptors and closing unused
 * descriptors. RO means Read-Only mode.
 *
 * @param pipe              a pipe of current process
 * @param pipes_descriptors pipes descriptors
 * @param log               a log file
 *
 * @return 0 on success, any non-zero value on error
 */
int prepare_ro_pipes(ProcessPipe *pipe, int **pipes_descriptors, FILE *log);

/**
 * Cleans up the 'read_pipes' and 'write_pipes' arrays (closing descriptors).
 *
 * @param pipe a pipe of current process
 * @param log  a log file
 *
 * @return 0 on success, any non-zero value on error
 */
int cleanup_rw_pipe(ProcessPipe *pipe, FILE *log);

/**
 * Cleans up the 'read_pipes' array (closing descriptors).
 *
 * @param pipe a pipe of current process
 * @param log  a log file
 *
 * @return 0 on success, any non-zero value on error
 */
int cleanup_ro_pipe(ProcessPipe *pipe, FILE *log);

/**
 * Logs an event.
 *
 * @param log a log file
 * @param fmt a format string
 */
void log_event(FILE *log, const char *fmt, ...);
